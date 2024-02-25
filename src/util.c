#include "util.h"

#include "znsccache.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <uuid/uuid.h>

/**
 * @brief Exit if NOMEM
 */
void
nomem() {
    fprintf(stderr, "ERROR: No memory\n");
    exit(ENOMEM);
}

/**
 * @brief Generate a 37byte UUID
 *
 * @return char* UUID or NULL on error
 */
char *
genuuid() {
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    char *uuid = malloc(UUID_SIZE);
    if (uuid == NULL) {
        return NULL;
    }
    uuid_unparse_upper(binuuid, uuid);

    return uuid;
}

/**
 * @brief Convert a string to an unsigned 64bit int
 *
 * @param str Input string in number form
 * @param num String as an uint64
 * @return int Non-zero on error
 */
int
string_to_uint64(char const *const str, uint64_t *num) {
    if (sscanf(str, "%" SCNu64, num) == 1) {
        return 0;
    }

    dbg_printf("Failed to convert %s to uint64_t\n", str);

    return -1;
}
