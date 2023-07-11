#include "srt_proto.h"

srt_context_s *allocate_srt_context()
{
    srt_context_s *ctx = (srt_context_s *)malloc(sizeof(srt_context_s));

    if (ctx == NULL)
    {
        LOG_ERROR("Memory allocation for SRT failed.\n");
        return ctx;
    }
    ctx->num_active_client = 0;
    ctx->their_fd_index = 0;
    ctx->udp_socket = -1;
    ctx->srt_state = NONE;
    ctx->srt_socket = SRT_INVALID_SOCK;
    for (int i = 0; i < MAX_SRT_CLIENT; ++i)
    {
        ctx->their_fd[i] = SRT_INVALID_SOCK;
    }

    return ctx;
}

void deallocate_srt_context(srt_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_ERROR("ctx NULL");
    }
    free(ctx);
}

/**
 * @brief thêm các tùy chọn SRT
 *
 */
static int srt_set_options(srt_context_s *ctx)
{
    int yes = 1;
    int latency = 240;

    if (ctx == NULL)
    {
        LOG_ERROR("ctx NULL");
        return SRT_PROTO_FAILED;
    }

    LOG_INFOR("srt setsockflag\n");
    if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_SNDSYN, &yes, sizeof yes))
    {
        LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
        return SRT_PROTO_FAILED;
    }

    if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_RCVSYN, &yes, sizeof yes))
    {
        LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
        return SRT_PROTO_FAILED;
    }

    if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_LATENCY, &latency, sizeof latency))
    {
        LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
        return SRT_PROTO_FAILED;
    }

    // int timeout = 10000;
    // if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_CONNTIMEO, &timeout, sizeof timeout))
    // {
    //     LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return SRT_PROTO_FAILED;
    // }

    return SRT_PROTO_OK;
}

void *srt_accept_loop(void *ctx)
{
    // Setup EID in order to pick up either readiness or error.
    // THis is only to make a formal response side, nothing here is to be tested.
    srt_context_s *srt_ctx = (srt_context_s *)ctx;
    int eid = srt_epoll_create();

    // Subscribe to R | E
    int re = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    srt_epoll_add_usock(eid, srt_ctx->srt_socket, &re);

    SRT_EPOLL_EVENT results[2];

    for (;;)
    {
        auto state = srt_getsockstate(srt_ctx->srt_socket);
        if (int(state) > int(SRTS_CONNECTED))
        {
            LOG_ERROR("[T] Listener socket closed, exitting");
            break;
        }

        LOG_INFOR("[T] Waiting for epoll to accept");
        int res = srt_epoll_uwait(eid, results, 2, 1000);
        if (res == 1)
        {
            if (results[0].events == SRT_EPOLL_IN)
            {
                SRTSOCKET acp = srt_accept(srt_ctx->srt_socket, NULL, NULL);
                if (acp == SRT_INVALID_SOCK)
                {
                    LOG_ERROR("[T] Accept failed, so exitting");
                    break;
                }

                // add valid client_fd into array
                for (int i = 0; i < MAX_SRT_CLIENT; ++i)
                {

                    if (srt_ctx->their_fd[i] == SRT_INVALID_SOCK)
                    {
                        srt_ctx->their_fd[i] = acp;
                        srt_ctx->srt_state = ACCEPTED;
                        srt_ctx->their_fd_index = i;
                        srt_ctx->num_active_client++;
                        LOG_INFOR("Accepted a srt client, number of active client: %d", srt_ctx->num_active_client);
                        // pthread_t send_file_thread;
                        // int st;
                        // st = pthread_create(&send_file_thread, NULL, send_ts_handle, (void *)srt_ctx);
                        // if (st)
                        // {
                        //     LOG_ERROR("Error creating thread for receiver: %d\n", st);
                        //     break;
                        // }
                        break;
                    }
                }

                continue;
            }

            // Then it can only be SRT_EPOLL_ERR, which
            // can be done by having the socket closed
            break;
        }

        if (res == 0) // probably timeout, just repeat
        {
            LOG_INFOR("[T] (NOTE: epoll timeout, still waiting)");
            continue;
        }
    }
    srt_epoll_release(eid);
    pthread_exit(NULL);
}

int init_srt_proto(srt_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_ERROR("ctx NULL");
        return SRT_PROTO_FAILED;
    }
    if (ctx->ip_address == NULL || ctx->ip_port <= MIN_PORT || ctx->ip_port >= MAX_PORT)
    {
        LOG_ERROR("ip address or port is <= %d or >= %d", MIN_PORT, MAX_PORT);
        return SRT_PROTO_FAILED;
    }

    if (ctx->srt_mode > SRT_MODE_RE || ctx->srt_mode < SRT_MODE_CALLER)
    {
        LOG_ERROR("srt mode error");
        return SRT_PROTO_FAILED;
    }

    if (ctx->udp_socket == -1)
    {
        LOG_INFOR("SRT Using bind mode");
        // return SRT_PROTO_FAILED;
    }
    else
    {
        LOG_INFOR("SRT Using bind acquire mode");
    }

    /**Start SRT*/
    int st;
    struct sockaddr_storage their_addr;

    LOG_INFOR("srt startup\n");
    srt_startup();

    LOG_INFOR("srt socket\n");
    ctx->srt_socket = srt_create_socket();
    if (ctx->srt_socket == SRT_ERROR)
    {
        LOG_ERROR("srt_socket: %s\n", srt_getlasterror_str());
        return SRT_PROTO_FAILED;
    }

    LOG_INFOR("srt remote address\n");
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(ctx->ip_port);
    if (ctx->srt_mode == SRT_MODE_LISTENER)
    {
        sa.sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        if (inet_pton(AF_INET, ctx->ip_address, &sa.sin_addr) != 1)
        {
            LOG_ERROR("inet_pton failed");
            return SRT_PROTO_FAILED;
        }
    }

    if (ctx->udp_socket != -1)
    {
        LOG_INFOR("SRT bind\n");
        st = srt_bind_acquire(ctx->srt_socket, ctx->udp_socket);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_bind: %s\n", srt_getlasterror_str());
            return SRT_PROTO_FAILED;
        }
    }
    else
    {
        if (ctx->srt_mode == SRT_MODE_LISTENER)
        {
            st = srt_bind(ctx->srt_socket, (struct sockaddr *)&sa, sizeof sa);
        }
    }

    // Set srt options
    if (srt_set_options(ctx) == SRT_PROTO_FAILED)
    {
        return SRT_PROTO_FAILED;
    }

    if (ctx->srt_mode == SRT_MODE_CALLER)
    {

        LOG_INFOR("srt connect");
        st = srt_connect(ctx->srt_socket, (struct sockaddr *)&sa, sizeof sa);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_connect: %s\n", srt_getlasterror_str());
            return SRT_PROTO_FAILED;
        }
        ctx->srt_state = ACCEPTED;
    }
    else if (ctx->srt_mode == SRT_MODE_LISTENER)
    {
        LOG_INFOR("SRT listen");
        st = srt_listen(ctx->srt_socket, 2);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_listen: %s\n", srt_getlasterror_str());
            return SRT_PROTO_FAILED;
        }

        // Listener Callback
        // srt_listen_callback(ctx->srt_socket, &SrtTestListenCallback, (void *)ctx);

        // Accept thread
        st = pthread_create(&ctx->accept_thread, NULL, srt_accept_loop, (void *)ctx);
        if (st)
        {
            LOG_ERROR("Error creating thread for accept: %d\n", st);
            return SRT_PROTO_FAILED;
        }
    }
    return SRT_PROTO_OK;
}

int destroy_srt_proto(srt_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_ERROR("ctx is NULL");
        return SRT_PROTO_FAILED;
    }

    LOG_INFOR("srt close\n");
    int st;
    st = srt_close(ctx->srt_socket);
    if (st == SRT_ERROR)
    {
        LOG_ERROR("srt_close: %s\n", srt_getlasterror_str());
        return SRT_PROTO_FAILED;
    }

    LOG_INFOR("srt cleanup");
    srt_cleanup();
    deallocate_srt_context(ctx);

    return SRT_PROTO_OK;
}