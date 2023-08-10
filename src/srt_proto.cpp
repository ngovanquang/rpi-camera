#include "srt_proto.h"
// #include "media_stream.h"
#include <assert.h>
#include <stdint.h>

typedef struct srt_context_s
{
    int srt_socket;
    srt_mode_e srt_mode;
    int udp_socket;
    int their_fd[MAX_SRT_CLIENT];
    int isFirstSend[MAX_SRT_CLIENT];
    int their_fd_index;
    int num_active_client;
    char *ip_address;
    int ip_port;
    char *stream_id;
    char *srt_snd_buf;
    int srt_snd_data_len;
    int has_data_to_send;
    srt_state_e srt_state;
    pthread_t listen_thread;
    pthread_mutex_t lock;
} srt_context_s;

static srt_context_s *srt_ctx = NULL;

srt_context_s *get_srt_instance(void)
{
    if (srt_ctx == NULL)
    {
        srt_ctx = (srt_context_s *)calloc(sizeof(srt_context_s), 1);
        if (srt_ctx == NULL)
        {
            LOG_ERROR("Unable to allocate srt_ctx");
            assert(srt_ctx != NULL);
        }

        srt_ctx->ip_address = NULL;
        srt_ctx->stream_id = NULL;
        srt_ctx->listen_thread = NULL;
        srt_ctx->udp_socket = -1;
        srt_ctx->srt_snd_buf = (char *)calloc(SRT_PAYLOAD_SIZE, 1);
        srt_ctx->has_data_to_send = 0;
        srt_ctx->srt_snd_data_len = 0;
        assert(srt_ctx->srt_snd_buf != NULL);

        LOG_INFOR("SRT allocate OK");
    }

    return srt_ctx;
}

static void *srt_accept_loop(void *ctx)
{
    // Setup EID in order to pick up either readiness or error.
    // THis is only to make a formal response side, nothing here is to be tested.
    srt_context_s *t_srt_ctx = (srt_context_s *)ctx;
    int eid = srt_epoll_create();

    // Subscribe to R | E
    int re = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    srt_epoll_add_usock(eid, t_srt_ctx->srt_socket, &re);

    SRT_EPOLL_EVENT results[2];

    for (;;)
    {
        auto state = srt_getsockstate(t_srt_ctx->srt_socket);
        if (int(state) > int(SRTS_CONNECTED))
        {
            LOG_ERROR("[T] Listener socket closed, exitting");
            break;
        }

        // LOG_INFOR("[T] Waiting for epoll to accept");
        int res = srt_epoll_uwait(eid, results, 2, 1000);
        if (res == 1)
        {
            if (results[0].events == SRT_EPOLL_IN)
            {
                SRTSOCKET acp = srt_accept(t_srt_ctx->srt_socket, NULL, NULL);
                if (acp == SRT_INVALID_SOCK)
                {
                    LOG_ERROR("[T] Accept failed, so exitting");
                    break;
                }

                // add valid client_fd into array

                for (int i = 0; i < MAX_SRT_CLIENT; ++i)
                {
                    if (t_srt_ctx->their_fd[i] == SRT_INVALID_SOCK)
                    {
                        t_srt_ctx->their_fd[i] = acp;
                        t_srt_ctx->isFirstSend[i] = 1;
                        if (t_srt_ctx->srt_state == SRT_STATE_ACCEPTED)
                        {
                            t_srt_ctx->srt_state = SRT_STATE_CLIENT_INCREASE;
                            LOG_INFOR("Change srt to \"increase\" state, num active client: %d", t_srt_ctx->num_active_client);
                        }
                        else
                        {
                            t_srt_ctx->srt_state = SRT_STATE_ACCEPTED;
                        }
                        t_srt_ctx->their_fd_index = i;
                        t_srt_ctx->num_active_client++;
                        LOG_INFOR("Accepted a srt client, number of active client: %d", t_srt_ctx->num_active_client);
                        break;
                    }
                }
                if (t_srt_ctx->num_active_client == MAX_SRT_CLIENT)
                {

                    t_srt_ctx->srt_state = SRT_STATE_CLIENT_MAX;
                    LOG_INFOR("Change srt to \"client max\" state, num active client: %d", t_srt_ctx->num_active_client);
                    srt_epoll_release(eid);
                    pthread_exit(NULL);
                }
                continue;
            }

            // Then it can only be SRT_EPOLL_ERR, which
            // can be done by having the socket closed
            break;
        }

        if (res == 0) // probably timeout, just repeat
        {
            // LOG_INFOR("[T] (NOTE: epoll timeout, still waiting)");
            continue;
        }
    }
    srt_epoll_release(eid);
    pthread_exit(NULL);
}

static int start_listen_thread(srt_context_s *ctx)
{
    LOG_INFOR("start srt listen thread");
    if (ctx->listen_thread == NULL)
    {
        int ret = pthread_create(&ctx->listen_thread, NULL, srt_accept_loop, (void *)ctx);
        if (ret)
        {
            LOG_ERROR("Error creating thread for listen srt connection: %d\n", ret);
            return SRT_PROTO_FAILED;
        }
    }
    return SRT_PROTO_OK;
}

static int stop_srt_proto(srt_context_s *ctx)
{
    int st;
    if (ctx != NULL)
    {
        LOG_INFOR("srt close\n");
        st = srt_close(ctx->srt_socket);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_close: %s\n", srt_getlasterror_str());
            return SRT_PROTO_FAILED;
        }

        LOG_INFOR("srt cleanup");
        st = srt_cleanup();
    }
    return st;
}

/**
 * @brief thêm các tùy chọn SRT
 *
 */
static int srt_set_options(srt_context_s *ctx)
{
    int yes = 1;
    int payloadsize = 1316;
    int64_t max_bandwidth = 5000000;

    assert(ctx != NULL);
    // if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_TRANSTYPE, &transtype, sizeof transtype))
    // {
    //     LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return SRT_PROTO_FAILED;
    // }

    // if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_MSS, &mss, sizeof mss))
    // {
    //     LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return SRT_PROTO_FAILED;
    // }

    if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_MAXBW, &max_bandwidth, sizeof max_bandwidth))
    {
        LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
        return SRT_PROTO_FAILED;
    }

    if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_PAYLOADSIZE, &payloadsize, sizeof payloadsize))
    {
        LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
        return SRT_PROTO_FAILED;
    }

    // if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_FC, &fc, sizeof fc))
    // {
    //     LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return SRT_PROTO_FAILED;
    // }
    // if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_RCVBUF, &rcvbuf, sizeof rcvbuf))
    // {
    //     LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return SRT_PROTO_FAILED;
    // }

    // LOG_INFOR("srt setsockflag\n");
    // if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_SNDSYN, &yes, sizeof yes))
    // {
    //     LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return SRT_PROTO_FAILED;
    // }

    // if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_RCVSYN, &yes, sizeof yes))
    // {
    //     LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return SRT_PROTO_FAILED;
    // }

    // if (SRT_ERROR == srt_setsockflag(ctx->srt_socket, SRTO_LATENCY, &latency, sizeof latency))
    // {
    //     LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return SRT_PROTO_FAILED;
    // }

    return SRT_PROTO_OK;
}

static int init_srt_proto(srt_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_ERROR("ctx NULL");
        return SRT_PROTO_FAILED;
    }

    // if (ctx->srt_mode == SRT_MODE_RENDERVOUS || ctx->srt_mode == SRT_MODE_CALLER ||
    //     ctx->srt_mode == SRT_MODE_LISTENER)
    // {
    //     LOG_ERROR("srt mode error");
    //     return SRT_PROTO_FAILED;
    // }

    if (ctx->udp_socket == -1)
    {
        LOG_INFOR("SRT Using bind mode");
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
        LOG_INFOR("Change srt to \"accepted\" state, num active client: %d", ctx->num_active_client);
        ctx->srt_state = SRT_STATE_ACCEPTED;
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
        st = start_listen_thread(ctx);
        assert(st == SRT_PROTO_OK);
    }
    return SRT_PROTO_OK;
}

int change_srt_to_idle_state(srt_context_s *ctx)
{
    if (ctx == NULL)
    {
        ctx = get_srt_instance();
    }
    else
    {
        if (ctx->srt_state != SRT_STATE_IDLE)
        {

            // teminate listen thread
            if (ctx->listen_thread != NULL)
            {
                LOG_INFOR("Teminate srt listen thread");
                pthread_join(ctx->listen_thread, NULL);
                pthread_cancel(ctx->listen_thread);
            }

            stop_srt_proto(ctx);
        }
    }
    pthread_mutex_lock(&ctx->lock);
    ctx->listen_thread = NULL;
    ctx->srt_socket = SRT_INVALID_SOCK;
    ctx->srt_state = SRT_STATE_IDLE;
    ctx->num_active_client = 0;
    for (int i = 0; i < MAX_SRT_CLIENT; i++)
    {
        ctx->their_fd[i] = SRT_INVALID_SOCK;
        ctx->isFirstSend[i] = 1;
    }
    ctx->their_fd_index = 0;
    pthread_mutex_unlock(&ctx->lock);
    return SRT_PROTO_OK;
}

int change_srt_to_listen_state(srt_context_s *ctx)
{
    change_srt_to_idle_state(ctx);
    return init_srt_proto(ctx);
}

void destroy_srt_proto(srt_context_s *ctx)
{
    if (ctx != NULL)
    {

        if (ctx->srt_state != SRT_STATE_IDLE)
        {
            stop_srt_proto(ctx);
        }

        if (ctx->srt_snd_buf != NULL)
        {
            free(ctx->srt_snd_buf);
            ctx->srt_snd_buf = NULL;
        }

        if (ctx->ip_address != NULL)
        {
            free(ctx->ip_address);
            ctx->ip_address = NULL;
        }
        free(ctx);
        ctx = NULL;
    }
}

int set_srt_ipaddress(srt_context ctx, char *ipaddress, int ipaddress_len)
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
            LOG_ERROR("Could not set srt ipaddress, ipaddress field is NULL");
            return SRT_PROTO_FAILED;
        }
    }
    else
    {
        LOG_ERROR("Could not set srt ipaddress, you have to get srt instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}

int get_srt_ipaddress(srt_context ctx, char **ipaddress, int *ipaddress_len)
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
            LOG_ERROR("Srt ipaddress is not yet set");
            return SRT_PROTO_FAILED;
        }
    }
    else
    {
        LOG_ERROR("Could not get srt ipaddress, you have to get srt instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}

int set_srt_ipport(srt_context ctx, int ipport)
{
    if (ctx != NULL)
    {
        if (ipport <= MIN_PORT || ipport >= MAX_PORT)
        {
            LOG_ERROR("ip port is <= %d or >= %d", MIN_PORT, MAX_PORT);
            return SRT_PROTO_FAILED;
        }
        LOG_INFOR("Set srt ip port to: %d", ipport);
        ctx->ip_port = ipport;
    }
    else
    {
        LOG_ERROR("Could not get srt ipport, you have to get srt instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}

int set_srt_udp_socket(srt_context ctx, int udp_socket)
{
    if (ctx != NULL)
    {
        LOG_INFOR("Set srt udp socket to: %d", udp_socket);
        ctx->udp_socket = udp_socket;
    }
    else
    {
        LOG_ERROR("Could not get srt ipport, you have to get srt instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}

int get_srt_iport(srt_context ctx, int *ipport)
{
    if (ctx != NULL)
    {
        *ipport = ctx->ip_port;
    }
    else
    {
        LOG_ERROR("Could not get srt ipport, you have to get srt instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}

int set_srt_mode(srt_context ctx, srt_mode_e srt_mode)
{
    if (ctx != NULL)
    {
        if (srt_mode == SRT_MODE_CALLER || srt_mode == SRT_MODE_LISTENER || srt_mode == SRT_MODE_RENDERVOUS)
        {
            ctx->srt_mode = srt_mode;
            LOG_INFOR("SRT mode is set to : %s%s%s", (srt_mode == SRT_MODE_CALLER ? "Caller" : ""),
                      (srt_mode == SRT_MODE_LISTENER ? "Listener" : ""),
                      (srt_mode == SRT_MODE_RENDERVOUS ? "Rendervous" : ""));
        }
        else
        {
            LOG_ERROR("SRT set mode failed, srt_mode is not correct");
            return SRT_PROTO_FAILED;
        }
    }
    else
    {
        LOG_ERROR("Could not set srt mode, you have to get srt instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}
int get_srt_mode(srt_context ctx, srt_mode_e *srt_mode)
{
    if (ctx != NULL)
    {
        *srt_mode = ctx->srt_mode;
    }
    else
    {
        LOG_ERROR("Could not get srt mode, you have to get srt instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}

int get_srt_state(srt_context ctx, srt_state_e *srt_state)
{
    if (ctx != NULL)
    {
        *srt_state = ctx->srt_state;
    }
    else
    {
        LOG_ERROR("Could not get srt state, you have to get srt instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}

int srt_send_from_snd_buffer(srt_context ctx, media_frame_e frame_type)
{
    int st = 0;

    if (ctx != NULL)
    {
        if (ctx->has_data_to_send)
        {
            if (ctx->srt_state == SRT_STATE_ACCEPTED || ctx->srt_state == SRT_STATE_CLIENT_MAX || ctx->srt_state == SRT_STATE_CLIENT_INCREASE || ctx->srt_state == SRT_STATE_CLIENT_DECREASE)
            {
                if (ctx->srt_mode == SRT_MODE_CALLER)
                {
                    if (ctx->srt_socket != SRT_INVALID_SOCK)
                    {
                        st = srt_sendmsg2(ctx->srt_socket, ctx->srt_snd_buf, ctx->srt_snd_data_len, NULL);
                        if (st == SRT_ERROR)
                        {
                            fprintf(stderr, "srt_sendmsg: %s\n", srt_getlasterror_str());
                            ctx->srt_socket = SRT_INVALID_SOCK;
                            ctx->srt_state = SRT_STATE_DISCONNECTED;
                        }
                    }
                }
                else if (ctx->srt_mode == SRT_MODE_LISTENER)
                {
                    int active_client = ctx->num_active_client;
                    if (active_client == 0)
                    {
                        LOG_INFOR("Change srt to \"listen\" state.");
                        ctx->srt_state = SRT_STATE_LISTEN;
                    }
                    int i = 0;
                    while (active_client)
                    {
                        if (ctx->their_fd[i] != SRT_INVALID_SOCK)
                        {
                            // LOG_INFOR(">>>>>>>>>>>>>>>>> a1");
                            // /* If is first send: we will send SPS/PPS frame */
                            // if (ctx->isFirstSend[i] == 1 && (frame_type == H264_I_FRAME || frame_type == H264_P_FRAME || frame_type == AAC_FRAME))
                            // {
                            //     LOG_INFOR(">>>>>>>>>>>>>>>>> a2");
                            //     i++;
                            //     active_client--;
                            //     continue;
                            // }
                            // /** we will not send PPS/SPS frame when send to client 3 times*/
                            // if (ctx->isFirstSend[i] == 0 && (frame_type == H264_PPS_FRAME || frame_type == H264_SPS_FRAME))
                            // {
                            //     LOG_INFOR(">>>>>>>>>>>>>>>>> a3");
                            //     active_client--;
                            //     i++;
                            //     continue;
                            // }
                            // LOG_INFOR(">>>>>>>>>>>>>>>>> a4");
                            st = srt_sendmsg2(ctx->their_fd[i], ctx->srt_snd_buf, ctx->srt_snd_data_len, NULL);
                            active_client--;
                            if (st == SRT_ERROR)
                            {
                                fprintf(stderr, "srt_sendmsg: %s\n", srt_getlasterror_str());
                                ctx->their_fd[i] = SRT_INVALID_SOCK;
                                ctx->num_active_client = ctx->num_active_client - 1;
                                if (ctx->num_active_client == 1)
                                {
                                    LOG_INFOR("Change srt to \"accepted\" state, num active client: %d.", ctx->num_active_client);
                                    ctx->srt_state = SRT_STATE_ACCEPTED;
                                }
                                else
                                {
                                    LOG_INFOR("Change srt to \"decrease\" state, num active client: %d.", ctx->num_active_client);
                                    ctx->srt_state = SRT_STATE_CLIENT_DECREASE;
                                }
                                if (ctx->srt_state == SRT_STATE_CLIENT_MAX)
                                {
                                    start_listen_thread(ctx);
                                }
                            }

                            // if (ctx->isFirstSend[i] > 0)
                            // {
                            //     if (frame_type == H264_PPS_FRAME || frame_type == H264_SPS_FRAME)
                            //     {
                            //         ctx->isFirstSend[i]++;
                            //         LOG_INFOR(">>>>>>>>>>>>>>> ctx->isFirsrtSend[%d] = %d", i, ctx->isFirstSend[i]);
                            //     }
                            //     if (ctx->isFirstSend[i] == 10)
                            //     {
                            //         ctx->isFirstSend[i] = 0;
                            //     }
                            // }
                        }
                        i++;
                    }
                }
            }
            else
            {
                LOG_ERROR("You are not in valid state to able to send buffer: %s%s%s%s%s%s%s", (ctx->srt_state == SRT_STATE_IDLE ? "IDLE" : ""),
                          (ctx->srt_state == SRT_STATE_LISTEN ? "Listen" : ""), (ctx->srt_state == SRT_STATE_DISCONNECTED ? "Disconnect" : ""),
                          (ctx->srt_state == SRT_STATE_ACCEPTED ? "Accept" : ""), (ctx->srt_state == SRT_STATE_CLIENT_DECREASE ? "Decrease" : ""),
                          (ctx->srt_state == SRT_STATE_CLIENT_INCREASE ? "Increase" : ""), (ctx->srt_state == SRT_STATE_CLIENT_MAX ? "Max_client" : ""));
                return SRT_PROTO_FAILED;
            }
        }
        memset(ctx->srt_snd_buf, 0, SRT_PAYLOAD_SIZE);
        ctx->has_data_to_send = 0;
    }
    else
    {
        LOG_ERROR("Failed to send srt buffer, you have to get instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}

int srt_send_buffer(srt_context ctx, char *buffer, int buffer_len, media_frame_e frame_type)
{
    int len = buffer_len;
    if (len >= SRT_PAYLOAD_SIZE)
    {
        while (1)
        {
            memset(srt_ctx->srt_snd_buf, 0, SRT_PAYLOAD_SIZE);
            memcpy(srt_ctx->srt_snd_buf, buffer, SRT_PAYLOAD_SIZE);
            len -= SRT_PAYLOAD_SIZE;
            ctx->has_data_to_send = 1;
            ctx->srt_snd_data_len = SRT_PAYLOAD_SIZE;
            if (srt_send_from_snd_buffer(ctx, frame_type) == SRT_PROTO_FAILED)
            {
                return SRT_PROTO_FAILED;
            }

            if (len < SRT_PAYLOAD_SIZE)
            {
                break;
            }
        }
    }

    if (len < SRT_PAYLOAD_SIZE && len != 0)
    {
        ctx->has_data_to_send = 1;
        ctx->srt_snd_data_len = len;
        memset(srt_ctx->srt_snd_buf, 0, SRT_PAYLOAD_SIZE);
        memcpy(srt_ctx->srt_snd_buf, buffer, len);
        if (srt_send_from_snd_buffer(ctx, frame_type) == SRT_PROTO_FAILED)
        {
            return SRT_PROTO_FAILED;
        }
    }

    return SRT_PROTO_OK;
}

int srt_receive_data_from_primary_client(srt_context ctx, char **buffer, int *buffer_len)
{
    int primary_client_socket = SRT_INVALID_SOCK;
    int ret = 0;
    if (ctx != NULL)
    {
        if (ctx->srt_state == SRT_STATE_ACCEPTED || ctx->srt_state == SRT_STATE_CLIENT_DECREASE || ctx->srt_state == SRT_STATE_CLIENT_INCREASE || ctx->srt_state == SRT_STATE_CLIENT_MAX)
        {
            if (ctx->srt_mode == SRT_MODE_LISTENER)
            {
                for (int i = 0; i < MAX_SRT_CLIENT; i++)
                {
                    if (ctx->their_fd[i] != SRT_INVALID_SOCK)
                    {
                        primary_client_socket = ctx->their_fd[i];
                        break;
                    }
                }
            }
            else if (ctx->srt_mode == SRT_MODE_CALLER)
            {
                primary_client_socket = ctx->srt_socket;
            }

            if (primary_client_socket != SRT_INVALID_SOCK)
            {
                ret = srt_recvmsg2(primary_client_socket, *buffer, SRT_PAYLOAD_SIZE, NULL);
                // LOG_INFOR("SRT RECV: %d", ret);
                if (ret == SRT_ERROR)
                {
                    LOG_ERROR("srt_recv: %s", srt_getlasterror_str());
                    return SRT_PROTO_FAILED;
                }
                *buffer_len = ret;
            }
            else
            {
                LOG_ERROR("srt_recv: no client connected");
                return SRT_PROTO_FAILED;
            }
        }
    }
    else
    {
        LOG_ERROR("Failed to receive srt buffer, you have to get instance first");
        return SRT_PROTO_FAILED;
    }
    return SRT_PROTO_OK;
}