#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdint.h>

#define UUID_SIZE 37

void nomem();
char * genuuid();
int string_to_uint64(char const * const str, uint64_t *num);

/* Will only print messages (to stdout) when DEBUG is defined */
#ifdef DEBUG
#define dbg_printf(M, ...) printf("%s: " M , __func__, ##__VA_ARGS__)
#else
#define dbg_printf(...)
#endif

// Get chunk number from (zone, chunk)
#define ABSOLUTE_CHUNK(z, c) (((z) * (c)) + (c))

#endif
