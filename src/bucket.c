#include <stdlib.h>

#include "znsccache.h"

void zncc_bucket_init_list(zncc_bucket_list* list) {
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
}

void zncc_bucket_push_back(zncc_bucket_list* list, zncc_chunk_info data) {
    zncc_bucket_node* new = (zncc_bucket_node*)malloc(sizeof(zncc_bucket_node));
    if (new == NULL) {
        nomem();
    }
    new->data = data;
    new->next = NULL;
    new->prev = list->tail;

    if (list->tail != NULL) {
        list->tail->next = new;
    } else {
        // List is empty
        list->head = new;
    }
    list->tail = new;
    list->length++;
    dbg_printf("Push chunk=%u, zone=%u, new length=%u\n", data.chunk, data.zone, list->length);
}

int zncc_bucket_pop_back(zncc_bucket_list* list, zncc_chunk_info *data_out) {
    if (list->tail == NULL) {
        // Empty
        return -1;
    }

    zncc_bucket_node* removed_node = list->tail;
    *data_out = removed_node->data;

    list->tail = removed_node->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else { // List was only one element long
        list->head = NULL;
    }

    list->length--;

    dbg_printf("Pop chunk=%u, zone=%u, new length=%u\n", data_out->chunk, data_out->zone, list->length);

    free(removed_node);
    return 0;
}

void zncc_bucket_push_front(zncc_bucket_list* list, zncc_chunk_info data) {
    zncc_bucket_node* new = (zncc_bucket_node*)malloc(sizeof(zncc_bucket_node));
    if (new == NULL) {
        nomem();
    }

    new->data = data;
    new->prev = NULL;
    new->next = list->head;

    if (list->head != NULL) {
        list->head->prev = new; // Link old head back to new node
    } else {
        // If the list was empty, new node is the tail.
        list->tail = new;
    }

    list->length++;
    list->head = new;
    dbg_printf("Push chunk=%u, zone=%u, new length=%u\n", data.chunk, data.zone, list->length);
}




