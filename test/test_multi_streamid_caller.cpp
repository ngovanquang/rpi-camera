/***
 *  @name Test nhiều caller truyền qua một UDP port, stream đến các listener khác nhau, truyền đi cùng 1 dữ liệu
 *
 *  Kịch bản: (VLC listener 1) <---- (UDP port máy nhận 1) ---- (UDP port 5000) ---- (SRT caller 1)
 *            (VLC listener 2) <---- (UDP port máy nhận 2) ---- (UDP port 5000) ---- (SRT caller 2)
 *            (VLC listener 2) <---- (UDP port máy nhận 2) ---- (UDP port 5000) ---- (SRT caller 3)
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
#define MAX_STREAMID (3)

// database[stream_id][file]
const char *database[MAX_STREAMID][2] = {
    {"stream_01", "test1.ts"},
    {"stream_02", "test2.ts"},
    {"stream_03", "test2.ts"}};

int32_t sockfds[10] = {-1};
unsigned int active_srt_socks = 0;

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
    int ip_port;
    const char *stream_id;
    int srt_state;
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
    int timeout = 10000;
    if (SRT_ERROR == srt_setsockflag(cxt->srt_socket, SRTO_CONNTIMEO, &timeout, sizeof timeout))
    {
        LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
        return FAILED;
    }

    // if (SRT_ERROR == srt_setsockflag(cxt->srt_socket, SRTO_STREAMID, cxt->stream_id, strlen(cxt->stream_id)))
    // {
    //     LOG_ERROR("srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return FAILED;
    // }

    
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

    int udp_sockfd;
    struct sockaddr_in server_addr, client_addr;

    // Create UDP socket on server side
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
    server_addr.sin_port = htons(5000);

    // Bind socket to the specified address and port
    if (bind(udp_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        LOG_ERROR("UDP bind failed");
        return FAILED;
    }

    LOG_INFOR("UDP server listening on port %d...\n", 5000);

    return udp_sockfd;
}

/**
 * @brief khởi tạo một SRT socket sử dụng UDP socket có từ trước
 *
 * @param cxt
 * @return srt socket
 */

void *send_ts_handle(void *param)
{
    while (1)
    {
        if (active_srt_socks > 0)
        {
            break;
        }
        usleep(20000);
    }
    char buffer[1316] = {0};
    char file_path[1000] = {0};
    sprintf(file_path, "%s%s", MEDIA_PATH, "test1.ts");
    LOG_INFOR("Media file path: %s", file_path);
    FILE *fp = fopen(file_path, "rb");
    int st;
    int ret;
    int index = 0;

    while (!feof(fp))
    {
        ret = fread(buffer, 1, 1316, fp);
        for (int i = 0; i < 10; i++)
        {
            if (sockfds[i] != SRT_INVALID_SOCK)
            {
                // pthread_mutex_lock(&mutex);
                st = srt_sendmsg2(sockfds[i], buffer, ret, NULL);
                // pthread_mutex_unlock(&mutex);
                if (st == SRT_ERROR)
                {
                    LOG_ERROR("srt_sendmsg: %s, socket %d\n", srt_getlasterror_str(), sockfds[i]);
                    sockfds[i] = SRT_INVALID_SOCK;
                    active_srt_socks--;
                    continue;
                }
            }
        }

        // LOG_INFOR("SRT SEND: %d bytes\n", buffer_len);
        memset(buffer, 0, sizeof(buffer));
        usleep(2000);
    }

    fclose(fp);

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
    printf("NGOVANQUANG_01\n");
    LOG_INFOR("srt connect");
    st = srt_connect(cxt->srt_socket, (struct sockaddr *)&sa, sizeof sa);
    if (st == SRT_ERROR)
    {
        LOG_ERROR("srt_connect: %s\n", srt_getlasterror_str());
        return FAILED;
    }
    printf("NGOVANQUANG_01\n");
    active_srt_socks++;
    for (size_t i = 0; i < 10; i++)
    {
        if (sockfds[i] == SRT_INVALID_SOCK)
        {
            sockfds[i] = cxt->srt_socket;
            break;
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
    if (argc != 7)
    {
        LOG_ERROR("Usage: %s <host1> <port1> <host2> <port2> <host3> <port3>\n", argv[0]);
        return FAILED;
    }

    for (int i = 0; i < 10; ++i)
    {
        sockfds[i] = SRT_INVALID_SOCK;
    }

    /**Create UDP socket*/
    srt_context_s *temp = allocate_srt_context();
    int udp_socket = create_udp_socket(temp);
    if (udp_socket == FAILED)
    {
        return FAILED;
    }

    srt_context_s *caller1 = allocate_srt_context();
    srt_context_s *caller2 = allocate_srt_context();
    srt_context_s *caller3 = allocate_srt_context();

    caller1->ip_address = argv[1];
    caller1->ip_port = atoi(argv[2]);
    caller1->srt_mode = SRT_MODE_CALLER;
    caller1->udp_socket = udp_socket;
    caller1->stream_id = database[0][0];

    caller2->ip_address = argv[3];
    caller2->ip_port = atoi(argv[4]);
    caller2->srt_mode = SRT_MODE_CALLER;
    caller2->udp_socket = udp_socket;
    caller2->stream_id = database[1][0];

    caller3->ip_address = argv[5];
    caller3->ip_port = atoi(argv[6]);
    caller3->srt_mode = SRT_MODE_CALLER;
    caller3->udp_socket = udp_socket;
    caller3->stream_id = database[2][0];
    // init srt
    if (FAILED == init_srt(caller1))
    {
        LOG_ERROR("can't create srt socket to: %s %d", caller1->ip_address, caller1->ip_port);
    }

    if (FAILED == init_srt(caller2))
    {
        LOG_ERROR("can't create srt socket to: %s %d", caller2->ip_address, caller2->ip_port);
    }

    if (FAILED == init_srt(caller3))
    {
        LOG_ERROR("can't create srt socket to: %s %d", caller3->ip_address, caller3->ip_port);
    }

    pthread_t send_thread = NULL;
    pthread_create(&send_thread, NULL, send_ts_handle, NULL);

    while (1)
    {
        usleep(1000000);
    }

end:
    destroy_srt(caller1);
    destroy_srt(caller2);
    destroy_srt(caller3);
    // pthread_mutex_destroy(&mutex);
    return 0;
}