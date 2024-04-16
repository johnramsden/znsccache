#include "znsccache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static S3Status statusG = 0;

static S3Status
get_object_data_callback(int bufferSize, const char *buffer, void *callbackData) {
    zncc_get_object_callback_data *data = (zncc_get_object_callback_data *) callbackData;
    size_t bytes_to_copy = (data->buffer_size - data->buffer_position > bufferSize) ?
                               bufferSize :
                               data->buffer_size - data->buffer_position;

    if (bytes_to_copy) {
        memcpy(data->buffer + data->buffer_position, buffer, bytes_to_copy);
        data->buffer_position += bytes_to_copy;
    }

    return S3StatusOK;
}

static int
copy_s3_str(char **out, char const *in) {
    *out = malloc(strlen(in) + 1);
    if (*out == NULL) {
        return -1;
    }
    strcpy(*out, in);
    return 0;
}

static S3Status
responsePropertiesCallback(const S3ResponseProperties *properties, void *callbackData) {
    return S3StatusOK;
}

static void
responseCompleteCallback(S3Status status, const S3ErrorDetails *error, void *callbackData) {
    statusG = status;
    if (error != NULL) {
        dbg_printf("%s\n", error->message);
    }
    dbg_printf("status=%d\n", status);
}

/**
 * @brief Initialize S3
 *
 * @param ctx               S3 context
 * @param bucket_name       Name of S3 bucket (ownership transferred to ctx)
 * @param access_key_id     Id of S3 key (ownership transferred to ctx)
 * @param secret_access_key Secret S3 key (ownership transferred to ctx)
 * @param host_name         S3 Host
 * @param buffer_sz         Size of calls to s3 bucket
 * @return int              Non-zero on error
 */
void
zncc_s3_init(zncc_s3 *ctx, char *bucket_name, char *access_key_id, char *secret_access_key,
             char *host_name, size_t buffer_sz) {

    ctx->access_key_id = access_key_id;
    ctx->secret_access_key = secret_access_key;
    ctx->bucket_name = bucket_name;

    // Initialize the library
    S3_initialize("s3", S3_INIT_ALL, NULL);

    // Set up the bucket context
    ctx->bucket_context.hostName = host_name;
    ctx->bucket_context.protocol = S3ProtocolHTTP;
    ctx->bucket_context.uriStyle = S3UriStylePath;

    ctx->bucket_context.bucketName = ctx->bucket_name;
    ctx->bucket_context.accessKeyId = ctx->access_key_id;
    ctx->bucket_context.secretAccessKey = ctx->secret_access_key;

    S3GetConditions getConditions = {.ifModifiedSince = -1,
                                     .ifNotModifiedSince = -1,
                                     .ifMatchETag = NULL,
                                     .ifNotMatchETag = NULL};

    // Initialize get conditions
    ctx->get_conditions = getConditions;

    // Initialize the callback data structure
    ctx->callback_data.buffer = NULL;
    ctx->callback_data.buffer_size = buffer_sz;
    ctx->callback_data.buffer_position = 0;

    ctx->get_object_handler.responseHandler =
        (S3ResponseHandler){&responsePropertiesCallback, &responseCompleteCallback};

    ctx->get_object_handler.getObjectDataCallback = &get_object_data_callback;

    ctx->status = 0;
}

void
zncc_s3_destroy(zncc_s3 *ctx) {
    free(ctx->bucket_name);
    free(ctx->access_key_id);
    free(ctx->secret_access_key);
    free(ctx->callback_data.buffer);
    S3_deinitialize();
}

/**
 * @brief Get data by object ID from S3
 *
 * Pre-req, ctx->callback_data.buffer is allocated to correct size
 *
 * @param ctx
 * @param obj_id
 * @param start_byte
 * @param byte_count
 * @return int
 */
int
zncc_s3_get(zncc_s3 *ctx, char const *obj_id, uint64_t start_byte, uint64_t byte_count) {

    statusG = S3StatusOK;

    dbg_printf("get(%s)\n", obj_id);
    int ret = 0;
    S3_get_object(&ctx->bucket_context, obj_id, &ctx->get_conditions, start_byte, byte_count, 0,
                  &ctx->get_object_handler, &ctx->callback_data);

    // Check if the buffer_position is less than buffer_size to ensure there was no overflow
    if (ctx->callback_data.buffer_position <= ctx->callback_data.buffer_size) {
        dbg_printf("Object fetched successfully, pos=%lu\n", ctx->callback_data.buffer_position);
    } else {
        fprintf(stderr, "Buffer overflow detected. pos=%lu, size=%lu\n",
                ctx->callback_data.buffer_position, ctx->callback_data.buffer_size);
        ret = -1;
    }

    dbg_printf("S3 get object status: %d\n", ctx->status);

    ctx->callback_data.buffer_position = 0;
    ctx->status = statusG;

    if (ctx->status != S3StatusOK) {
        fprintf(stderr, "S3 get object error: %d\n", ctx->status);
        ret = -1;
    }

    return ret;
}
