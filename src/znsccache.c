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

#define S3_BUFF_SZ (8192*1024)

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


int
test_get(zncc_chunkcache *cc, char *test_file) {
    int ret;

    // 32K objects
    size_t xf = 32*1024;
    // size_t xf = 512*1024*1024;

    FILE *file;
    char line[40];  // Adjust buffer size as needed

    file = fopen(test_file, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    int count = 0;
    int total = 2621440;

    while (fgets(line, sizeof(line), file)) {
        char *v1;
        line[36] = '\0';

        if (count % 5000 == 0) {
            printf("%d / %d (%.4f): %s\n", count, total, (float)count / total, line);
        }
        ret = zncc_get(cc, line, 0, xf, &v1);
        if (ret != 0) {
            fprintf(stderr, "Err for uuid=%s\n", line);
        } else {
            free(v1);
        }

        count++;

        // if (count == 30) {
        //     break;
        // }
    }

    fclose(file);

    return 0;
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
test_s3_latency(zncc_chunkcache *cc) {

// * 32K obj  - chunk sz 16K, 32K   - evict, no evict - S3 avg 32K get
// * 512K obj - chunk sz 256K, 512K - evict, no evict - S3 avg 512K get
// * 1M obj   - chunk sz 512K, 1M   - evict, no evict - S3 avg 1M get
// * 1G obj   - chunk sz 512M, 1G   - evict, no evict - S3 avg 1G get
    int ret = 0;
    size_t xf = 1*1024*1024*1024;

    struct timespec start, end;

    #define GET_T_SZ 15
    double times[GET_T_SZ];
    char *test[GET_T_SZ][2] = {
        {"28cde48f-2c60-43b6-b6b9-76556fafc0ed", "s"},
        {"f939bcfc-6e73-4f14-aa58-e630df39e89f", "t"},
        {"35e96113-87e0-42f6-97ea-ce97971801a2", "u"},
        {"2fc318fd-8ec2-4307-b995-20b933d7dcd9", "v"},
        {"57468674-a34d-48ef-9a64-aa7f86e26fba", "w"},
        {"28cde48f-2c60-43b6-b6b9-76556fafc0ed", "s"},
        {"f939bcfc-6e73-4f14-aa58-e630df39e89f", "t"},
        {"35e96113-87e0-42f6-97ea-ce97971801a2", "u"},
        {"2fc318fd-8ec2-4307-b995-20b933d7dcd9", "v"},
        {"57468674-a34d-48ef-9a64-aa7f86e26fba", "w"},
        {"28cde48f-2c60-43b6-b6b9-76556fafc0ed", "s"},
        {"f939bcfc-6e73-4f14-aa58-e630df39e89f", "t"},
        {"35e96113-87e0-42f6-97ea-ce97971801a2", "u"},
        {"2fc318fd-8ec2-4307-b995-20b933d7dcd9", "v"},
        {"57468674-a34d-48ef-9a64-aa7f86e26fba", "w"},
                               };

    double sum = 0;

    size_t adjusted_size = adjust_size_to_multiple(xf, cc->chunk_size);
    char * buf = malloc(adjusted_size);
    if (buf == NULL) {
        nomem();
    }
    for (int i = 0; i < GET_T_SZ; i++) {
        char *v1;

        cc->s3->callback_data.buffer = buf;

        TIME_NOW(&start);
        ret = zncc_s3_get(cc->s3, test[i][0], 0, xf);
        if (ret != 0) {
            printf("S3 get failed %d\n", i);
            return ret;
        }
        TIME_NOW(&end);

        times[i] = TIME_DIFFERENCE_MILLISEC(start, end);
        sum += times[i];
        printf("%d=%f\n", i, times[i]);
    }

    printf("avg=%f\n", sum / GET_T_SZ);

    return ret;
}

int
main(int argc, char **argv) {
    int ret = 0;
    int fd = 0;
    char *device = NULL;
    char *chunk_size_str = NULL;
    char *test_file = NULL;
    char *metrics_file = NULL;
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

    while ((c = getopt(argc, argv, "d:s:c:f:m:")) != -1) {
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
            case 'f':
                test_file = optarg;
                break;
            case 'm':
                metrics_file = optarg;
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
    zncc_s3_init(&s3, bucket, key, secret, host_name, S3_BUFF_SZ);

    ret = zncc_init(&cc, device, chunk_size_int, &s3, metrics_file);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize chunk cache=%s.\n", chunk_size_str);
        exit(EXIT_FAILURE);
    }
    // ret = test_s3_latency(&cc);
    // if (ret != 0) {
    //     fprintf(stderr, "Failed to test s3\n");
    //     exit(EXIT_FAILURE);
    // }

    // basic_write_test(device);

    if (test_file != NULL) {
        ret = test_get(&cc, test_file);
        if (ret != 0) {
            fprintf(stderr, "Test failed\n");
        }
    }

    // // v1[512] = '\0';
    // // dbg_printf("v1[512]=%s\n", v1);

    // // // zncc_get(&cc, "54b4afb0-7624-448d-9c52-ddcc56e56bf2", 0, xf, &v2);

    // // dbg_printf("v2[512]=%s\n", v2);

    // // dbg_printf("v2[0]=%s\n", v2[0]);

    // // ret = zncc_s3_get(cc.s3, "e8ade124-f00a-47ea-aa7d-0dbd55dd0866");

    // // test_put(&cc);

    zncc_destroy(&cc);

    return ret;
}
