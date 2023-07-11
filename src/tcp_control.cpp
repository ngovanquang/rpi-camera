#include "tcp_control.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include "logger.h"

tcp_control_t *allocate_tcp_context(void)
{
    tcp_control_t *ctx = (tcp_control_t *)calloc(1, sizeof(tcp_control_t));
    if (ctx == NULL)
    {
        LOG_ERROR("Can't allocate tcp context");
        return NULL;
    }
    ctx->control_message = (char *)calloc(1, BUFFER_SIZE);
    if (ctx->control_message == NULL)
    {
        LOG_ERROR("Can't allocate tcp control message buffer");
        free(ctx);
        return NULL;
    }
    return ctx;
}
void init_tcp_connection(tcp_control_t *ctx)
{
    struct sockaddr_in serverAddress, clientAddress;
    int opt = 1;
    int addrlen = sizeof(serverAddress);

    // Create socket
    if ((ctx->tcp_listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        LOG_ERROR("TCP Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(ctx->tcp_listen_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        LOG_ERROR("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(ctx->ip_port);

    // Bind the socket to a specific address and port
    if (bind(ctx->tcp_listen_socket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        LOG_ERROR("Bind failed");
        exit(EXIT_FAILURE);
    }
}

void start_tcp_connection(tcp_control_t *ctx)
{
    // Listen for incoming connections
    if (listen(ctx->tcp_listen_socket, 3) < 0)
    {
        LOG_ERROR("Listen failed");
        exit(EXIT_FAILURE);
    }

    LOG_INFOR("Server started. Waiting for client...\n");

    // Accept incoming connection
    if ((ctx->tcp_control_socket = accept(ctx->tcp_listen_socket, (struct sockaddr *)&ctx->client_address, (socklen_t *)&ctx->addr_len)) < 0)
    {
        LOG_ERROR("Accept failed");
        exit(EXIT_FAILURE);
    }

    LOG_INFOR("Client connected.\n");
}

int recv_control_message(tcp_control_t *ctx)
{
    // Read from the client
    int bytesRead = read(ctx->tcp_control_socket, ctx->control_message, BUFFER_SIZE);
    printf("size: %d\n", bytesRead);
    if (bytesRead == 0)
    {
        LOG_ERROR("Client is disconnected.");
        return 0;
    }
    ctx->handle_message_cb(ctx->control_message, bytesRead);
    memset(ctx->control_message, 0, BUFFER_SIZE);
    return bytesRead;
    // LOG_INFOR("Received from client: %2x %2x %2x %2x %2x\n", ctx->control_message[0], ctx->control_message[1], ctx->control_message[2], ctx->control_message[3], ctx->control_message[4]);
}

void stop_tcp_connection(tcp_control_t *ctx)
{
    close(ctx->tcp_control_socket);
    close(ctx->tcp_listen_socket);
}

void destroy_tcp_connection(tcp_control_t *ctx)
{
    free(ctx);
}
