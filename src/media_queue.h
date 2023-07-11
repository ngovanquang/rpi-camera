#ifndef __MEDIA_QUEUE__
#define __MEDIA_QUEUE__

#include "media_stream.h"
#include <pthread.h>

#define MEDIA_QUEUE_OK (0)
#define MEDIA_QUEUE_FAILED (-1)

typedef struct media_frame_s
{
    int frame_type;
    int frame_number;
    char *frame_data;
    int frame_size;
} media_frame_s;

// Define the node structure for the queue
typedef struct node_s
{
    media_frame_s *media_frame;
    struct node_s *next;
} node_s;

// Define the queue structure
typedef struct media_queue_s
{
    node_s *front;
    node_s *rear;
    pthread_mutex_t lock;
} media_queue_s;

media_frame_s *create_media_frame(int frame_type, int frame_number, char *frame_data, int data_size);

// Function to create an empty queue
media_queue_s *create_media_queue();

// Function to check if the queue is empty
int is_empty(media_queue_s *queue);

// Function to enqueue a frame
void enqueue(media_queue_s *queue, media_frame_s *frame);

// Function to dequeue a frame
media_frame_s *dequeue(media_queue_s *queue);

// Function to display the contents of the queue
void display_queue(media_queue_s *queue);
#endif