#include "znsccache.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <libzbd/zbd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
test_get(zncc_chunkcache *cc) {
    int ret;
    size_t xf = 512*1024*1024;

    #define GET_T_SZ 16
    char *test[GET_T_SZ][2] = {{"6c228e61-abb7-4100-a5df-417f25b47c36", "s"},
                               {"cf752bc3-b33f-435f-ac30-178e971f54a2", "t"},
                               {"7c968f5a-0c97-4cd8-960b-1c1bf6ab67ee", "u"},
                               {"c3f16f76-8f94-40f1-950d-695bb226fe43", "v"},
                               {"54b4afb0-7624-448d-9c52-ddcc56e56bf2", "w"},
                               {"3f5d58a4-1780-46b5-bd96-d4c5b4f55b2b", "x"},
                               {"e5a19178-6b77-40b7-9bf8-f45f70ea19a5", "y"},
                               {"13f9c273-3f2f-486a-8003-ba2fba58b258", "z"},
                               {"c75b2775-f71e-47f9-9500-530c8cf9f344", "a"},
                               {"e5be69c3-6e22-4c2c-aa09-0bff9417c361", "b"},
                               {"982aff04-b7b6-4060-a141-241af8c46375", "c"},
                               {"62c0ae68-e637-4ddc-a0aa-aa1151f88595", "d"},
                               {"7fc3ca1b-3968-4274-9a5b-688f01dca81b", "e"},
                               {"b9cece7d-50ce-4f00-9abb-4e9c07ed68fd", "f"},
                               {"7446cbb0-430b-44c4-9da9-443a044c14d1", "g"},
                               {"8c0630ac-c4b5-4bf1-a7a3-13a7e95032dd", "h"}};

    // #define GET_T_SZ 4
    // char *test[GET_T_SZ][2] = {{"6c228e61-abb7-4100-a5df-417f25b47c36", "s"},
    //                            {"cf752bc3-b33f-435f-ac30-178e971f54a2", "t"},
    //                            {"7c968f5a-0c97-4cd8-960b-1c1bf6ab67ee", "u"},
    //                            {"c3f16f76-8f94-40f1-950d-695bb226fe43", "v"}};
    for (int i = 0; i < GET_T_SZ; i++) {
        char *v1;

        ret = zncc_get(cc, test[i][0], 0, xf, &v1);

        if (ret != 0) {
            dbg_printf("Err for uuid=%s, c=%s\n", test[i][0], test[i][1]);
        }
    }
    // for (int i = 0; i < cc->chunks_total; i++) {
    //     print_bucket(&cc->buckets[i], i);
    // }
}

static int
basic_write_test(char * device) {
    int ret = 0;
    struct zbd_info info;
    int fd = zbd_open(device, O_RDWR, &info);
    if (fd < 0) {
        fprintf(stderr, "Error opening device: %s\n", device);
        return fd;
    }

    print_zbd_info(&info);
    // If len is 0, at most nr_zones zones starting from ofst up to the end on the device
    // capacity will be reported.
    off_t ofst = 0;
    off_t len = 0;
    size_t sz = info.nr_zones*(sizeof(struct zbd_zone));
    struct zbd_zone *zones = malloc(sz);
    if (zones == NULL) {
        fprintf(stderr, "Couldn't allocate %lu bytes for zones\n", sz);
        return -1;
    }
    unsigned int nr_zones = info.nr_zones;
    ret = zbd_report_zones(fd, ofst, len,
                            ZBD_RO_ALL, zones, &nr_zones);
    if (ret != 0) {
        fprintf(stderr, "Couldn't report zone info\n");
        return ret;
    }

    printf("zone %u write pointer: %llu\n", 0, zones[0].wp);

    // ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

    // write to fd at offset 0

    ret = zbd_reset_zones(fd, 0, 0);
    if (ret != 0) {
        fprintf(stderr, "Couldn't reset zones\n");
        goto cleanup;
    }

    size_t to_write = 512*1024*1024;
    char * buffer = malloc(to_write);
    ssize_t bytes_written;
    size_t total_written = 0;
    ssize_t chunk_size = 4096; // Size of each write, e.g., 4 KiB

    while (total_written < to_write) {
        bytes_written = pwrite(fd, buffer + total_written, chunk_size, zones[0].wp);
        dbg_printf("Wrote %ld bytes to fd\n", bytes_written);
        if (bytes_written == -1) {
            fprintf(stderr, "Error: %s\n", strerror(errno));
            fprintf(stderr, "Couldn't write to fd\n");
            ret = -1;
            goto cleanup;
        }
        total_written += bytes_written;
        dbg_printf("total_written=%ld bytes of %zu\n", total_written, to_write);
    }

cleanup:
    zbd_close(fd);
    free(zones);
    free(buffer);
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

    zbd_set_log_level(ZBD_LOG_ERROR);

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
    zncc_s3_init(&s3, bucket, key, secret, host_name, 8192*1024);

    ret = zncc_init(&cc, device, chunk_size_int, &s3);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize chunk cache=%s.\n", chunk_size_str);
        exit(EXIT_FAILURE);
    }

    // basic_write_test(device);

    test_get(&cc);

    // // v1[512] = '\0';
    // // dbg_printf("v1[512]=%s\n", v1);

    // // // zncc_get(&cc, "54b4afb0-7624-448d-9c52-ddcc56e56bf2", 0, xf, &v2);

    // // dbg_printf("v2[512]=%s\n", v2);

    // // dbg_printf("v2[0]=%s\n", v2[0]);

    // // ret = zncc_s3_get(cc.s3, "e8ade124-f00a-47ea-aa7d-0dbd55dd0866");

    // // test_put(&cc);

    // zncc_destroy(&cc);

    return ret;
}
