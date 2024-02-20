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
 * @brief Convert a string to an unsigned 64bit int
 *
 * @param str Input string in number form
 * @param num String as an uint64
 * @return int Non-zero on error
 */
static int string_to_uint64(char const * const str, uint64_t *num) {
    if (sscanf(str, "%" SCNu64, num) == 1) {
        return 0;
    }

    dbg_printf("Failed to convert %s to uint64_t\n", str);

    return -1;
}


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

int
main(int argc, char **argv) {
    int ret = 0;
    int fd = 0;
    char *device = NULL;
    char *chunk_size_str = NULL;
    uint64_t chunk_size_int;
    int c;
    zncc_chunkcache cc;

    if (geteuid() != 0) {
        fprintf(stderr, "Please run as root\n");
        return -1;
    }

    opterr = 0;

    while ((c = getopt (argc, argv, "d:c:")) != -1) {
        switch (c) {
            case 'd':
                device = optarg;
                break;
            case 'c':
                chunk_size_str = optarg;
                break;
            case '?':
                if (optopt == 'd') {
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                } else if (isprint (optopt)) {
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                } else
                    fprintf (stderr,
                        "Unknown option character `\\x%x'.\n",
                        optopt);
                return -1;
            default:
                exit(-1);
        }
    }

    if (device == NULL) {
        fprintf (stderr, "Option -d is a required flag.\n");
        exit (EXIT_FAILURE);
    }

    if (chunk_size_str == NULL) {
        fprintf (stderr, "Option -c is a required flag.\n");
        exit (EXIT_FAILURE);
    }

    printf("Device: %s\n", device);

    ret = string_to_uint64(chunk_size_str, &chunk_size_int);

    if (ret != 0) {
        fprintf (stderr, "Invalid chunk size=%s (bytes).\n", chunk_size_str);
        exit(EXIT_FAILURE);
    }

    ret = zncc_init(&cc, device, chunk_size_int);
    if (ret != 0) {
        fprintf (stderr, "Failed to initialize chunk cache=%s.\n", chunk_size_str);
        exit(EXIT_FAILURE);
    }

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
