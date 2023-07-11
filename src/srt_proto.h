#ifndef __SRT_PROTO__
#define __SRT_PROTO__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "srt/srt.h"
#include "logger.h"

#define SRT_PROTO_FAILED (-1)
#define SRT_PROTO_OK (0)
#define MAX_PORT (55000)
#define MIN_PORT (1000)
#define MAX_SRT_CLIENT (5)

typedef enum
{
    SRT_MODE_CALLER = 0,
    SRT_MODE_LISTENER,
    SRT_MODE_RE
} srt_mode_e;

typedef enum
{
    NONE = 0,
    ACCEPTED
} srt_state_e;

typedef struct srt_context
{
    int srt_socket;
    int srt_mode;
    int udp_socket;
    int their_fd[MAX_SRT_CLIENT];
    int their_fd_index;
    int num_active_client;
    const char *ip_address;
    int ip_port;
    const char *stream_id;
    int srt_state;
    pthread_t accept_thread;
} srt_context_s;

srt_context_s *allocate_srt_context();

void deallocate_srt_context(srt_context_s *cxt);

int init_srt_proto(srt_context_s *cxt);

int destroy_srt_proto(srt_context_s *cxt);

#endif