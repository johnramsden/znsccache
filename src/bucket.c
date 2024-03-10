#include "znsccache.h"

#include <stdlib.h>
#include <string.h>

void
zncc_bucket_destroy_list(zncc_bucket_list *list) {
    zncc_bucket_node *b = list->head;
    zncc_bucket_node *b_free;
    while (1) {
        if (b == NULL) {
            break;
        }
        b_free = b;
        b = b->next;
        free(b_free);
    }
}

/**
 * @brief Get a chunk item based on UUID and remove from list
 *
 * @param list Linked list
 * @param uuid UUID to match on
 * @param data_out Found chunk
 * @return int zero if match, nonzero on no match
 */
int
zncc_bucket_pop_by_uuid(zncc_bucket_list *list, char const *const uuid, off_t offset,
                        zncc_chunk_info *data_out) {
    zncc_bucket_node *current = list->head;
    while (current != NULL) {
        if (strcmp(current->data.uuid, uuid) != 0) {
            // No match
            current = current->next;
            continue;
        }

        if (current->data.offset != offset) {
            // No match
            current = current->next;
            continue;
        }

        // Matched uuid
        *data_out = current->data;

        if (current->prev != NULL) {
            current->prev->next = current->next;
        } else {
            // Removed the head node
            list->head = current->next;
        }

        if (current->next != NULL) {
            current->next->prev = current->prev;
        } else {
            // Removed the tail node
            list->tail = current->prev;
        }

        free(current);
        return 0;
    }

    return -1; // No match
}

/**
 * @brief Get a chunk item based on UUID
 *
 * @param list Linked list
 * @param uuid UUID to match on
 * @param data_out Found chunk
 * @return int zero if match, nonzero on no match
 */
int
zncc_bucket_peek_by_uuid(zncc_bucket_list *list, char const *const uuid, off_t offset,
                         zncc_chunk_info *data_out) {
    zncc_bucket_node *current = list->head;
    while (current != NULL) {
        if (strcmp(current->data.uuid, uuid) != 0) {
            // No match
            current = current->next;
            continue;
        }

        if (current->data.offset != offset) {
            // No match
            current = current->next;
            continue;
        }

        // Matched uuid
        *data_out = current->data;
        return 0;
    }

    return -1; // No match
}

void
zncc_bucket_init_list(zncc_bucket_list *list) {
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
}

void
zncc_bucket_push_back(zncc_bucket_list *list, zncc_chunk_info data) {
    zncc_bucket_node *new = (zncc_bucket_node *) malloc(sizeof(zncc_bucket_node));
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

int
zncc_bucket_pop_back(zncc_bucket_list *list, zncc_chunk_info *data_out) {
    if (list->tail == NULL) {
        // Empty
        return -1;
    }

    zncc_bucket_node *removed_node = list->tail;
    *data_out = removed_node->data;

    list->tail = removed_node->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else { // List was only one element long
        list->head = NULL;
    }

    list->length--;

    dbg_printf("Pop chunk=%u, zone=%u, new length=%u\n", data_out->chunk, data_out->zone,
               list->length);

    free(removed_node);
    return 0;
}

void
zncc_bucket_push_front(zncc_bucket_list *list, zncc_chunk_info data) {
    zncc_bucket_node *new = (zncc_bucket_node *) malloc(sizeof(zncc_bucket_node));
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
