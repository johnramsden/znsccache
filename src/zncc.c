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
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

int evictions = 0;
int evict_errors = 0;
int hit = 0;
int accesses = 0;

struct timespec started_zncc;
#define SINCE_BEGIN(now) (TIME_DIFFERENCE_MILLISEC(started_zncc, (now)))

#define EPOCH_MAX 18446744073709551615UL
#define WRITE_GRANULARITY 4096
#define EVICT_THRESH_ZONES_LEFT 1
#define EVICT_THRESH_ZONES_REMOVE 8
#define ZONES_USED 6
#define METRIC_BUFFER_CHARS 100000

#define METRIC_HITRATE "hitrate"
#define METRIC_EVICTION "evictions"
#define METRIC_GET "get"
#define METRIC_READ "read"
#define METRIC_WRITE "write"
#define METRIC_FREE_ZONES "freezones"

char METRIC_BUFFER[METRIC_BUFFER_CHARS];

/**
 * @brief Do eviction
 *
 * @param cc   Chunk cache
 * @return int Non-zero on error
 */
static int
zncc_evict(zncc_chunkcache *cc) {
    struct zbd_info info;

    int ret = 0;
    uint32_t full_zones = 0;

    uint64_t evictable[EVICT_THRESH_ZONES_REMOVE];
    int32_t evictable_ind[EVICT_THRESH_ZONES_REMOVE] = {0};

    for (int i = 0; i < EVICT_THRESH_ZONES_REMOVE; i++) {
        evictable_ind[i] = -1;
        evictable[i] = EPOCH_MAX;
    }
    for (int i = 0; i < cc->zones_total; i++) {
        int largest_evictable_ind = 0;
        if (cc->epoch_list[i].full) {
            full_zones++;
        }
        // printf("zone=%u epoch sum=%lu.\n", i, cc->epoch_list[i].time_sum);
        // Get largest in evictable
        for (int j = 0; j < EVICT_THRESH_ZONES_REMOVE; j++) {
            if (evictable[j] > evictable[largest_evictable_ind]) {
                // printf("largest_evictable_ind=%d.\n", largest_evictable_ind);
                largest_evictable_ind = j;
            }
        }

        if ((evictable_ind[largest_evictable_ind] == -1) ||
            (cc->epoch_list[i].time_sum < evictable[largest_evictable_ind]) ) {
            // printf("Setting evictable[%d]=%lu.\n", largest_evictable_ind, cc->epoch_list[i].time_sum);
            // printf("Setting evictable_ind[%d]=%d.\n", largest_evictable_ind, i);
            evictable[largest_evictable_ind] = cc->epoch_list[i].time_sum;
            evictable_ind[largest_evictable_ind] = i;
        }
    }

    uint32_t zones_remain = cc->zones_total - full_zones;
    struct timespec fz_ts;
    TIME_NOW(&fz_ts);
    metric_printf(cc->metrics_fd, "%f,%s,%d\n", SINCE_BEGIN(fz_ts), METRIC_FREE_ZONES, zones_remain);
    if (zones_remain > EVICT_THRESH_ZONES_LEFT) {
        dbg_printf("%u zones remain, not evicting.\n", zones_remain);
        // No evict
        return 0;
    }

    struct timespec evict_ts;
    TIME_NOW(&evict_ts);
    metric_printf(cc->metrics_fd, "%f,%s,%d\n", SINCE_BEGIN(evict_ts), METRIC_EVICTION, ++evictions);

    dbg_printf("%u zones remain, evicting.\n", zones_remain);

    int fd = zbd_open(cc->device, O_RDWR, &info);
    if (fd < 0) {
        fprintf(stderr, "Error opening device: %s\n", cc->device);
        return fd;
    }

    // Set epoch
    uint64_t epoch_now = microsec_since_epoch();
    for (int i = 0; i < EVICT_THRESH_ZONES_REMOVE; i++) {
        if (evictable_ind[i] == -1) {
            evict_errors++;
            fprintf(stdout, "WARNING, evictable_ind[%d]=%d, error=%d\n", i, evictable_ind[i], evict_errors);
            continue;
        }
        dbg_printf("Evicting zone %d.\n", evictable_ind[i]);
        cc->epoch_list[evictable_ind[i]].full = false;
        cc->epoch_list[evictable_ind[i]].time_sum = epoch_now*cc->chunks_per_zone;
        for (int j = 0; j < cc->chunks_per_zone; j++) {
            cc->epoch_list[evictable_ind[i]].epoch_chunks[j].chunk_time = epoch_now;
        }

        uint64_t zone_ptr = CHUNK_POINTER(cc->zone_size, cc->chunk_size, 0, evictable_ind[i]);
        ret = zbd_reset_zones(fd, zone_ptr, 1);
        if (ret != 0) {
            fprintf(stderr, "Couldn't reset zone\n");
            goto cleanup;
        }

        // Place in free list
        zncc_bucket_push_back(&cc->free_list,
                            (zncc_chunk_info){.chunk = 0, .zone = evictable_ind[i]});

        dbg_printf("Evicted zone %u, lowest epoch sum=%lu.\n", evictable_ind[i], evictable[i]);
    }

    dbg_printf("Eviction number %d\n", ++evictions);

cleanup:
    zbd_close(fd);
    return ret;
}

void
print_bucket(zncc_bucket_list *bucket, uint32_t b_num) {
    zncc_bucket_node *b = bucket->head;
    while (1) {
        if (b == NULL) {
            break;
        }
        dbg_printf("[b=%u] (zone=%u,chunk=%u,uuid=%s)\n", b_num, b->data.zone, b->data.chunk,
               b->data.uuid);
        b = b->next;
    }
}

void
print_free_list(zncc_bucket_list *bucket) {
    dbg_printf("Free List\n");
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

    if (cc->metrics_fd != NULL) {
        fflush(cc->metrics_fd);
        fclose(cc->metrics_fd);
    }
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
zncc_read_chunk(zncc_chunkcache *cc, zncc_chunk_info chunk_info, char *data) {
    struct timespec start, end;
    TIME_NOW(&start);

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
    size_t sz = cc->zones_total * (sizeof(struct zbd_zone));
    struct zbd_zone *zones = malloc(sz);
    if (zones == NULL) {
        fprintf(stderr, "Couldn't allocate %lu bytes for zones\n", sz);
        return -1;
    }
    unsigned int nr_zones = cc->zones_total;
    ret = zbd_report_zones(fd, ofst, len, ZBD_RO_ALL, zones, &nr_zones);
    if (ret != 0) {
        fprintf(stderr, "Couldn't report zone info\n");
        return ret;
    }

    dbg_printf("read [zone,chunk]=[%d,%d]\n", chunk_info.zone, chunk_info.chunk);

    unsigned long long wp =
        CHUNK_POINTER(cc->zone_size, cc->chunk_size, chunk_info.chunk, chunk_info.zone);

    dbg_printf("zone %u write pointer: %llu\n", chunk_info.zone, wp);

    // ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

    // write to fd at offset 0

    size_t b = pread(fd, data, cc->chunk_size, wp);
    if (b != cc->chunk_size) {
        fprintf(stderr, "Couldn't read from fd\n");
        ret = -1;
        goto cleanup;
    }

    // (*data)[512] = '\0';

    // dbg_printf("Read %s from fd\n", *data);

cleanup:
    zbd_close(fd);
    free(zones);
    TIME_NOW(&end);
    double t_now = SINCE_BEGIN(end);
    metric_printf(cc->metrics_fd, "%f,%s,%0.2f\n", t_now, METRIC_READ, TIME_DIFFERENCE_MILLISEC(start, end));
    return ret;
}

/**
 * @brief Write buffer to disk
 *
 * @param to_write Total size of write
 * @param buffer   Buffer to write to disk
 * @param fd       Disk file descriptor
 * @param write_size Granularity for each write
 * @return int     Non-zero on error
 */
static int
write_out(int fd, size_t to_write, char * buffer, ssize_t write_size, unsigned long long wp_start) {
    int ret = 0;

    ssize_t bytes_written;
    size_t total_written = 0;

    errno = 0;
    while (total_written < to_write) {
        bytes_written = pwrite(fd, buffer + total_written, write_size, wp_start+total_written);
        fsync(fd);
        // dbg_printf("Wrote %ld bytes to fd at offset=%llu\n", bytes_written, wp_start+total_written);
        if ((bytes_written == -1) || (errno != 0)) {
            dbg_printf("Error: %s\n", strerror(errno));
            dbg_printf("Couldn't write to fd\n");
            return -1;
        }
        total_written += bytes_written;
        // dbg_printf("total_written=%ld bytes of %zu\n", total_written, to_write);
    }
    return 0;
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
    struct timespec start, end;
    TIME_NOW(&start);

    struct zbd_info info;

    int ret = 0;

    uint64_t zone_ptr = CHUNK_POINTER(cc->zone_size, cc->chunk_size, 0, chunk_info.zone);

    int fd = zbd_open(cc->device, O_RDWR, &info);
    if (fd < 0) {
        fprintf(stderr, "Error opening device: %s\n", cc->device);
        return fd;
    }

    // If len is 0, at most nr_zones zones starting from ofst up to the end on the device
    // capacity will be reported.
    off_t ofst = 0;
    off_t len = 0;
    size_t sz = cc->zones_total * (sizeof(struct zbd_zone));
    struct zbd_zone *zones = malloc(sz);
    if (zones == NULL) {
        fprintf(stderr, "Couldn't allocate %lu bytes for zones\n", sz);
        return -1;
    }
    unsigned int nr_zones = cc->zones_total;
    ret = zbd_report_zones(fd, ofst, len, ZBD_RO_ALL, zones, &nr_zones);
    if (ret != 0) {
        fprintf(stderr, "Couldn't report zone info\n");
        return ret;
    }

    ret = zbd_open_zones(fd, zone_ptr, 1);
    if (ret != 0) {
        fprintf(stderr, "Failed to open zones\n");
        goto cleanup;
    }
    dbg_printf("write [zone,chunk]=[%d,%d]\n", chunk_info.zone, chunk_info.chunk);

    dbg_printf("zone %u write pointer: %llu, zone size=%u, chunk size=%u\n",
    chunk_info.zone, zones[chunk_info.zone].wp, cc->zone_size, cc->chunk_size);
    dbg_printf("Chunk pointer: %lu\n", CHUNK_POINTER(cc->zone_size, cc->chunk_size, chunk_info.chunk, chunk_info.zone));
    // ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

    // printf("((%u) * (%d)) + ((%d) * (%d))=%d\n", cc->zone_size, chunk_info.zone, chunk_info.chunk,
    //         cc->chunk_size, (((cc->zone_size) * (chunk_info.zone)) + ((chunk_info.chunk) * (cc->chunk_size))));
    assert(CHUNK_POINTER(cc->zone_size, cc->chunk_size, chunk_info.chunk, chunk_info.zone) ==
           zones[chunk_info.zone].wp);

    unsigned long long wp_old = zones[chunk_info.zone].wp;
    ret = write_out(fd, cc->chunk_size, data, WRITE_GRANULARITY,
                    CHUNK_POINTER(cc->zone_size, cc->chunk_size, chunk_info.chunk, chunk_info.zone));
    if (ret != 0) {
        goto cleanup;
    }

    // ret = zbd_report_zones(fd, ofst, len, ZBD_RO_ALL, zones, &nr_zones);
    // if (ret != 0) {
    //     fprintf(stderr, "Couldn't report zone info\n");
    //     return ret;
    // }
    // dbg_printf("wp_old=%llu, wp_new=%llu\n", wp_old, zones[chunk_info.zone].wp);
    // assert(wp_old+cc->chunk_size == zones[chunk_info.zone].wp);

    ret = zbd_close_zones(fd, zone_ptr, 1);
    if (ret != 0) {
        fprintf(stderr, "Failed to close zone\n");
    }

    if (chunk_info.chunk >= (cc->chunks_per_zone-1)) {
        ret = zbd_finish_zones(fd, zone_ptr, 1);
        if (ret != 0) {
            fprintf(stderr, "Failed to finish zone\n");
        }
    }

cleanup:
    zbd_close(fd);
    free(zones);
    TIME_NOW(&end);
    double t_now = SINCE_BEGIN(end);
    metric_printf(cc->metrics_fd, "%f,%s,%0.2f\n", t_now, METRIC_WRITE, TIME_DIFFERENCE_MILLISEC(start, end));
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

    dbg_printf("intermediate_uuid->offset=%ld, intermediate_uuid->uuid_hash=%u\n",
               intermediate_uuid->offset, intermediate_uuid->uuid_hash);

    return ret;
}

static void
update_epoch(zncc_chunkcache *cc, zncc_chunk_info *zi, bool full) {
    uint64_t old_epoch = cc->epoch_list[zi->zone].epoch_chunks[zi->chunk].chunk_time;
    uint64_t new_epoch = microsec_since_epoch();
    dbg_printf("EPOCH AVG OLD for zone=%lu\n", cc->epoch_list[zi->zone].time_sum);
    // Update total epoch avg
    cc->epoch_list[zi->zone].time_sum = update_average(
        cc->epoch_list[zi->zone].time_sum, cc->zones_total, old_epoch, new_epoch);
    cc->epoch_list[zi->zone].epoch_chunks[zi->chunk].chunk_time = new_epoch;
    cc->epoch_list[zi->zone].full = full;
    dbg_printf("EPOCH AVG NEW for zone=%lu\n", cc->epoch_list[zi->zone].time_sum);
    dbg_printf("EPOCH OLD for chunk=%lu\n", old_epoch);
    dbg_printf("EPOCH OLD for chunk=%lu\n", new_epoch);
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

    // Add next chunk in zone to free list
    if (zi.chunk < (cc->chunks_per_zone - 1)) {
        zncc_bucket_push_back(&cc->free_list,
                               (zncc_chunk_info){.chunk = zi.chunk + 1, .zone = zi.zone});
        // print_free_list(&cc->free_list);
    }

    update_epoch(cc, &zi, zi.chunk >= (cc->chunks_per_zone-1));

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

    struct timespec start, end;
    TIME_NOW(&start);

    accesses++;

    int ret = 0;
    uint32_t bucket;

    dbg_printf("Get: uuid=%s\n", uuid);

    __uuid_intermediate intermediate_uuid;
    memset(&intermediate_uuid, 0, sizeof(intermediate_uuid));

    ret = setup_intermediate_uuid(&intermediate_uuid, uuid, offset);

    uint32_t hash;
    ret = crc32_hash((char *) &intermediate_uuid, sizeof(intermediate_uuid), &hash);
    if (ret != 0) {
        return ret;
    }

    bucket = find_bucket(hash, cc->chunks_total);

    dbg_printf("Found bucket: %u\n", bucket);

    zncc_chunk_info data_out;

    // Use chunk multiple
    size_t adjusted_size = adjust_size_to_multiple(size, cc->chunk_size);
    char * buf = malloc(adjusted_size);
    if (buf == NULL) {
        nomem();
    }

    if (zncc_bucket_peek_by_uuid(&cc->buckets[bucket], uuid, offset, &data_out) != 0) {
        cc->s3->callback_data.buffer = buf;
        ret = zncc_s3_get(cc->s3, uuid, offset, size);
        if (ret != 0) {
            dbg_printf("S3 get failed: uuid=%s, bucket=%u\n", uuid, bucket);
            return ret;
        }

        off_t t_offset = offset;
        uint32_t written = 0;
        while (written < size) {
            uint32_t remaining = size - written;
            uint32_t write_now = cc->chunk_size;
            if (remaining < cc->chunk_size) {
                write_now = remaining;
            }

            dbg_printf("Writing: uuid=%s, bucket=%u, offset=%ld, size=%u, data=0x%" PRIXPTR "\n",
                       uuid, bucket, t_offset, write_now, (uintptr_t)cc->s3->callback_data.buffer+written);
            ret = zncc_put_in_bucket(cc, bucket, uuid, t_offset, write_now, cc->s3->callback_data.buffer+written);
            if (ret != 0) {
                dbg_printf("Failed bucket put: uuid=%s, bucket=%u\n", uuid, bucket);
                return ret;
            }
            t_offset += write_now;
            written += write_now;
        }

        *data = cc->s3->callback_data.buffer;
    } else {
        hit++;
        *data = buf;
        dbg_printf("Read from chunk\n");
        uint32_t num_chunk = adjusted_size / cc->chunk_size;
        uint32_t offset = 0;
        for (int offset = 0; offset < adjusted_size; offset+=cc->chunk_size) {
            // TODO: Multi-chunk
            int tret = zncc_read_chunk(cc, data_out, *data+offset);
            if (tret != 0) {
                free(buf);
                return -1;
            }
        }
    }

    TIME_NOW(&end);
    double t_now = SINCE_BEGIN(end);
    metric_printf(cc->metrics_fd, "%f,%s,%0.2f\n", t_now, METRIC_HITRATE, (float)hit / accesses);
    metric_printf(cc->metrics_fd, "%f,%s,%0.2f\n", t_now, METRIC_GET, TIME_DIFFERENCE_MILLISEC(start, end));

    dbg_printf("accesses=%d, hit rate=%.4f\n", accesses, (float)hit / accesses);

    return zncc_evict(cc);
}

#define MAX_ZONE_USAGE 1073741824

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

    // *chunks_in_zone = zone_size / chunk_size; // Always rounds down
    // TODO: Bug on writes > 1GiB
    *chunks_in_zone = MAX_ZONE_USAGE / chunk_size; // Always rounds down

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

static void
init_epoch_list(zncc_chunkcache *cc) {
    cc->epoch_list = malloc(cc->zones_total * sizeof(zncc_epoch_list));
    if (cc->epoch_list == NULL) {
        nomem();
    }
    size_t sz_chunks = cc->chunks_per_zone * sizeof(zncc_epoch_chunk);
    uint64_t epoch_now = microsec_since_epoch();
    for (int i = 0; i < cc->zones_total; i++) {
        cc->epoch_list[i].full = false;
        cc->epoch_list[i].time_sum = epoch_now;
        cc->epoch_list[i].epoch_chunks = malloc(sz_chunks);
        if (cc->epoch_list[i].epoch_chunks == NULL) {
            nomem();
        }
        for (int j = 0; j < cc->chunks_per_zone; j++) {
            cc->epoch_list[i].epoch_chunks[j].chunk_time = epoch_now;
        }
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
zncc_init(zncc_chunkcache *cc, char const *const device, uint64_t chunk_size, zncc_s3 *s3, char const *const metrics_file) {
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

    cc->zone_size = info.zone_size;

    if (chunks_in_zone(chunk_size, cc->zone_size, &cc->chunks_per_zone) != 0) {
        ret = -1;
        goto cleanup;
    }

    cc->s3 = s3;

    cc->device = device;
    cc->chunk_size = chunk_size;
    cc->zones_total = info.nr_zones;

    // TEST
    cc->zones_total = ZONES_USED;
    // TEST FIN

    cc->chunks_total = cc->zones_total * cc->chunks_per_zone;

    zncc_bucket_init_list(&cc->free_list);
    populate_free_list(cc);

    dbg_printf("Populated free list\n");

    cc->buckets = malloc(cc->chunks_total * sizeof(zncc_bucket_list));
    if (cc->buckets == NULL) {
        nomem();
    }

    init_epoch_list(cc);

    if (cc->chunks_per_zone <= 0) {
        dbg_printf("Minimum 1 chunk per zone, found %u\n", cc->chunks_per_zone);
        ret = -1;
        goto cleanup;
    }

    for (int i = 0; i < cc->chunks_total; i++) {
        zncc_bucket_init_list(&cc->buckets[i]);
    }

    dbg_printf("Initialized chunk cache:\n"
               "device=%s\nchunks_per_zone=%u\nzones_total=%u\ntotal_chunks=%u\nchunks_size=%u\npblock_size=%u\n",
               device, cc->chunks_per_zone, cc->zones_total, cc->chunks_total, cc->chunk_size, info.pblock_size);

    dbg_printf("zone_size (bytes)=%llu\n", info.zone_size);

    cc->metrics_fd = NULL;
    if (metrics_file != NULL) {
        cc->metrics_fd = fopen(metrics_file, "w");
        if (cc->metrics_fd == NULL) {
            ret = -1;
            goto cleanup;
        }
        setvbuf(cc->metrics_fd, METRIC_BUFFER, _IOFBF, sizeof(METRIC_BUFFER));
    }

    metric_printf(cc->metrics_fd, "timestamp,type,timems\n");
    fflush(cc->metrics_fd);

    TIME_NOW(&started_zncc);


cleanup:
    zbd_close(fd);
    return ret;
}
