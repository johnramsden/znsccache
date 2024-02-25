#ifndef ZNSCCACHE_H
#define ZNSCCACHE_H

#include <stdio.h>
#include <stdint.h>
#include "util.h"

#define UUID_SIZE 37

// BUCKET

typedef struct zncc_chunk_info {
    char uuid[UUID_SIZE];
    uint32_t chunk;
    uint32_t zone;
} zncc_chunk_info;

typedef struct zncc_bucket_node {
    zncc_chunk_info data;
    struct zncc_bucket_node * next;
    struct zncc_bucket_node * prev;
} zncc_bucket_node;

typedef struct zncc_bucket_list {
    uint32_t length;
    zncc_bucket_node* head;
    zncc_bucket_node* tail;
} zncc_bucket_list;

void zncc_bucket_destroy_list(zncc_bucket_list* list);
void zncc_bucket_init_list(zncc_bucket_list* list);
void zncc_bucket_push_back(zncc_bucket_list* list, zncc_chunk_info data);
int zncc_bucket_pop_back(zncc_bucket_list* list, zncc_chunk_info *data_out);
void zncc_bucket_push_front(zncc_bucket_list* list, zncc_chunk_info data);
int zncc_bucket_peek_by_uuid(zncc_bucket_list* list, char const * const uuid, zncc_chunk_info *data_out);
int zncc_bucket_pop_by_uuid(zncc_bucket_list* list, char const * const uuid, zncc_chunk_info *data_out);

// CACHE

typedef struct zncc_chunkcache {
    uint32_t chunk_size;
    uint32_t chunks_total;
    uint32_t chunks_per_zone;
    uint32_t zones_total;
    uint32_t *allocated;
    const char * device;
    zncc_bucket_list *buckets;
    zncc_bucket_list free_list;
} zncc_chunkcache;

int zncc_put(zncc_chunkcache *cc, char const * const uuid, char * data);
int zncc_init(zncc_chunkcache *cc, char const * const device, uint64_t chunk_size);
void zncc_destroy(zncc_chunkcache *cc);
int zncc_get(zncc_chunkcache *cc, char const * const uuid, char ** data);
int zncc_write_chunk(zncc_chunkcache *cc, zncc_chunk_info chunk_info, char * data);
int zncc_read_chunk(zncc_chunkcache *cc, zncc_chunk_info chunk_info, char ** data);

#endif
