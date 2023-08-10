#ifndef __SRT_PROTO__
#define __SRT_PROTO__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "media_stream.h"
#include "srt/srt.h"
#include "logger.h"

#define SRT_PROTO_FAILED (-1)
#define SRT_PROTO_OK (0)
#define MAX_PORT (55000)
#define MIN_PORT (1000)
#define MAX_SRT_CLIENT (5)
#define SRT_PAYLOAD_SIZE (1316)
typedef enum
{
    SRT_MODE_CALLER = 0,
    SRT_MODE_LISTENER,
    SRT_MODE_RENDERVOUS
} srt_mode_e;

typedef enum
{
    SRT_STATE_IDLE = 0,
    SRT_STATE_LISTEN,
    SRT_STATE_ACCEPTED,
    SRT_STATE_CLIENT_DECREASE,
    SRT_STATE_CLIENT_INCREASE,
    SRT_STATE_CLIENT_MAX,
    SRT_STATE_DISCONNECTED
} srt_state_e;

// typedef struct srt_context
// {
//     int srt_socket;
//     int srt_mode;
//     int udp_socket;
//     int their_fd[MAX_SRT_CLIENT];
//     int their_fd_index;
//     int num_active_client;
//     const char *ip_address;
//     int ip_port;
//     const char *stream_id;
//     int srt_state;
//     pthread_t accept_thread;
// } srt_context_s;

typedef struct srt_context_s * srt_context;

srt_context get_srt_instance(void);

int change_srt_to_idle_state(srt_context ctx);
int change_srt_to_listen_state(srt_context ctx);

void destroy_srt_proto(srt_context ctx);

int set_srt_ipaddress(srt_context ctx, char* ipaddress);
int get_srt_ipaddress(srt_context ctx, char** ipaddress, int* ipaddress_len);

int set_srt_ipport(srt_context ctx, int ipport);
int get_srt_iport(srt_context ctx, int* ipport);

int set_srt_mode(srt_context ctx, srt_mode_e srt_mode);
int get_srt_mode(srt_context ctx, srt_mode_e *srt_mode);

int get_srt_state(srt_context srt_ctx, srt_state_e *srt_state);

int srt_send_buffer(srt_context ctx, char *buffer, int buffer_len, media_frame_e frame_type);

int srt_receive_data_from_primary_client(srt_context ctx, char **buffer, int *buffer_len);

#endif