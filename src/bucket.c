#include <stdlib.h>

#include "znsccache.h"

void zncc_bucket_push(zncc_bucket_node ** head, zncc_chunk_info chunk_info) {
    zncc_bucket_node * new = (zncc_bucket_node *) malloc(sizeof(zncc_bucket_node));

    new->chunk_info = chunk_info;
    new->next = *head;
    *head = new;
}
