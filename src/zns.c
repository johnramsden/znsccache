#include <stdio.h>
#include <fcntl.h>

#include <libzbd/zbd.h>
#include <stdlib.h>

#define DEVICE "/dev/nvme0n2"


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
main() {
    int ret = 0;
    int fd = 0;

    if (geteuid() != 0) {
        fprintf(stderr, "Please run as root\n");
        return -1;
    }

    struct zbd_info info;

    fd = zbd_open(DEVICE, O_RDWR, &info);
    if (fd < 0) {
        fprintf(stderr, "Error opening device: %s\n", DEVICE);
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
    int num = 2;
    ssize_t b = pwrite(fd, &num, sizeof(num), zones[0].wp);
    if (b != sizeof(num)) {
        fprintf(stderr, "Couldn't write to fd\n");
        ret = -1;
        goto cleanup;
    }

    int buf = 0;
    b = pread(fd, &buf, sizeof(buf), zones[0].wp);
    if (b != sizeof(num)) {
        fprintf(stderr, "Couldn't read from fd\n");
        ret = -1;
        goto cleanup;
    }
    if (buf != num) {
        fprintf(stderr, "buf(%i) != num(%i)\n", buf, num);
        ret = -1;
        goto cleanup;
    }
    printf("Read %i from fd\n", buf);

cleanup:
    zbd_close(fd);
    free(zones);
    return ret;
}
