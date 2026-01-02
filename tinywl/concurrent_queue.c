// concurrent_queue.c - Thread-safe queue implementation
#include "concurrent_queue.h"
#include <stdlib.h>
#include <pthread.h>

// The internal node structure for our linked list
typedef struct queue_node {
    render_notification_t item; // This type is now known
    struct queue_node* next;
} queue_node_t;

// The actual implementation of our opaque queue struct
struct concurrent_queue {
    queue_node_t* head;
    queue_node_t* tail;
    pthread_mutex_t mutex;
    // We don't need a condition variable for try_pop, but it would be
    // needed for a blocking pop(). It's good practice to keep it.
    pthread_cond_t cond;
};

concurrent_queue_t* queue_create(void) {
    concurrent_queue_t* q = calloc(1, sizeof(concurrent_queue_t));
    if (!q) {
        return NULL;
    }

    q->head = NULL;
    q->tail = NULL;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);

    return q;
}

void queue_destroy(concurrent_queue_t* q) {
    if (!q) {
        return;
    }

    pthread_mutex_lock(&q->mutex);

    // Free any remaining nodes in the queue to prevent memory leaks
    queue_node_t* current = q->head;
    while (current != NULL) {
        queue_node_t* temp = current;
        current = current->next;
        free(temp);
    }

    pthread_mutex_unlock(&q->mutex);

    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
    free(q);
}

// In concurrent_queue.c

// Add 'const' to the second argument to match the header file.
void queue_push(concurrent_queue_t* q, const render_notification_t* item) {
    queue_node_t* new_node = malloc(sizeof(queue_node_t));
    if (!new_node) {
        // Failed to allocate memory, handle error appropriately
        return;
    }
    new_node->item = *item; // This is a copy, so it's safe.
    new_node->next = NULL;

    pthread_mutex_lock(&q->mutex);

    if (q->tail == NULL) {
        // Queue is empty
        q->head = new_node;
        q->tail = new_node;
    } else {
        // Add to the end of the queue
        q->tail->next = new_node;
        q->tail = new_node;
    }

    // Signal that an item has been added
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

bool queue_try_pop(concurrent_queue_t* q, render_notification_t* item) {
    pthread_mutex_lock(&q->mutex);

    if (q->head == NULL) {
        // Queue is empty
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    // Remove the first item from the queue
    queue_node_t* old_head = q->head;
    *item = old_head->item;
    q->head = old_head->next;

    if (q->head == NULL) {
        // The queue is now empty
        q->tail = NULL;
    }

    free(old_head);

    pthread_mutex_unlock(&q->mutex);
    return true;
}