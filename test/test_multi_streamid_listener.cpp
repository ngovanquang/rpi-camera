/***
 *  @name Test từ 1 listener trên một udp socket stream đến nhiều caller, các caller vs streamid khác nhau, tương ứng với dữ liệu khác nhau. 
 *
 *  Kịch bản: (VLC caller 1 - streamid01) <---- (UDP port máy nhận 1) ---- (UDP port 5000) ---- (SRT listener)
 *            (VLC caller 2 - streamid02) <---- (UDP port máy nhận 2) ---- (UDP port 5000) ---- (SRT listener)
 *  - Kết nối SRT qua UDP socket đó
 *
 *  Sử dụng trên PC or RPI
 *
 */

#include "logger.h"
#include "srt/srt.h"
#include "srt/access_control.h"
#include <stdio.h>
#include <assert.h>
#include <map>

#define FAILED (-1)
#define OK (0)
#define MAX_PORT (55000)
#define MIN_PORT (1000)
#define MAX_SRT_CLIENT (5)

#define MEDIA_PATH ("../out/")
#define MAX_STREAMID (2)

pthread_mutex_t mutex;

// database[stream_id][file]
const char *database[MAX_STREAMID][2] = {
    {"stream_01", "test1.ts"},
    {"stream_02", "test2.ts"}};

static FILE *vfp;
static FILE *afp;
// ts_demuxer_t *ts;

char send_buffer[1316];
int buffer_len = 0;

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
    const char *ip_address;
    const char *stream_id;
    const char *ts_file;
    pthread_t accept_thread;
    int ip_port;
    int srt_state;
    // pthread_mutex_t srt_mutex;
} srt_context_s;

/**
 * @brief constructer khởi tạo 1 struct srt.
 *
 */
srt_context_s *allocate_srt_context()
{
    srt_context_s *cxt = (srt_context_s *)malloc(sizeof(srt_context_s));

    if (cxt == NULL)
    {
        LOG_ERROR("Memory allocation for SRT failed.\n");
        return cxt;
    }
    cxt->ts_file = NULL;
    cxt->udp_socket = -1;
    cxt->srt_state = NONE;
    cxt->srt_socket = SRT_INVALID_SOCK;
    for (int i = 0; i < MAX_SRT_CLIENT; ++i)
    {
        cxt->their_fd[i] = SRT_INVALID_SOCK;
    }

    return cxt;
}

/**
 * @brief  destructer hủy struct srt
 *
 */
void deallocate_srt_context(srt_context_s *cxt)
{
    if (cxt == NULL)
    {
        LOG_ERROR("cxt NULL");
    }
    free(cxt);
}

/**
 * @brief thêm các tùy chọn SRT
 *
 */
int srt_set_options(srt_context_s *cxt)
{
    int yes = 1;

    if (cxt == NULL)
    {
        LOG_ERROR("cxt NULL");
        return FAILED;
    }

    LOG_INFOR("srt setsockflag\n");
    if (SRT_ERROR == srt_setsockflag(cxt->srt_socket, SRTO_SNDSYN, &yes, sizeof yes))
    {
        LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
        return FAILED;
    }

    if (SRT_ERROR == srt_setsockflag(cxt->srt_socket, SRTO_RCVSYN, &yes, sizeof yes))
    {
        LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
        return FAILED;
    }
    return OK;
}

/**
 * @brief tạo một socket UDP
 *
 */
int create_udp_socket(srt_context_s *cxt)
{
    if (cxt == NULL)
    {
        LOG_ERROR("cxt NULL");
        return FAILED;
    }

    if (cxt->ip_address == NULL || cxt->ip_port <= MIN_PORT || cxt->ip_port >= MAX_PORT)
    {
        LOG_ERROR("ip address or port is <= %d or >= %d", MIN_PORT, MAX_PORT);
        return FAILED;
    }
    if (cxt->srt_mode > SRT_MODE_RE || cxt->srt_mode < SRT_MODE_CALLER)
    {
        LOG_ERROR("srt mode error");
        return FAILED;
    }
    int udp_sockfd;
    struct sockaddr_in server_addr, client_addr;

    // Create UDP socket on server side
    if (cxt->srt_mode == SRT_MODE_LISTENER)
    {
        // Create UDP socket
        udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_sockfd < 0)
        {
            LOG_ERROR("Failed to create UDP socket");
            return FAILED;
        }

        // Configure server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(cxt->ip_port);

        // Bind socket to the specified address and port
        if (bind(udp_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            LOG_ERROR("UDP bind failed");
            return FAILED;
        }

        LOG_INFOR("UDP server listening on port %d...\n", cxt->ip_port);

        socklen_t client_len = sizeof(client_addr);

        // Receive message from client

        char buffer[100];
        size_t recv_len = recvfrom(udp_sockfd, buffer, 100, 0,
                                   (struct sockaddr *)&client_addr, &client_len);
        if (recv_len < 0)
        {
            LOG_ERROR("Recvfrom failed");
            return FAILED;
        }

        // Print received message
        LOG_INFOR("Received udp message from %s:%d\n", inet_ntoa(client_addr.sin_addr),
                  ntohs(client_addr.sin_port));
        LOG_INFOR("Message: %s", buffer);
    }
    // Create UDP socket on Client side
    else if (cxt->srt_mode == SRT_MODE_CALLER)
    {

        // Create UDP socket
        udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_sockfd < 0)
        {
            LOG_ERROR("Failed to create socket");
            return FAILED;
        }

        // Configure server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(cxt->ip_address); // Replace with desired remote IP address
        server_addr.sin_port = htons(cxt->ip_port);               // Replace with desired remote port

        // Connect socket to the remote address and port
        if (connect(udp_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            LOG_ERROR("Connect failed");
            return FAILED;
        }

        LOG_INFOR("Connected to server %s:%d\n", inet_ntoa(server_addr.sin_addr),
                  ntohs(server_addr.sin_port));

        // Send data to the server
        const char *message = "Hello, server!";
        ssize_t send_len = send(udp_sockfd, message, strlen(message), 0);
        if (send_len < 0)
        {
            LOG_ERROR("Send failed");
            return FAILED;
        }
    }

    return udp_sockfd;
}

/**
 * @brief khởi tạo một SRT socket sử dụng UDP socket có từ trước
 *
 * @param cxt
 * @return srt socket
 */

int SrtTestListenCallback(void *opaq, SRTSOCKET ns, int hsversion, const struct sockaddr *peeraddr, const char *streamid)
{
    using namespace std;

    srt_context_s *cxt = (srt_context_s *)opaq;
    if (cxt == NULL)
    {
        LOG_ERROR("opaq is not NULL");
        return FAILED; // enforce EXPECT to fail
    }

    if (hsversion != 5)
    {
        LOG_ERROR("hsversion expected 5");
        return FAILED;
    }

    if (!peeraddr)
    {
        // XXX Might be better to check the content, too.
        LOG_ERROR("null peeraddr");
        return FAILED;
    }

    // find streamid in database
    for (int i = 0; i < MAX_STREAMID; ++i)
    {
        if (strcmp(database[i][0], streamid) == 0)
        {
            LOG_INFOR("Connect to client has streamid = %s", streamid);
            cxt->ts_file = database[i][1];
            return OK;
        }
    }

    srt_setrejectreason(ns, SRT_REJX_BAD_REQUEST);
    LOG_ERROR("TEST: Invalid Stream ID, returning false.");
    return FAILED;
}

void *send_ts_handle(void *cxt)
{
    srt_context_s *srt_cxt = (srt_context_s *)cxt;
    while (1)
    {
        if (srt_cxt->srt_state == ACCEPTED)
        {
            break;
        }
        usleep(20000);
    }
    char buffer[1316] = {0};
    if (srt_cxt->ts_file == NULL)
    {
        LOG_ERROR("file ts is NULL");
        return NULL;
    }
    char file_path[1000] = {0};
    sprintf(file_path, "%s%s", MEDIA_PATH, srt_cxt->ts_file);
    LOG_INFOR("Media file path: %s", file_path);
    FILE *fp = fopen(file_path, "rb");
    int st;
    int ret;
    int fd_index = srt_cxt->their_fd_index;
    int srt_socket = srt_cxt->their_fd[srt_cxt->their_fd_index];

    while (!feof(fp))
    {
        ret = fread(buffer, 1, 1316, fp);

        if (srt_socket != SRT_INVALID_SOCK)
        {
            // pthread_mutex_lock(&mutex);
            st = srt_sendmsg2(srt_socket, buffer, ret, NULL);
            // pthread_mutex_unlock(&mutex);
            if (st == SRT_ERROR)
            {
                LOG_ERROR("srt_sendmsg: %s, socket %d\n", srt_getlasterror_str(), srt_socket);
                srt_cxt->their_fd[fd_index] = SRT_INVALID_SOCK;
                break;
            }
        }

        // LOG_INFOR("SRT SEND: %d bytes\n", buffer_len);
        memset(buffer, 0, sizeof(buffer));
        usleep(2000);
    }

    // khoi phuc bien moi truong
    srt_cxt->their_fd[fd_index] = SRT_INVALID_SOCK;
    srt_cxt->ts_file = NULL;
    srt_cxt->accept_thread = NONE;

    fclose(fp);

    pthread_exit(NULL);
}

void *srt_accept_loop(void *cxt)
{
    // Setup EID in order to pick up either readiness or error.
    // THis is only to make a formal response side, nothing here is to be tested.
    srt_context_s *srt_cxt = (srt_context_s *)cxt;
    int eid = srt_epoll_create();

    // Subscribe to R | E
    int re = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    srt_epoll_add_usock(eid, srt_cxt->srt_socket, &re);

    SRT_EPOLL_EVENT results[2];

    for (;;)
    {
        auto state = srt_getsockstate(srt_cxt->srt_socket);
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
                SRTSOCKET acp = srt_accept(srt_cxt->srt_socket, NULL, NULL);
                if (acp == SRT_INVALID_SOCK)
                {
                    LOG_ERROR("[T] Accept failed, so exitting");
                    break;
                }

                // add valid client_fd into array
                for (int i = 0; i < MAX_SRT_CLIENT; ++i)
                {

                    if (srt_cxt->their_fd[i] == SRT_INVALID_SOCK)
                    {
                        srt_cxt->their_fd[i] = acp;
                        srt_cxt->srt_state = ACCEPTED;
                        srt_cxt->their_fd_index = i;
                        pthread_t send_file_thread;
                        int st;
                        st = pthread_create(&send_file_thread, NULL, send_ts_handle, (void *)srt_cxt);
                        if (st)
                        {
                            LOG_ERROR("Error creating thread for receiver: %d\n", st);
                            break;
                        }
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

int init_srt(srt_context_s *cxt)
{
    if (cxt == NULL)
    {
        LOG_ERROR("cxt NULL");
        return FAILED;
    }
    if (cxt->ip_address == NULL || cxt->ip_port <= MIN_PORT || cxt->ip_port >= MAX_PORT)
    {
        LOG_ERROR("ip address or port is <= %d or >= %d", MIN_PORT, MAX_PORT);
        return FAILED;
    }

    if (cxt->srt_mode > SRT_MODE_RE || cxt->srt_mode < SRT_MODE_CALLER)
    {
        LOG_ERROR("srt mode error");
        return FAILED;
    }

    if (cxt->udp_socket == -1)
    {
        LOG_ERROR("udp socket is invalid!");
        return FAILED;
    }

    /**Start SRT*/
    int st;
    struct sockaddr_storage their_addr;

    LOG_INFOR("srt startup\n");
    srt_startup();

    LOG_INFOR("srt socket\n");
    cxt->srt_socket = srt_create_socket();
    if (cxt->srt_socket == SRT_ERROR)
    {
        LOG_ERROR("srt_socket: %s\n", srt_getlasterror_str());
        return FAILED;
    }

    LOG_INFOR("srt remote address\n");
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(cxt->ip_port);
    if (inet_pton(AF_INET, cxt->ip_address, &sa.sin_addr) != 1)
    {
        LOG_ERROR("inet_pton failed");
        return FAILED;
    }

    LOG_INFOR("SRT bind\n");
    // st = srt_bind(cxt->srt_socket, (struct sockaddr *)&sa, sizeof sa);
    st = srt_bind_acquire(cxt->srt_socket, cxt->udp_socket);
    if (st == SRT_ERROR)
    {
        LOG_ERROR("srt_bind: %s\n", srt_getlasterror_str());
        return FAILED;
    }

    // Set srt options
    if (srt_set_options(cxt) == FAILED)
    {
        return FAILED;
    }

    if (cxt->srt_mode == SRT_MODE_CALLER)
    {

        LOG_INFOR("srt connect");
        st = srt_connect(cxt->srt_socket, (struct sockaddr *)&sa, sizeof sa);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_connect: %s\n", srt_getlasterror_str());
            return FAILED;
        }
    }
    else if (cxt->srt_mode == SRT_MODE_LISTENER)
    {
        LOG_INFOR("SRT listen");
        st = srt_listen(cxt->srt_socket, 2);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_listen: %s\n", srt_getlasterror_str());
            return FAILED;
        }

        // Listener Callback
        srt_listen_callback(cxt->srt_socket, &SrtTestListenCallback, (void *)cxt);

        // Accept thread
        st = pthread_create(&cxt->accept_thread, NULL, srt_accept_loop, (void *)cxt);
        if (st)
        {
            LOG_ERROR("Error creating thread for accept: %d\n", st);
            return FAILED;
        }
    }

    return OK;
}

int destroy_srt(srt_context_s *cxt)
{
    if (cxt == NULL)
    {
        LOG_ERROR("cxt is NULL");
        return FAILED;
    }

    LOG_INFOR("srt close\n");
    int st;
    st = srt_close(cxt->srt_socket);
    if (st == SRT_ERROR)
    {
        LOG_ERROR("srt_close: %s\n", srt_getlasterror_str());
        return FAILED;
    }

    LOG_INFOR("srt cleanup");
    srt_cleanup();
    deallocate_srt_context(cxt);

    return OK;
}

int main(int argc, char const *argv[])
{
    int st;
    if (argc != 3)
    {
        LOG_ERROR("Usage: %s <host> <port>\n", argv[0]);
        return FAILED;
    }
    // pthread_mutex_init(&mutex, NULL);

    /**Create UDP socket*/
    srt_context_s *temp = allocate_srt_context();
    temp->ip_address = argv[1];
    temp->ip_port = atoi(argv[2]);
    temp->srt_mode = SRT_MODE_LISTENER;
    int udp_socket = create_udp_socket(temp);
    if (udp_socket == FAILED)
    {
        return FAILED;
    }

    srt_context_s *listener = allocate_srt_context();

    listener->ip_address = argv[1];
    listener->ip_port = atoi(argv[2]);
    listener->srt_mode = SRT_MODE_LISTENER;
    listener->udp_socket = udp_socket;

    // init srt
    if (FAILED == init_srt(listener))
    {
        return FAILED;
    }

    while (1)
    {
        usleep(1000000);
    }

end:
    destroy_srt(listener);
    // pthread_mutex_destroy(&mutex);
    return 0;
}