#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

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
uint64_t
microsec_since_epoch();
int
msleep(long msec);
uint64_t
update_average(uint64_t current_avg, int n, uint64_t value_to_remove, uint64_t value_to_add);
size_t
adjust_size_to_multiple(size_t size, size_t multiple);

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
#define CHUNK_POINTER(z_sz, c_sz, c_num, z_num)                                                    \
    (((uint64_t) (z_sz) * (uint64_t) (z_num)) + ((uint64_t) (c_num) * (uint64_t) (c_sz)))

#define TIME_NOW(_t) (clock_gettime(CLOCK_MONOTONIC, (_t)))

#define TIME_DIFFERENCE(_start, _end)                                                              \
    ((_end.tv_sec + _end.tv_nsec / 1.0e9) - (_start.tv_sec + _start.tv_nsec / 1.0e9))

#define TIME_DIFFERENCE_MILLISEC(_start, _end)                                                     \
    ((_end.tv_nsec / 1.0e6 < _start.tv_nsec / 1.0e6)) ?                                            \
        ((_end.tv_sec - 1.0 - _start.tv_sec) * 1.0e3 + (_end.tv_nsec / 1.0e6) + 1.0e3 -            \
         (_start.tv_nsec / 1.0e6)) :                                                               \
        ((_end.tv_sec - _start.tv_sec) * 1.0e3 + (_end.tv_nsec / 1.0e6) -                          \
         (_start.tv_nsec / 1.0e6))

#define TIME_DIFFERENCE_NSEC(_start, _end)                                                         \
    ((_end.tv_nsec < _start.tv_nsec)) ?                                                            \
        ((_end.tv_sec - 1 - (_start.tv_sec)) * 1e9 + _end.tv_nsec + 1e9 - _start.tv_nsec) :        \
        ((_end.tv_sec - (_start.tv_sec)) * 1e9 + _end.tv_nsec - _start.tv_nsec)

#define TIME_DIFFERENCE_GETTIMEOFDAY(_start, _end)                                                 \
    ((_end.tv_sec + _end.tv_usec / 1.0e6) - (_start.tv_sec + _start.tv_usec / 1.0e6))

#endif
