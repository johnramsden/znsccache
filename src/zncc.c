#include "znsccache.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gcrypt.h>
#include <getopt.h>
#include <inttypes.h>
#include <libzbd/zbd.h>
#include <stdio.h>
#include <stdlib.h>

void
print_bucket(zncc_bucket_list *bucket, uint32_t b_num) {
    zncc_bucket_node *b = bucket->head;
    while (1) {
        if (b == NULL) {
            break;
        }
        printf("[b=%u] (zone=%u,chunk=%u,uuid=%s)\n", b_num, b->data.zone, b->data.chunk,
               b->data.uuid);
        b = b->next;
    }
}

void
print_free_list(zncc_bucket_list *bucket) {
    printf("Free List\n");
    zncc_bucket_node *b = bucket->head;
    while (1) {
        if (b == NULL) {
            break;
        }
        printf("(zone=%u,chunk=%u)\n", b->data.zone, b->data.chunk);
        b = b->next;
    }
}

typedef struct __uuid_intermediate {
    uint32_t uuid_hash;
    off_t offset;
} __uuid_intermediate;

/**
 * @brief Cleanup resources
 *
 * @param cc  Chunk cache
 */
void
zncc_destroy(zncc_chunkcache *cc) {
    for (int i = 0; i < cc->chunks_total; i++) {
        zncc_bucket_destroy_list(&cc->buckets[i]);
    }
    free(cc->buckets);
    zncc_bucket_destroy_list(&cc->free_list);
    free(cc->allocated);
}

/**
 * @brief Read a chunk from a zone
 *
 * @param cc         chunk cache
 * @param chunk_info chunk location to read from to
 * @param data       Data to write
 * @return int       Non-zero on error
 */
int
zncc_read_chunk(zncc_chunkcache *cc, zncc_chunk_info chunk_info, char **data) {
    struct zbd_info info;

    int ret = 0;

    int fd = zbd_open(cc->device, O_RDWR, &info);
    if (fd < 0) {
        fprintf(stderr, "Error opening device: %s\n", cc->device);
        return fd;
    }

    // If len is 0, at most nr_zones zones starting from ofst up to the end on the device
    // capacity will be reported.
    off_t ofst = 0;
    off_t len = 0;
    size_t sz = info.nr_zones * (sizeof(struct zbd_zone));
    struct zbd_zone *zones = malloc(sz);
    if (zones == NULL) {
        fprintf(stderr, "Couldn't allocate %lu bytes for zones\n", sz);
        return -1;
    }
    unsigned int nr_zones = info.nr_zones;
    ret = zbd_report_zones(fd, ofst, len, ZBD_RO_ALL, zones, &nr_zones);
    if (ret != 0) {
        fprintf(stderr, "Couldn't report zone info\n");
        return ret;
    }

    unsigned long long wp =
        CHUNK_POINTER(cc->zone_size, cc->chunk_size, chunk_info.chunk, chunk_info.zone);

    dbg_printf("zone %u write pointer: %llu\n", chunk_info.zone, wp);

    // ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

    // write to fd at offset 0

    *data = malloc(cc->chunk_size);
    if (*data == NULL) {
        ret = -1;
        goto cleanup;
    }
    size_t b = pread(fd, *data, cc->chunk_size, wp);
    if (b != cc->chunk_size) {
        fprintf(stderr, "Couldn't read from fd\n");
        free(*data);
        ret = -1;
        goto cleanup;
    }

    // (*data)[512] = '\0';

    // dbg_printf("Read %s from fd\n", *data);

cleanup:
    zbd_close(fd);
    free(zones);
    return ret;
}

/**
 * @brief Write a chunk to a zone
 *
 * @param cc         chunk cache
 * @param chunk_info chunk location to write to
 * @param data       Data to write
 * @return int       Non-zero on error
 */
int
zncc_write_chunk(zncc_chunkcache *cc, zncc_chunk_info chunk_info, char *data) {
    struct zbd_info info;

    int ret = 0;

    int fd = zbd_open(cc->device, O_RDWR, &info);
    if (fd < 0) {
        fprintf(stderr, "Error opening device: %s\n", cc->device);
        return fd;
    }

    // If len is 0, at most nr_zones zones starting from ofst up to the end on the device
    // capacity will be reported.
    off_t ofst = 0;
    off_t len = 0;
    size_t sz = info.nr_zones * (sizeof(struct zbd_zone));
    struct zbd_zone *zones = malloc(sz);
    if (zones == NULL) {
        fprintf(stderr, "Couldn't allocate %lu bytes for zones\n", sz);
        return -1;
    }
    unsigned int nr_zones = info.nr_zones;
    ret = zbd_report_zones(fd, ofst, len, ZBD_RO_ALL, zones, &nr_zones);
    if (ret != 0) {
        fprintf(stderr, "Couldn't report zone info\n");
        return ret;
    }

    dbg_printf("zone %u write pointer: %llu\n", chunk_info.zone, zones[chunk_info.zone].wp);

    // ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

    // write to fd at offset 0

    ssize_t b = pwrite(fd, data, cc->chunk_size, zones[chunk_info.zone].wp);
    if (b != cc->chunk_size) {
        fprintf(stderr, "Couldn't write to fd\n");
        ret = -1;
        goto cleanup;
    }

    assert(CHUNK_POINTER(cc->zone_size, cc->chunk_size, chunk_info.chunk, chunk_info.zone) ==
           zones[chunk_info.zone].wp);

cleanup:
    zbd_close(fd);
    free(zones);
    return ret;
}

/**
 * @brief Take a CRC32 hash of a string
 *
 * @param[in]  hashable String to be hashed
 * @param[out] out      Hash of hashable
 * @return uint32_t     Non-zero on error
 */
static uint32_t
crc32_hash(char const *const hashable, uint32_t len, uint32_t *out) {
    gcry_error_t err;
    gcry_md_hd_t hd;
    unsigned char *hash;

    dbg_printf("Hashing: %s\n", hashable);

    if (!gcry_check_version(GCRYPT_VERSION)) {
        dbg_printf("libgcrypt version mismatch\n");
        return 1;
    }

    // Disable secure memory (not needed for hashing)
    gcry_control(GCRYCTL_DISABLE_SECMEM, 0);

    // Initialize
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

    // Open a hash handle
    err = gcry_md_open(&hd, GCRY_MD_CRC32, 0);
    if (err) {
        dbg_printf("gcry_md_open failed: %s\n", gpg_strerror(err));
        return 1;
    }

    // Hash string
    gcry_md_write(hd, hashable, len);

    // Get hash
    hash = gcry_md_read(hd, GCRY_MD_CRC32);
    if (!hash) {
        dbg_printf("gcry_md_read failed\n");
        gcry_md_close(hd);
        return 1;
    }

    // Print hash
    dbg_printf("Hash: ");
    for (int i = 0; i < gcry_md_get_algo_dlen(GCRY_MD_CRC32); i++) {
        dbg_printf("%02x", hash[i]);
    }

    *out = ((unsigned int) hash[0] << 24) | ((unsigned int) hash[1] << 16) |
           ((unsigned int) hash[2] << 8) | (unsigned int) hash[3];

    dbg_printf("\n");

    dbg_printf("Hashed int: %u\n", *out);

    gcry_md_close(hd);

    return 0;
}

/**
 * @brief Find a "bucket" for a given UUID based on total chunks on disk
 *
 * @param uuid       Chunk unique ID
 * @param num_chunks Number of chunks total on disk
 * @return uint32_t     Bucket number (0 to num_chunks-1)
 */
static uint32_t
find_bucket(uint32_t hash, uint32_t num_chunks) {
    return hash % num_chunks;
}

static int
setup_intermediate_uuid(__uuid_intermediate *intermediate_uuid, char const *const uuid,
                        off_t offset) {
    uint32_t hash;
    int ret = crc32_hash(uuid, strlen(uuid), &intermediate_uuid->uuid_hash);
    if (ret != 0) {
        return ret;
    }

    intermediate_uuid->offset = offset;

    return ret;
}

/**
 * @brief Place an item in the cache
 *
 * @param cc    Chunk cache instance
 * @param uuid  Chunk unique ID
 * @param data  Buffer containing data
 * @return int  Non-zero on error
 */
static int
zncc_put_in_bucket(zncc_chunkcache *cc, uint32_t bucket, char const *const uuid, off_t offset,
                   uint32_t valid_size, char *data) {
    int ret = 0;
    // Get free chunk
    zncc_chunk_info zi;
    ret = zncc_bucket_pop_back(&cc->free_list, &zi);
    if (ret != 0) {
        dbg_printf("Cache full, length=%u\n", cc->free_list.length);
        // TODO: Evict
        return -1;
    }
    dbg_printf("Found free [zone,chunk]=[%u,%u]\n", zi.zone, zi.chunk);

    strcpy(zi.uuid, uuid);
    zi.offset = offset;
    zi.valid_size = valid_size;

    ret = zncc_write_chunk(cc, zi, data);
    if (ret != 0) {
        return ret;
    }

    // Add to bucket
    zncc_bucket_push_front(&cc->buckets[bucket], zi);

    // Set allocated
    cc->allocated[ABSOLUTE_CHUNK(cc->chunks_per_zone, zi.zone, zi.chunk)] = 1;

    // Add next chunk in zone to free list
    if (zi.chunk < (cc->chunks_per_zone - 1)) {
        zncc_bucket_push_front(&cc->free_list,
                               (zncc_chunk_info){.chunk = zi.chunk + 1, .zone = zi.zone});
        print_free_list(&cc->free_list);
    }

    return 0;
}

/**
 * @brief
 *
 * @param cc
 * @param uuid   UUID string (MAX 37 bytes with \0)
 * @param offset
 * @param size
 * @param data
 * @return int
 */
int
zncc_get(zncc_chunkcache *cc, char const *const uuid, off_t offset, uint32_t size, char **data) {

    int ret = 0;
    uint32_t bucket;

    dbg_printf("Get: uuid=%s\n", uuid);

    __uuid_intermediate intermediate_uuid;

    ret = setup_intermediate_uuid(&intermediate_uuid, uuid, offset);

    uint32_t hash;
    ret = crc32_hash((char *) &intermediate_uuid, sizeof(intermediate_uuid), &hash);
    if (ret != 0) {
        return ret;
    }

    bucket = find_bucket(hash, cc->chunks_total);

    dbg_printf("Found bucket: %u\n", bucket);

    zncc_chunk_info data_out;

    if (zncc_bucket_peek_by_uuid(&cc->buckets[bucket], uuid, offset, &data_out) != 0) {
        ret = zncc_s3_get(cc->s3, uuid, offset, size);
        if (ret != 0) {
            dbg_printf("S3 get failed: uuid=%s, bucket=%u\n", uuid, bucket);
            return ret;
        }
        ret = zncc_put_in_bucket(cc, bucket, uuid, offset, size, cc->s3->callback_data.buffer);
        if (ret != 0) {
            dbg_printf("Failed bucket put: uuid=%s, bucket=%u\n", uuid, bucket);
            return ret;
        }
        *data = cc->s3->callback_data.buffer;
    } else {
        dbg_printf("Read from chunk\n");
        ret = zncc_read_chunk(cc, data_out, data);
    }

    return ret;
}

/**
 * @brief Calculate number of chunks for a zone
 *
 * @param[in] chunk_size Size of chunk
 * @param[in] zone_size  Size of zone
 * @param[out] chunks_in_zone Number of chunks per zone
 * @return int Non-zero on error
 */
static int
chunks_in_zone(uint64_t chunk_size, unsigned long long zone_size, uint32_t *chunks_in_zone) {
    if (chunk_size > zone_size) {
        dbg_printf("chunk_size=%" PRIu64 " > zone_size=%llu: not supported\n", chunk_size,
                   zone_size);
        return -1;
    }

    *chunks_in_zone = zone_size / chunk_size; // Always rounds down

    return 0;
}

/**
 * @brief Fill the "free list" with zns free chunks
 *
 * @param cc   Chunk cache
 * @return int Non-zero on error
 */
static void
populate_free_list(zncc_chunkcache *cc) {
    for (int zti = 0; zti < cc->zones_total; zti++) {
        zncc_bucket_push_front(&cc->free_list, (zncc_chunk_info){.chunk = 0, .zone = zti});
    }
}

/**
 * @brief Initialize the chunk cache
 *
 * @param[out] cc        Chunk cache to initialize
 * @param[in] device     ZNS block device
 * @param[in] chunk_size Size of chunk in bytes
 * @return int           Non-zero on error
 */
int
zncc_init(zncc_chunkcache *cc, char const *const device, uint64_t chunk_size, zncc_s3 *s3) {
    int ret = 0;
    struct zbd_info info;

    int fd = zbd_open(device, O_RDWR, &info);

    if (fd < 0) {
        dbg_printf("Error opening device: %s\n", device);
        ret = fd;
        goto cleanup;
    }

    ret = zbd_reset_zones(fd, 0, 0);
    if (ret != 0) {
        fprintf(stderr, "Couldn't reset zones\n");
        goto cleanup;
    }

    if (chunks_in_zone(chunk_size, info.zone_size, &cc->chunks_per_zone) != 0) {
        ret = -1;
        goto cleanup;
    }

    cc->s3 = s3;
    cc->zone_size = info.zone_size;
    cc->device = device;
    cc->chunk_size = chunk_size;
    cc->zones_total = info.nr_zones;

    // TEST
    cc->zones_total = 2;
    // TEST FIN

    cc->chunks_total = cc->zones_total * cc->chunks_per_zone;

    uint32_t sz_allocated = cc->chunks_total * sizeof(uint32_t);
    cc->allocated = malloc(sz_allocated);
    if (cc->allocated == NULL) {
        nomem();
    }

    // Set allocated to zero
    memset(cc->allocated, 0, sz_allocated);

    cc->buckets = malloc(cc->chunks_total * sizeof(zncc_bucket_list));
    if (cc->buckets == NULL) {
        nomem();
    }

    if (cc->chunks_per_zone <= 0) {
        dbg_printf("Minimum 1 chunk per zone, found %u\n", cc->chunks_per_zone);
        ret = -1;
        goto cleanup;
    }

    for (int i = 0; i < cc->chunks_total; i++) {
        zncc_bucket_init_list(&cc->buckets[i]);
    }

    zncc_bucket_init_list(&cc->free_list);

    populate_free_list(cc);

    dbg_printf("Initialized chunk cache:\n"
               "device=%s\nchunks_per_zone=%u\nzones_total=%u\ntotal_chunks=%u\nchunks_size=%u\n",
               device, cc->chunks_per_zone, cc->zones_total, cc->chunks_total, cc->chunk_size);
cleanup:
    zbd_close(fd);
    return ret;
}
