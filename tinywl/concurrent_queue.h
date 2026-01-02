// In concurrent_queue.h
#ifndef CONCURRENT_QUEUE_H
#define CONCURRENT_QUEUE_H
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include "nano_ipc.h"

typedef struct concurrent_queue concurrent_queue_t;

typedef enum {
    RENDER_NOTIFICATION_TYPE_FRAME_READY,
    RENDER_NOTIFICATION_TYPE_PLUGIN_DIED,
    RENDER_NOTIFICATION_TYPE_WINDOW_MOVE,  // ADD THIS
} render_notification_type_t;
// **REPLACE your old struct with this new one**
typedef struct {
    render_notification_type_t type;
    pid_t plugin_pid;

    // A union allows this struct to hold different data for different notification types,
    // which is the correct and most efficient design.
    union {
        // Data for RENDER_NOTIFICATION_TYPE_FRAME_READY
        int ready_buffer_index;

        // Data for RENDER_NOTIFICATION_TYPE_WINDOW_MOVE
        struct {
            int window_index;
            double move_x;
            double move_y;
        } move_data;

    }; // The union can be anonymous for direct access (e.g., notification.ready_buffer_index)
} render_notification_t;

concurrent_queue_t* queue_create(void);
void queue_destroy(concurrent_queue_t* q);
void queue_push(concurrent_queue_t* q, const render_notification_t* item);
bool queue_try_pop(concurrent_queue_t* q, render_notification_t* item);

#endif // CONCURRENT_QUEUE_H