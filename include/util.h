#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define UUID_SIZE 37

void
nomem();
char *
genuuid();
int
string_to_uint64(char const *const str, uint64_t *num);
char *
read_file_to_string(char const *filename);
int
read_credentials(const char *filename, char **key, char **secret, char **bucket, char **host_name);
long int
ms_since_epoch();

/* Will only print messages (to stdout) when DEBUG is defined */
#ifdef DEBUG
#    define dbg_printf(M, ...) printf("%s: " M, __func__, ##__VA_ARGS__)
#else
#    define dbg_printf(...)
#endif

// Get chunk number from (zone, chunk)
#define ABSOLUTE_CHUNK(n_chunks, z, c) (((n_chunks) * (z)) + (c))
// 0, 1, | 2, 3, | 4, 5, | 6, 7

// Get write pointer from (zone, chunk)
#define CHUNK_POINTER(z_sz, c_sz, c_num, z_num) (((uint64_t)(z_sz) * (uint64_t)(z_num)) + ((uint64_t)(c_num) * (uint64_t)(c_sz)))

#endif
