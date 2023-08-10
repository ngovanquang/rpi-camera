#include "media_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include "logger.h"

media_frame_s *create_media_frame(media_frame_e frame_type, int frame_number, char *frame_data, int data_size)
{
    media_frame_s *new_frame = (media_frame_s *)malloc(sizeof(media_frame_s));
    if (new_frame == NULL)
    {
        LOG_ERROR("Unable to allocate new frame");
        return NULL;
    }

    new_frame->frame_data = (char *)malloc(data_size);
    if (new_frame->frame_data == NULL)
    {
        LOG_ERROR("Unable to allocate new frame");
        return NULL;
    }
    memcpy(new_frame->frame_data, frame_data, data_size);
    new_frame->frame_size = data_size;
    new_frame->frame_type = frame_type;
    new_frame->frame_number = frame_number;
    return new_frame;
}

// Function to create an empty queue
media_queue_s *create_media_queue()
{
    media_queue_s *queue = (media_queue_s *)malloc(sizeof(media_queue_s));
    queue->front = NULL;
    queue->rear = NULL;
    pthread_mutex_init(&(queue->lock), NULL);
    return queue;
}

// Function to check if the queue is empty
int is_empty(media_queue_s *queue)
{
    return queue->front == NULL;
}

// Function to enqueue a frame
void enqueue(media_queue_s *queue, media_frame_s *frame)
{
    node_s *new_node = (node_s *)malloc(sizeof(node_s));
    new_node->media_frame = frame;
    new_node->next = NULL;

    pthread_mutex_lock(&(queue->lock));

    if (is_empty(queue))
    {
        queue->front = new_node;
        queue->rear = new_node;
    }
    else
    {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    pthread_mutex_unlock(&(queue->lock));

    // LOG_INFOR("Enqueue frame type: %s, frame number: %d, frame size: %d", frame->frame_type ? "AAC" : "H264", frame->frame_number, frame->frame_size);
}

// Function to dequeue a frame
media_frame_s *dequeue(media_queue_s *queue)
{
    pthread_mutex_lock(&(queue->lock));

    if (is_empty(queue))
    {
        // LOG_INFOR("Queue is empty");
        pthread_mutex_unlock(&(queue->lock));
        return NULL;
    }

    node_s *front_node = queue->front;
    media_frame_s *front_frame = front_node->media_frame;
    queue->front = front_node->next;

    if (queue->front == NULL)
    {
        queue->rear = NULL;
    }

    pthread_mutex_unlock(&(queue->lock));

    free(front_node);
    // LOG_INFOR("Dequeue frame type: %s, frame number: %d, frame size: %d", front_frame->frame_type ? "AAC" : "H264", front_frame->frame_number, front_frame->frame_size);

    return front_frame;
}

// Function to display the contents of the queue
void display_queue(media_queue_s *queue)
{
    pthread_mutex_lock(&(queue->lock));

    if (is_empty(queue))
    {
        LOG_INFOR("Queue is empty");
        return;
    }

    node_s *current_node = queue->front;
    printf("Queue: \n");
    while (current_node != NULL)
    {
        printf("(%d, %d, %d) ", current_node->media_frame->frame_type, current_node->media_frame->frame_number, current_node->media_frame->frame_size);
        current_node = current_node->next;
    }

    printf("\n");

    pthread_mutex_unlock(&(queue->lock));
}