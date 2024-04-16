#ifndef ZNSCCACHE_H
#define ZNSCCACHE_H

#include "util.h"

#include <libs3.h>
#include <stdint.h>
#include <stdio.h>

#define UUID_SIZE 37

// BUCKET

typedef struct zncc_chunk_info {
    char uuid[UUID_SIZE];
    uint32_t chunk;
    uint32_t valid_size;
    uint32_t zone;
    off_t offset;
} zncc_chunk_info;

typedef struct zncc_bucket_node {
    zncc_chunk_info data;
    struct zncc_bucket_node *next;
    struct zncc_bucket_node *prev;
} zncc_bucket_node;

typedef struct zncc_bucket_list {
    uint32_t length;
    zncc_bucket_node *head;
    zncc_bucket_node *tail;
} zncc_bucket_list;

typedef struct zncc_epoch_list {
    uint64_t time_sum;
    uint64_t *chunk_times;
} zncc_epoch_list;

void
zncc_bucket_destroy_list(zncc_bucket_list *list);
void
zncc_bucket_init_list(zncc_bucket_list *list);
void
zncc_bucket_push_back(zncc_bucket_list *list, zncc_chunk_info data);
int
zncc_bucket_pop_back(zncc_bucket_list *list, zncc_chunk_info *data_out);
void
zncc_bucket_push_front(zncc_bucket_list *list, zncc_chunk_info data);
int
zncc_bucket_peek_by_uuid(zncc_bucket_list *list, char const *const uuid, off_t offset,
                         zncc_chunk_info *data_out);
int
zncc_bucket_pop_by_uuid(zncc_bucket_list *list, char const *const uuid, off_t offset,
                        zncc_chunk_info *data_out);

// s3
// Callback data structure
typedef struct zncc_get_object_callback_data {
    char *buffer;
    size_t buffer_size;
    size_t buffer_position;
} zncc_get_object_callback_data;

typedef struct zncc_s3 {
    char *bucket_name;
    char *access_key_id;
    char *secret_access_key;
    char *host_name;
    S3Status status;
    S3BucketContext bucket_context;
    S3GetConditions get_conditions;
    S3GetObjectHandler get_object_handler;
    zncc_get_object_callback_data callback_data;
} zncc_s3;

void
zncc_s3_init(zncc_s3 *ctx, char *bucket_name, char *access_key_id, char *secret_access_key,
             char *host_name, size_t buffer_sz);
void
zncc_s3_destroy(zncc_s3 *ctx);
int
zncc_s3_get(zncc_s3 *ctx, char const *obj_id, uint64_t start_byte, uint64_t byte_count);

// CACHE

typedef struct zncc_chunkcache {
    uint32_t zone_size;
    uint32_t chunk_size;
    uint32_t chunks_total;
    uint32_t chunks_per_zone;
    uint32_t zones_total;
    uint32_t *allocated;
    zncc_epoch_list *epoch_list;
    const char *device;
    zncc_bucket_list *buckets;
    zncc_bucket_list free_list;
    zncc_s3 *s3;
} zncc_chunkcache;

// int
// zncc_put(zncc_chunkcache *cc, char const *const uuid, char *data);
int
zncc_init(zncc_chunkcache *cc, char const *const device, uint64_t chunk_size, zncc_s3 *s3);
void
zncc_destroy(zncc_chunkcache *cc);
int
zncc_get(zncc_chunkcache *cc, char const *const uuid, off_t offset, uint32_t size, char **data);
int
zncc_write_chunk(zncc_chunkcache *cc, zncc_chunk_info chunk_info, char *data);
int
zncc_read_chunk(zncc_chunkcache *cc, zncc_chunk_info chunk_info, char *data);

void
print_bucket(zncc_bucket_list *bucket, uint32_t b_num);
void
print_free_list(zncc_bucket_list *bucket);
#endif
