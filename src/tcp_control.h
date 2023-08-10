#ifndef __TCP_CONTROL__
#define __TCP_CONTROL__

#include <netinet/in.h>

#define BUFFER_SIZE (100)

#define TCP_RETURN_OK (0)
#define TCP_RETURN_FAILED (-1)
#define MIN_PORT (1000)
#define MAX_PORT (50000)

typedef enum
{
    TCP_STATE_IDLE = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_ACCEPTED,
    TCP_STATE_DISCONNECTED
} tcp_state_e;

typedef struct tcp_context_s *tcp_context;

tcp_context get_tcp_instance(void);

void change_tcp_to_idle_state(tcp_context ctx);
int change_tcp_to_listen_state(tcp_context ctx);
int change_tcp_to_accepted_state(tcp_context ctx);
void destroy_tcp_control(tcp_context ctx);

int set_tcp_ipaddress(tcp_context ctx, char *ipaddress);
int get_tcp_ipaddress(tcp_context ctx, char **ipaddress, int *ipaddress_len);

int set_tcp_ipport(tcp_context ctx, int ipport);
int get_tcp_iport(tcp_context ctx, int *ipport);

int recv_control_message(tcp_context ctx, char **buffer, int *buffer_len);

int get_tcp_state(tcp_context ctx, tcp_state_e *tcp_state);

#endif