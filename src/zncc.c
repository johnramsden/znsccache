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

/**
 * @brief Place an item in the cache
 *
 * @param cc
 * @param uuid
 * @param data
 * @return int
 */
int zncc_put(zncc_chunkcache *cc, char const * const uuid, void * data) {
    uint32_t bucket;
    int ret = find_bucket(uuid, cc->chunks_total, &bucket);
    // if (cc->buckets[bucket] == NULL) {

    // }
    if (ret != 0) {
        return ret;
    }
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
 * @brief Initialize the chunk cache
 *
 * @param[out] cc        Chunk cache to initialize
 * @param[in] device     ZNS block device
 * @param[in] chunk_size Size of chunk in bytes
 * @return int3          Non-zero on error
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

    cc->allocated = malloc(cc->chunks_total *sizeof(uint32_t));
    if (cc->allocated == NULL) {
        nomem();
    }

    cc->buckets = malloc(cc->chunks_total *sizeof(zncc_bucket_node));
    if (cc->buckets == NULL) {
        nomem();
    }

    dbg_printf("Initialized chunk cache:\n"
               "device=%s\nchunks_per_zone=%u\nzones_total=%u\ntotal_chunks=%u\n",
               device, cc->chunks_per_zone, cc->zones_total, cc->chunks_total );

    return 0;
}
