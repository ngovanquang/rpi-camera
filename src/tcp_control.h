#ifndef __TCP_CONTROL__
#define __TCP_CONTROL__

#include <netinet/in.h>

#define BUFFER_SIZE (100)

typedef int(*handle_control_message_t)(const char* cmd, int len);

typedef struct tcp_control_t
{
    int tcp_control_socket;
    int tcp_listen_socket;
    char* ip_address;
    int ip_port;
    char* control_message;
    struct sockaddr_in client_address;
    int addr_len;
    handle_control_message_t handle_message_cb;
} tcp_control_t;

tcp_control_t* allocate_tcp_context(void);
void init_tcp_connection(tcp_control_t* ctx);
void start_tcp_connection(tcp_control_t* ctx);
int recv_control_message(tcp_control_t* ctx);
void stop_tcp_connection(tcp_control_t* ctx);
void destroy_tcp_connection(tcp_control_t* ctx);


#endif