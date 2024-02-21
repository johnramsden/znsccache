#ifndef ZNSCCACHE_H
#define ZNSCCACHE_H

#include <stdio.h>
#include <stdint.h>

typedef struct zncc_chunk_info {
    uint32_t chunk;
    uint32_t zone;
} zncc_chunk_info;

typedef struct zncc_bucket_node {
    zncc_chunk_info data;
    struct zncc_bucket_node * next;
    struct zncc_bucket_node * prev;
} zncc_bucket_node;

typedef struct zncc_bucket_list {
    zncc_bucket_node* head;
    zncc_bucket_node* tail;
} zncc_bucket_list;

void zncc_bucket_init_list(zncc_bucket_list* list);
void zncc_bucket_push_back(zncc_bucket_list* list, zncc_chunk_info data);
int zncc_bucket_pop_back(zncc_bucket_list* list, zncc_chunk_info *data_out);
void zncc_bucket_push_front(zncc_bucket_list* list, zncc_chunk_info data);

typedef struct zncc_chunkcache {
    uint32_t chunks_total;
    uint32_t chunks_per_zone;
    uint32_t zones_total;
    uint32_t *allocated;
    const char * device;
    zncc_bucket_list *buckets;
    zncc_bucket_list free_list;
} zncc_chunkcache;

int zncc_put(zncc_chunkcache *cc, char const * const uuid, void * data);
int zncc_init(zncc_chunkcache *cc, char const * const device, uint64_t chunk_size);

void nomem();

/* Will only print messages (to stdout) when DEBUG is defined */
#ifdef DEBUG
#define dbg_printf(M, ...) printf("%s: " M , __func__, ##__VA_ARGS__)
#else
#define dbg_printf(...)
#endif

#endif
