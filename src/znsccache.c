#include "znsccache.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <libzbd/zbd.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * https://github.com/westerndigitalcorporation/libzbd/blob/master/include/libzbd/zbd.h
 */
static void
print_zbd_info(struct zbd_info *info) {
    printf("vendor_id=%s\n", info->vendor_id);
    printf("nr_sectors=%llu\n", info->nr_sectors);
    printf("nr_lblocks=%llu\n", info->nr_lblocks);
    printf("nr_pblocks=%llu\n", info->nr_pblocks);
    printf("zone_size (bytes)=%llu\n", info->zone_size);
    printf("zone_sectors=%u\n", info->zone_sectors);
    printf("lblock_size=%u\n", info->lblock_size);
    printf("pblock_size=%u\n", info->pblock_size);
    printf("nr_zones=%u\n", info->nr_zones);
    printf("max_nr_open_zones=%u\n", info->max_nr_open_zones);
    printf("max_nr_active_zones=%u\n", info->max_nr_active_zones);
    printf("model=%u\n", info->model);
}

// void zncc_bucket_push_back(zncc_bucket_list* list, zncc_chunk_info data);
// int zncc_bucket_pop_back(zncc_bucket_list* list, zncc_chunk_info *data_out);
// void zncc_bucket_push_front(zncc_bucket_list* list, zncc_chunk_info data);

void
test_ll(zncc_chunkcache *cc) {
    zncc_chunk_info ci[10];
    zncc_chunk_info ci_out[10];
    for (int i = 0; i < 10; i++) {
        ci[i] = (zncc_chunk_info){.chunk = i, .zone = i};
    }
    for (int i = 0; i < 10; i++) {
        zncc_bucket_push_front(&cc->free_list, ci[i]);
    }
    for (int i = 0; i < 10; i++) {
        zncc_bucket_pop_back(&cc->free_list, &ci_out[i]);
    }
}

void
test_put(zncc_chunkcache *cc) {
    char *td = malloc(cc->chunk_size);
    td[0] = 'a';
    td[1] = '\0';
    for (int i = 0; i < cc->zones_total; i++) {
        char *uuid = genuuid();
        // zncc_put(cc, uuid, td);
        zncc_get(cc, uuid, 0, 0, NULL);
        free(uuid);
    }
    free(td);
}

int
main(int argc, char **argv) {
    int ret = 0;
    int fd = 0;
    char *device = NULL;
    char *chunk_size_str = NULL;
    char *config = NULL;
    uint64_t chunk_size_int;
    int c;
    zncc_chunkcache cc;

    if (geteuid() != 0) {
        fprintf(stderr, "Please run as root\n");
        return -1;
    }

    opterr = 0;

    while ((c = getopt(argc, argv, "d:s:c:")) != -1) {
        switch (c) {
            case 'd':
                device = optarg;
                break;
            case 's':
                chunk_size_str = optarg;
                break;
            case 'c':
                config = optarg;
                break;
            case '?':
                if (optopt == 'd') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else if (isprint(optopt)) {
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                } else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return -1;
            default:
                exit(-1);
        }
    }

    if (device == NULL) {
        fprintf(stderr, "Option -d is a required flag.\n");
        exit(EXIT_FAILURE);
    }

    if (chunk_size_str == NULL) {
        fprintf(stderr, "Option -s is a required flag.\n");
        exit(EXIT_FAILURE);
    }

    if (config == NULL) {
        fprintf(stderr, "Option -c is a required flag.\n");
        exit(EXIT_FAILURE);
    }

    printf("Device: %s\n", device);

    ret = string_to_uint64(chunk_size_str, &chunk_size_int);

    if (ret != 0) {
        fprintf(stderr, "Invalid chunk size=%s (bytes).\n", chunk_size_str);
        exit(EXIT_FAILURE);
    }

    char *key;
    char *secret;
    char *bucket;
    char *host_name;

    ret = read_credentials(config, &key, &secret, &bucket, &host_name);
    if (ret != 0) {
        fprintf(stderr, "Failed to read credentials from %s.\n", config);
        exit(EXIT_FAILURE);
    }

    zncc_s3 s3;
    zncc_s3_init(&s3, bucket, key, secret, host_name, 512);

    ret = zncc_init(&cc, device, chunk_size_int, &s3);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize chunk cache=%s.\n", chunk_size_str);
        exit(EXIT_FAILURE);
    }

    size_t xf = 512 * 1024;

    char * v1;
    char * v2;

    ret = zncc_get(&cc, "54b4afb0-7624-448d-9c52-ddcc56e56bf2", 0, xf, &v1);
    ret = zncc_get(&cc, "54b4afb0-7624-448d-9c52-ddcc56e56bf2", 0, xf, &v2);

    if (ret != 0){
        exit(EXIT_FAILURE);
    }

    // v1[512] = '\0';
    // dbg_printf("v1[512]=%s\n", v1);

    // // zncc_get(&cc, "54b4afb0-7624-448d-9c52-ddcc56e56bf2", 0, xf, &v2);

    // dbg_printf("v2[512]=%s\n", v2);

    // dbg_printf("v2[0]=%s\n", v2[0]);

    // ret = zncc_s3_get(cc.s3, "e8ade124-f00a-47ea-aa7d-0dbd55dd0866");

    // test_put(&cc);

    zncc_destroy(&cc);

    //     struct zbd_info info;

    //     fd = zbd_open(DEVICE, O_RDWR, &info);
    //     if (fd < 0) {
    //         fprintf(stderr, "Error opening device: %s\n", DEVICE);
    //         return fd;
    //     }

    //     print_zbd_info(&info);
    //     // If len is 0, at most nr_zones zones starting from ofst up to the end on the device
    //     // capacity will be reported.
    //     off_t ofst = 0;
    //     off_t len = 0;
    //     size_t sz = info.nr_zones*(sizeof(struct zbd_zone));
    //     struct zbd_zone *zones = malloc(sz);
    //     if (zones == NULL) {
    //         fprintf(stderr, "Couldn't allocate %lu bytes for zones\n", sz);
    //         return -1;
    //     }
    //     unsigned int nr_zones = info.nr_zones;
    //     ret = zbd_report_zones(fd, ofst, len,
    //                            ZBD_RO_ALL, zones, &nr_zones);
    //     if (ret != 0) {
    //         fprintf(stderr, "Couldn't report zone info\n");
    //         return ret;
    //     }

    //     printf("zone %u write pointer: %llu\n", 0, zones[0].wp);

    //     // ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

    //     // write to fd at offset 0

    //     ret = zbd_reset_zones(fd, 0, 0);
    //     if (ret != 0) {
    //         fprintf(stderr, "Couldn't reset zones\n");
    //         goto cleanup;
    //     }
    //     int num = 2;
    //     ssize_t b = pwrite(fd, &num, sizeof(num), zones[0].wp);
    //     if (b != sizeof(num)) {
    //         fprintf(stderr, "Couldn't write to fd\n");
    //         ret = -1;
    //         goto cleanup;
    //     }

    //     int buf = 0;
    //     b = pread(fd, &buf, sizeof(buf), zones[0].wp);
    //     if (b != sizeof(num)) {
    //         fprintf(stderr, "Couldn't read from fd\n");
    //         ret = -1;
    //         goto cleanup;
    //     }
    //     if (buf != num) {
    //         fprintf(stderr, "buf(%i) != num(%i)\n", buf, num);
    //         ret = -1;
    //         goto cleanup;
    //     }
    //     printf("Read %i from fd\n", buf);

    // cleanup:
    //     zbd_close(fd);
    //     free(zones);
    return ret;
}
