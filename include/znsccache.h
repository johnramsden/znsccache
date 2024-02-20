#ifndef ZNSCCACHE_H
#define ZNSCCACHE_H

#include <stdint.h>

typedef struct zncc_chunk_info {
    uint32_t chunk;
    uint32_t zone;
} zncc_chunk_info;

typedef struct zncc_bucket_node {
    zncc_chunk_info chunk_info;
    struct zncc_bucket_node * next;
} zncc_bucket_node;

typedef struct zncc_chunkcache {
    uint32_t chunks_total;
    uint32_t chunks_per_zone;
    uint32_t zones_total;
    uint32_t *allocated;
    const char * device;
    zncc_bucket_node *buckets;

} zncc_chunkcache;

void zncc_bucket_push(zncc_bucket_node ** head, zncc_chunk_info chunk_info);
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
