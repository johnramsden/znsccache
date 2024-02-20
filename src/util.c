#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/**
 * @brief Exit if NOMEM
 */
void
nomem() {
    fprintf(stderr, "ERROR: No memory\n");
    exit(ENOMEM);
}
