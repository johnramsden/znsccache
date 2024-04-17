#include "util.h"

#include "znsccache.h"

#include <cjson/cJSON.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

char *
read_file_to_string(const char *file_name) {
    FILE *f = fopen(file_name, "rb");
    if (f == NULL) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = malloc(length + 1);
    if (data != NULL) {
        fread(data, 1, length, f);
        data[length] = '\0';
    }
    fclose(f);
    return data;
}

int
read_credentials(const char *filename, char **key, char **secret, char **bucket, char **host_name) {
    int ret = 0;
    char *json_data = read_file_to_string(filename);
    if (json_data == NULL) {
        fprintf(stderr, "Unable to read file %s\n", filename);
        return -1;
    }

    cJSON *json = cJSON_Parse(json_data);
    if (json == NULL) {
        char const *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        free(json_data);
        return -1;
    }

    const cJSON *access_key_id = cJSON_GetObjectItemCaseSensitive(json, "key");
    const cJSON *secret_access_key = cJSON_GetObjectItemCaseSensitive(json, "secret");
    const cJSON *json_bucket = cJSON_GetObjectItemCaseSensitive(json, "bucket");
    const cJSON *json_host_name = cJSON_GetObjectItemCaseSensitive(json, "host_name");

    if (cJSON_IsString(access_key_id) && (access_key_id->valuestring != NULL) &&
        cJSON_IsString(secret_access_key) && (secret_access_key->valuestring != NULL) &&
        cJSON_IsString(json_bucket) && (json_bucket->valuestring != NULL) &&
        cJSON_IsString(json_host_name) && (json_host_name->valuestring != NULL)) {
        *key = malloc(strlen(access_key_id->valuestring) + 1);
        if (*key == NULL) {
            nomem();
        }
        *secret = malloc(strlen(secret_access_key->valuestring) + 1);
        if (*secret == NULL) {
            nomem();
        }
        *bucket = malloc(strlen(json_bucket->valuestring) + 1);
        if (*bucket == NULL) {
            nomem();
        }
        *bucket = malloc(strlen(json_bucket->valuestring) + 1);
        if (*bucket == NULL) {
            nomem();
        }
        *host_name = malloc(strlen(json_host_name->valuestring) + 1);
        if (*host_name == NULL) {
            nomem();
        }
        strcpy(*key, access_key_id->valuestring);
        strcpy(*secret, secret_access_key->valuestring);
        strcpy(*bucket, json_bucket->valuestring);
        strcpy(*host_name, json_host_name->valuestring);
        ret = 0;
    } else {
        fprintf(stderr, "Invalid JSON: Required fields are missing.\n");
        ret = -1;
    }

    cJSON_Delete(json);
    free(json_data);
}

long int microsec_since_epoch() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

