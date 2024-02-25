#include <stdio.h>
#include <fcntl.h>
#include <getopt.h>
#include <ctype.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>

#include <libzbd/zbd.h>
#include <gcrypt.h>

#include "znsccache.h"

/**
 * @brief Cleanup resources
 *
 * @param cc  Chunk cache
 */
void zncc_destroy(zncc_chunkcache *cc) {
    for (int i = 0; i < cc->chunks_total; i++) {
        zncc_bucket_destroy_list(&cc->buckets[i]);
    }
    free(cc->buckets);
    zncc_bucket_destroy_list(&cc->free_list);
    free(cc->allocated);
}

int zncc_write_chunk(zncc_chunkcache *cc, zncc_chunk_info chunk_info, char * data) {

}

int zncc_read_chunk(zncc_chunkcache *cc, zncc_chunk_info chunk_info, char ** data) {

}

/**
 * @brief Take a CRC32 hash of a string
 *
 * @param[in]  hashable String to be hashed
 * @param[out] out      Hash of hashable
 * @return uint32_t     Non-zero on error
 */
static uint32_t crc32_hash(char const * const hashable, uint32_t *out) {
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
    gcry_md_write(hd, hashable, strlen(hashable));

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

    *out = ((unsigned int)hash[0] << 24) |
           ((unsigned int)hash[1] << 16) |
           ((unsigned int)hash[2] << 8)  |
           (unsigned int)hash[3];

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
 * @param bucket     Bucket number (0 to num_chunks-1)
 * @return uint32_t  Non-zero on error
 */
static int find_bucket(char const * const uuid, uint32_t num_chunks, uint32_t *bucket) {
    uint32_t hash;
    int ret = crc32_hash(uuid, &hash);
    if (ret != 0) {
        return ret;
    }
    *bucket = hash % num_chunks;
    return 0;
}

static void print_bucket(zncc_bucket_list *bucket, uint32_t b_num) {
    zncc_bucket_node *b = bucket->head;
    printf("Bucket=%u: ", b_num);
    while (1) {
        if (b == NULL) {
            break;
        }
        printf("(zone=%u,chunk=%u), ", b->data.zone, b->data.chunk);
        b = b->next;
    }
    printf("\n");
}

int zncc_get(zncc_chunkcache *cc, char const * const uuid, char ** data) {

    dbg_printf("Get: uuid=%s\n", uuid);
    uint32_t bucket;
    int ret = find_bucket(uuid, cc->chunks_total, &bucket);
    if (ret != 0) {
        return ret;
    }

    dbg_printf("Found bucket: %u\n", bucket);

    zncc_chunk_info data_out;

    zncc_bucket_peek_by_uuid(&cc->buckets[bucket], uuid, &data_out);

    dbg_printf("Found chunk (uuid=%s, zone=%u, chunk=%u)\n",
               data_out.uuid, data_out.zone, data_out.chunk);

    // TODO data
}

/**
 * @brief Place an item in the cache
 *
 * @param cc    Chunk cache instance
 * @param uuid  Chunk unique ID
 * @param data  Buffer containing data
 * @return int  Non-zero on error
 */
int zncc_put(zncc_chunkcache *cc, char const * const uuid, char * data) {
    uint32_t bucket;
    int ret = find_bucket(uuid, cc->chunks_total, &bucket);
    if (ret != 0) {
        return ret;
    }

    dbg_printf("Put: uuid=%s\n", uuid);
    dbg_printf("Found bucket: %u\n", bucket);

    // Get free chunk
    zncc_chunk_info zi;
    ret = zncc_bucket_pop_back(&cc->free_list, &zi);
    if (ret != 0) {
        dbg_printf("Cache full, length=%u\n", cc->free_list.length);
        // TODO
        return -1;
    }
    dbg_printf("Found free [zone,chunk]=[%u,%u]\n", zi.zone, zi.chunk);

    strcpy(zi.uuid, uuid);

    // Add to bucket
    zncc_bucket_push_front(&cc->buckets[bucket], zi);

    // TODO WRITE TO DISK!

    // Set allocated
    cc->allocated[ABSOLUTE_CHUNK(zi.zone, zi.chunk)] = 1;

    print_bucket(&cc->buckets[bucket],bucket);

    return 0;
}

/**
 * @brief Calculate number of chunks for a zone
 *
 * @param[in] chunk_size Size of chunk
 * @param[in] zone_size  Size of zone
 * @param[out] chunks_in_zone Number of chunks per zone
 * @return int Non-zero on error
 */
static int chunks_in_zone(uint64_t chunk_size, unsigned long long zone_size, uint32_t *chunks_in_zone) {
    if (chunk_size > zone_size) {
        dbg_printf("chunk_size=%" PRIu64 " > zone_size=%llu: not supported\n", chunk_size, zone_size);
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
static void populate_free_list(zncc_chunkcache *cc) {
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
int zncc_init(zncc_chunkcache *cc, char const * const device, uint64_t chunk_size) {
    struct zbd_info info;

    int fd = zbd_open(device, O_RDWR, &info);

    if (fd < 0) {
        dbg_printf("Error opening device: %s\n", device);
        return fd;
    }

    if (chunks_in_zone(chunk_size, info.zone_size, &cc->chunks_per_zone) != 0) {
        return -1;
    }

    cc->zones_total = info.nr_zones;
    cc->device = device;
    cc->chunks_total = cc->zones_total*cc->chunks_per_zone;

    uint32_t sz_allocated = cc->chunks_total *sizeof(uint32_t);
    cc->allocated = malloc(sz_allocated);
    if (cc->allocated == NULL) {
        nomem();
    }

    // Set allocated to zero
    memset(cc->allocated, 0, sz_allocated);

    cc->buckets = malloc(cc->chunks_total *sizeof(zncc_bucket_list));
    if (cc->buckets == NULL) {
        nomem();
    }

    if (cc->chunks_per_zone <= 0) {
        dbg_printf("Minimum 1 chunk per zone, found %u\n", cc->chunks_per_zone);
        return -1;
    }

    for (int i = 0; i < cc->chunks_total; i++) {
        zncc_bucket_init_list(&cc->buckets[i]);
    }

    zncc_bucket_init_list(&cc->free_list);

    populate_free_list(cc);

    dbg_printf("Initialized chunk cache:\n"
               "device=%s\nchunks_per_zone=%u\nzones_total=%u\ntotal_chunks=%u\n",
               device, cc->chunks_per_zone, cc->zones_total, cc->chunks_total );

    return 0;
}
