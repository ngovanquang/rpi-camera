#include "tcp_control.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include "logger.h"
#include <assert.h>

typedef struct tcp_context_s
{
    int tcp_control_socket;
    tcp_state_e tcp_state;
    int tcp_listen_socket;
    char *ip_address;
    int ip_port;
    // char *control_message;
    struct sockaddr_in client_address;
    int addr_len;
    // handle_control_message_t handle_message_cb;
} tcp_context_s;

static tcp_context_s *tcp_ctx = NULL;

tcp_context get_tcp_instance(void)
{
    if (tcp_ctx == NULL)
    {
        tcp_ctx = (tcp_context)calloc(sizeof(tcp_context_s), 1);
        if (tcp_ctx == NULL)
        {
            LOG_ERROR("Unable to allocate tcp_ctx");
            assert(tcp_ctx != NULL);
        }
        LOG_INFOR("TCP allocate OK");
    }
    tcp_ctx->ip_address = NULL;
    // tcp_ctx->control_message = NULL;
    return tcp_ctx;
}

static int init_tcp_connection(tcp_context_s *ctx)
{
    struct sockaddr_in serverAddress, clientAddress;
    int opt = 1;
    int addrlen = sizeof(serverAddress);

    // Create socket
    if ((ctx->tcp_listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        LOG_ERROR("TCP Socket creation failed");
        return TCP_RETURN_FAILED;
    }

    // Set socket options
    if (setsockopt(ctx->tcp_listen_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        LOG_ERROR("Setsockopt failed");
        return TCP_RETURN_FAILED;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(ctx->ip_port);

    // Bind the socket to a specific address and port
    if (bind(ctx->tcp_listen_socket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        LOG_ERROR("Bind failed");
        return TCP_RETURN_FAILED;
    }

    return TCP_RETURN_OK;
}

static void stop_tcp_connection(tcp_context_s *ctx)
{
    close(ctx->tcp_control_socket);
    close(ctx->tcp_listen_socket);
}

void change_tcp_to_idle_state(tcp_context ctx)
{
    if (ctx == NULL)
    {
        ctx = get_tcp_instance();
    }
    else
    {
        if (ctx->tcp_state != TCP_STATE_IDLE)
        {
            stop_tcp_connection(ctx);
        }
    }
    ctx->tcp_control_socket = -1;
    ctx->tcp_listen_socket = -1;
    ctx->tcp_state = TCP_STATE_IDLE;
}

static int listen_tcp_socket(tcp_context ctx)
{
    // Listen for incoming connections
    if (listen(ctx->tcp_listen_socket, 3) < 0)
    {
        LOG_ERROR("TCP listen failed");
        return TCP_RETURN_FAILED;
    }
    LOG_INFOR("Server started. Waiting for client...\n");
    return TCP_RETURN_OK;
}

int change_tcp_to_listen_state(tcp_context ctx)
{
    if (ctx == NULL)
    {
        change_tcp_to_idle_state(ctx);
        init_tcp_connection(ctx);
        if (listen_tcp_socket(ctx) == TCP_RETURN_FAILED)
            return TCP_RETURN_FAILED;
    }
    else
    {
        if (ctx->tcp_state == TCP_STATE_IDLE)
        {
            init_tcp_connection(ctx);
            if (listen_tcp_socket(ctx) == TCP_RETURN_FAILED)
                return TCP_RETURN_FAILED;
        }
        else if (ctx->tcp_state == TCP_STATE_DISCONNECTED)
        {
            change_tcp_to_idle_state(ctx);
            init_tcp_connection(ctx);
            if (listen_tcp_socket(ctx) == TCP_RETURN_FAILED)
                return TCP_RETURN_FAILED;
        }
        else if (ctx->tcp_state == TCP_STATE_ACCEPTED)
        {
            if (ctx->tcp_listen_socket != -1)
            {
                close(ctx->tcp_control_socket);
                ctx->tcp_control_socket = -1;
            }
            else
            {
                close(ctx->tcp_control_socket);
                if (listen_tcp_socket(ctx) == TCP_RETURN_FAILED)
                    return TCP_RETURN_FAILED;
            }
        }
    }
    LOG_INFOR("Change tcp to listen state successful");
    ctx->tcp_state = TCP_STATE_LISTEN;
    return TCP_RETURN_OK;
}

int change_tcp_to_accepted_state(tcp_context ctx)
{
    if (ctx == NULL)
    {
        change_tcp_to_listen_state(ctx);
    }
    else
    {
        if (ctx->tcp_state == TCP_STATE_IDLE || ctx->tcp_state == TCP_STATE_DISCONNECTED)
        {
            change_tcp_to_listen_state(ctx);
        }
    }
    if ((ctx->tcp_control_socket = accept(ctx->tcp_listen_socket, (struct sockaddr *)&ctx->client_address, (socklen_t *)&ctx->addr_len)) < 0)
    {
        LOG_ERROR("Failed to change tcp to Accepted state");
        return TCP_RETURN_FAILED;
    }
    ctx->tcp_state = TCP_STATE_ACCEPTED;
    LOG_INFOR("Client connected.\n");
    return TCP_RETURN_OK;
}

int recv_control_message(tcp_context ctx, char **buffer, int *buffer_len)
{
    // Read from the client
    if (ctx != NULL)
    {
        if (ctx->tcp_state == TCP_STATE_ACCEPTED)
        {
            memset(*buffer, 0, BUFFER_SIZE);
            *buffer_len = read(ctx->tcp_control_socket, *buffer, BUFFER_SIZE);
            if (*buffer_len == 0)
            {
                LOG_ERROR("Client is disconnected.");
                ctx->tcp_state = TCP_STATE_DISCONNECTED;
                return TCP_RETURN_FAILED;
            }
        }
        else
        {
            LOG_ERROR("Can not recv control packet, tcp is not accept state");
            return TCP_RETURN_FAILED;
        }
    }
    return TCP_RETURN_OK;
    // LOG_INFOR("Received from client: %2x %2x %2x %2x %2x\n", ctx->control_message[0], ctx->control_message[1], ctx->control_message[2], ctx->control_message[3], ctx->control_message[4]);
}

void destroy_tcp_control(tcp_context ctx)
{
    if (ctx != NULL)
    {
        if (ctx->ip_address != NULL)
        {
            free(ctx->ip_address);
        }
        free(ctx);
    }
}

int set_tcp_ipaddress(tcp_context ctx, char *ipaddress, int ipaddress_len)
{
    if (ctx != NULL)
    {
        if (ipaddress != NULL)
        {
            if (ctx->ip_address != NULL)
            {
                memcpy(ctx->ip_address, ipaddress, ipaddress_len);
            }
            else
            {
                ctx->ip_address = strdup(ipaddress);
            }
        }
        else
        {
            LOG_ERROR("Could not set tcp ipaddress, ipaddress field is NULL");
            return TCP_RETURN_FAILED;
        }
    }
    else
    {
        LOG_ERROR("Could not set tcp ipaddress, you have to get tcp instance first");
        return TCP_RETURN_FAILED;
    }
    return TCP_RETURN_OK;
}
int get_tcp_ipaddress(tcp_context ctx, char **ipaddress, int *ipaddress_len)
{
    if (ctx != NULL)
    {
        if (ctx->ip_address != NULL)
        {
            *ipaddress = ctx->ip_address;
            *ipaddress_len = strlen(ctx->ip_address);
        }
        else
        {
            LOG_ERROR("TCP ipaddress is not yet set");
            return TCP_RETURN_FAILED;
        }
    }
    else
    {
        LOG_ERROR("Could not get tcp ipaddress, you have to get tcp instance first");
        return TCP_RETURN_FAILED;
    }
    return TCP_RETURN_OK;
}
int set_tcp_ipport(tcp_context ctx, int ipport)
{
    if (ctx != NULL)
    {
        if (ipport <= MIN_PORT || ipport >= MAX_PORT)
        {
            LOG_ERROR("ip port is <= %d or >= %d", MIN_PORT, MAX_PORT);
            return TCP_RETURN_FAILED;
        }
        LOG_INFOR("Set tcp ip port to: %d", ipport);
        ctx->ip_port = ipport;
    }
    else
    {
        LOG_ERROR("Could not get tcp ipport, you have to get tcp instance first");
        return TCP_RETURN_FAILED;
    }
    return TCP_RETURN_OK;
}
int get_tcp_iport(tcp_context ctx, int *ipport)
{
    if (ctx != NULL)
    {
        *ipport = ctx->ip_port;
    }
    else
    {
        LOG_ERROR("Could not get tcp ipport, you have to get tcp instance first");
        return TCP_RETURN_FAILED;
    }
    return TCP_RETURN_OK;
}

int get_tcp_state(tcp_context ctx, tcp_state_e *tcp_state)
{
    if (ctx != NULL)
    {
        *tcp_state = ctx->tcp_state; 
    } else {
        LOG_ERROR("Could not get tcp state, you have to get tcp instance first");
        return TCP_RETURN_FAILED;
    }
    return TCP_RETURN_OK;
}