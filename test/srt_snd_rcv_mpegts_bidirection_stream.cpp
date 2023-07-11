/***
 *  @name Test chế độ truyền nhận 2 chiều trên SRT
 *  
 *           --.ts--> <May 1> <=--SRT--=> <May 2> -- .h264, .aac --
 *  -- .h264 .aac --> <May 1> <=--SRT--=> <May 2> -- .ts --
 * 
 * - Máy 1 gửi đi file .ts, Máy 2 nhận được và giải mã ra .h264 và .aac
 * - Máy 1 nhận được file .ts từ máy 2 và giải mã ra .h264 .aac
 * 
 * SỬ dụng trên RPI or PC 
*/

#include "mpegts/mpeg-ps.h"
#include "mpegts/mpeg-ts.h"
#include "mpegts/mpeg-types.h"
#include "flv/mpeg4-aac.h"
#include "logger.h"
#include "srt/srt.h"
#include <stdio.h>
#include <assert.h>
#include <map>

#define FAILED (-1)
#define OK (0)
#define MAX_PORT (55000)
#define MIN_PORT (1000)
#define MAX_SRT_CLIENT (2)

static FILE *vfp;
static FILE *afp;
ts_demuxer_t *ts;

char send_buffer[1316];
int buffer_len = 0;

typedef enum
{
    SRT_MODE_CALLER = 0,
    SRT_MODE_LISTENER,
    SRT_MODE_RE
} srt_mode_e;

typedef struct srt_context
{
    int srt_socket;
    int srt_mode;
    int their_fd[MAX_SRT_CLIENT];
    const char *ip_address;
    int ip_port;
    pthread_mutex_t srt_mutex;
} srt_context_s;

srt_context_s *allocate_srt_context()
{
    srt_context_s *cxt = (srt_context_s *)malloc(sizeof(srt_context_s));

    if (cxt == NULL)
    {
        LOG_ERROR("Memory allocation for SRT failed.\n");
        return cxt;
    }

    cxt->srt_socket = SRT_INVALID_SOCK;
    for (int i = 0; i < MAX_SRT_CLIENT; ++i)
    {
        cxt->their_fd[i] = SRT_INVALID_SOCK;
    }

    return cxt;
}

void deallocate_srt_context(srt_context_s *cxt)
{
    if (cxt == NULL)
    {
        LOG_ERROR("cxt NULL");
    }
    free(cxt);
}

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
    printf("cxt->srt_mode: %d\n", cxt->srt_mode);
    if (cxt->srt_mode > SRT_MODE_RE || cxt->srt_mode < SRT_MODE_CALLER)
    {
        LOG_ERROR("srt mode error");
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
        LOG_INFOR("SRT bind\n");
        st = srt_bind(cxt->srt_socket, (struct sockaddr *)&sa, sizeof sa);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_bind: %s\n", srt_getlasterror_str());
            return FAILED;
        }

        LOG_INFOR("SRT listen");
        st = srt_listen(cxt->srt_socket, 2);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_listen: %s\n", srt_getlasterror_str());
            return FAILED;
        }

        LOG_INFOR("SRT accept");
        int addr_size = sizeof their_addr;
        int their_fd = srt_accept(cxt->srt_socket, (struct sockaddr *)&their_addr, &addr_size);
        if (their_fd == SRT_INVALID_SOCK)
        {
            LOG_ERROR("srt_accept: %s\n", srt_getlasterror_str);
            return FAILED;
        }
        for (int i = 0; i < MAX_SRT_CLIENT; ++i)
        {
            if (cxt->their_fd[i] == SRT_INVALID_SOCK)
            {
                cxt->their_fd[i] = their_fd;
                break;
            }
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

/**
 * MPEG-TS for recv
 */

inline const char *ftimestamp(int64_t t, char *buf)
{
    if (PTS_NO_VALUE == t)
    {
        sprintf(buf, "(null)");
    }
    else
    {
        t /= 90;
        sprintf(buf, "%d:%02d:%02d.%03d", (int)(t / 3600000), (int)((t / 60000) % 60), (int)((t / 1000) % 60), (int)(t % 1000));
    }
    return buf;
}

static int on_ts_packet_recv_side(void * /*param*/, int program, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void *data, size_t bytes)
{
    static char s_pts[64], s_dts[64];

    if (PSI_STREAM_AAC == avtype || PSI_STREAM_AUDIO_OPUS == avtype)
    {
        static int64_t a_pts = 0, a_dts = 0;
        if (PTS_NO_VALUE == dts)
            dts = pts;
        // assert(0 == a_dts || dts >= a_dts);
        printf("[A][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d, bytes: %u%s%s\n", program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - a_pts) / 90, (int)(dts - a_dts) / 90, (unsigned int)bytes, flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");
        a_pts = pts;
        a_dts = dts;

        fwrite(data, 1, bytes, afp);

        int count = 0;
        int len = mpeg4_aac_adts_frame_length((const uint8_t *)data, bytes);
        while (len > 7 && (size_t)len <= bytes)
        {
            count++;
            bytes -= len;
            data = (const uint8_t *)data + len;
            len = mpeg4_aac_adts_frame_length((const uint8_t *)data, bytes);
        }
    }
    else if (PSI_STREAM_H264 == avtype || PSI_STREAM_H265 == avtype)
    {
        static int64_t v_pts = 0, v_dts = 0;
        // assert(0 == v_dts || dts >= v_dts);
        printf("[V][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d, bytes: %u%s%s%s\n", program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - v_pts) / 90, (int)(dts - v_dts) / 90, (unsigned int)bytes, flags & MPEG_FLAG_IDR_FRAME ? " [I]" : "", flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");
        v_pts = pts;
        v_dts = dts;
        static int first = 0;
        if ((flags & MPEG_FLAG_PACKET_LOST) || (flags & MPEG_FLAG_PACKET_LOST))
        {
            if (first == 0)
            {
                first++;
                fwrite(data, 1, bytes, vfp);
            }
            else
            {
                LOG_INFOR("Do Nothing!");
            }
        }
        else
        {
            fwrite(data, 1, bytes, vfp);
        }
    }
    else
    {
        static int64_t x_pts = 0, x_dts = 0;
        // assert(0 == x_dts || dts >= x_dts);
        printf("[%d][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d%s%s%s\n", avtype, program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - x_pts) / 90, (int)(dts - x_dts) / 90, flags & MPEG_FLAG_IDR_FRAME ? " [I]" : "", flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");
        x_pts = pts;
        x_dts = dts;
        // assert(0);
    }
    return OK;
}

static void mpeg_ts_dec_testonstream(void *param, int stream, int codecid, const void *extra, int bytes, int finish)
{
    printf("stream %d, codecid: %d, finish: %s\n", stream, codecid, finish ? "true" : "false");
}

void mpeg_ts_dec_test(const char *buffer, int len)
{
    unsigned char ptr[188];
    assert(len % 188 == 0);
    int n = len / 188;
    int cnt = 0;
    while (cnt < n)
    {
        ts_demuxer_input(ts, (const uint8_t *)(buffer + cnt * 188), 188);
        cnt++;
    }
}

/**
 * MPEG-TS for sender
 */
static void *ts_alloc(void * /*param*/, size_t bytes)
{
    static char s_buffer[188];
    assert(bytes <= sizeof(s_buffer));
    return s_buffer;
}

static void ts_free(void * /*param*/, void * /*packet*/)
{
    return;
}

static int ts_write(void *param, const void *packet, size_t bytes)
{
    srt_context_s *cxt = (srt_context_s *)param;
    if (cxt == NULL)
    {
        return FAILED;
    }
    int srt_sock;

    if (cxt->srt_mode == SRT_MODE_CALLER)
    {
        srt_sock = cxt->srt_socket;
    }
    else if (cxt->srt_mode == SRT_MODE_LISTENER)
    {
        for (int i = 0; i < MAX_SRT_CLIENT; ++i)
        {
            if (cxt->their_fd[i] != SRT_INVALID_SOCK)
            {
                srt_sock = cxt->their_fd[i];
                break;
            }
        }
    }

    if (srt_sock == SRT_INVALID_SOCK)
    {
        return FAILED;
    }

    int st;
    
    memcpy(send_buffer + buffer_len, packet, bytes);
    buffer_len += bytes;
    if (1316 == buffer_len)
    {
        // printf("i = %d\n", i);
        // write_frame(buffer, sizeof(buffer));
        st = srt_sendmsg2(srt_sock, (char *)send_buffer, sizeof(send_buffer), NULL);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_sendmsg: %s\n", srt_getlasterror_str());

            srt_sock = SRT_INVALID_SOCK;
            
            if (cxt->srt_mode == SRT_MODE_CALLER)
            {
                cxt->srt_socket = srt_sock;
            }
            else if (cxt->srt_mode == SRT_MODE_LISTENER)
            {
                for (int i = 0; i < MAX_SRT_CLIENT; ++i)
                {
                    if (cxt->their_fd[i] != SRT_INVALID_SOCK)
                    {
                        cxt->their_fd[i] = srt_sock;
                        break;
                    }
                }
            }

            return FAILED;
        }
        LOG_INFOR("SRT SEND: %d bytes\n", buffer_len);
        memset(send_buffer, 0, sizeof(send_buffer));
        buffer_len = 0;
        usleep(1000);
        // return 0;
    }

    // usleep(500);
    return 0;
}

inline const char *ts_type(int type)
{
    switch (type)
    {
    case PSI_STREAM_MP3:
        return "MP3";
    case PSI_STREAM_AAC:
        return "AAC";
    case PSI_STREAM_H264:
        return "H264";
    case PSI_STREAM_H265:
        return "H265";
    default:
        return "*";
    }
}

static int ts_stream(void *ts, int codecid)
{
    static std::map<int, int> streams;
    std::map<int, int>::const_iterator it = streams.find(codecid);
    if (streams.end() != it)
        return it->second;

    int i = mpeg_ts_add_stream(ts, codecid, NULL, 0);
    streams[codecid] = i;
    return i;
}

static int on_ts_packet(void *ts, int program, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void *data, size_t bytes)
{
    printf("[%s] pts: %08lu, dts: %08lu%s\n", ts_type(avtype), (unsigned long)pts, (unsigned long)dts, flags ? " [I]" : "");

    return mpeg_ts_write(ts, ts_stream(ts, avtype), flags, pts, dts, data, bytes);
}

static void mpeg_ts_file(const char *file, void *muxer)
{
    unsigned char ptr[188];
    struct ts_demuxer_t *ts;
    FILE *fp = fopen(file, "rb");

    ts = ts_demuxer_create(on_ts_packet, muxer);
    while (1 == fread(ptr, sizeof(ptr), 1, fp))
    {
        ts_demuxer_input(ts, ptr, sizeof(ptr));
    }
    ts_demuxer_flush(ts);
    ts_demuxer_destroy(ts);
    fclose(fp);
}

// mpeg_ts_test("test/fileSequence0.ts", "test/apple.ts")
void send_mpegts_file(const char *input, srt_context_s *srt_cxt)
{

    struct mpeg_ts_func_t tshandler;
    tshandler.alloc = ts_alloc;
    tshandler.write = ts_write;
    tshandler.free = ts_free;

    void *ts1 = mpeg_ts_create(&tshandler, (void *)srt_cxt);

    mpeg_ts_file(input, ts1);

    mpeg_ts_destroy(ts1);
}

/**
 * handler functions
 */
// void *snd_handler(void *thread_id)
// {
//     long tid = (long)thread_id;

//     for (int i = 0; i < MAX_COUNT; i++)
//     {
//         pthread_mutex_lock(&mutex);
//         shared_variable++;
//         pthread_mutex_unlock(&mutex);
//     }

//     printf("Thread %ld finished\n", tid);
//     pthread_exit(NULL);
// }

void *recv_handler(void *srt_cxt)
{
    srt_context_s *cxt = (srt_context_s *)srt_cxt;
    if (cxt == NULL)
    {
        return NULL;
    }
    int srt_sock;

    if (cxt->srt_mode == SRT_MODE_CALLER)
    {
        srt_sock = cxt->srt_socket;
    }
    else if (cxt->srt_mode == SRT_MODE_LISTENER)
    {
        for (int i = 0; i < MAX_SRT_CLIENT; ++i)
        {
            if (cxt->their_fd[i] != SRT_INVALID_SOCK)
            {
                srt_sock = cxt->their_fd[i];
                break;
            }
        }
    }

    if (srt_sock == SRT_INVALID_SOCK)
    {
        return NULL;
    }
    char buffer[1316];
    int st;

    while (1)
    {
        printf("HELLO\n");
        // pthread_mutex_lock(&cxt->srt_mutex);
        st = srt_recvmsg(srt_sock, buffer, sizeof(buffer));
        // pthread_mutex_unlock(&cxt->srt_mutex);
        if (st == SRT_ERROR)
        {
            LOG_ERROR("srt_recvmsg: %s\n", srt_getlasterror_str());
            return NULL;
        }
        mpeg_ts_dec_test(buffer, st);
    }
}

int main(int argc, char const *argv[])
{
    int st;
    if (argc != 7)
    {
        LOG_ERROR("Usage: %s <srt_mode> <host> <port> <input_file.ts> <audio_output_file.aac> <video_output_file.h264>\n", argv[0]);
        return FAILED;
    }

    vfp = fopen(argv[6], "wb");
    afp = fopen(argv[5], "wb");

    srt_context_s *srt_cxt = allocate_srt_context();

    if (strcmp("caller", argv[1]) == 0) {
        srt_cxt->srt_mode = SRT_MODE_CALLER;
        LOG_INFOR("SRT is caller mode");
    } else if (strcmp("listener", argv[1]) == 0) {
        srt_cxt->srt_mode = SRT_MODE_LISTENER;
        LOG_INFOR("SRT is listener mode");
    }
    srt_cxt->ip_address = argv[2];
    srt_cxt->ip_port = atoi(argv[3]);


    /**
     * MPeg-ts out
     */

    struct ts_demuxer_notify_t notify = {
        mpeg_ts_dec_testonstream,
    };

    ts = ts_demuxer_create(on_ts_packet_recv_side, NULL);
    ts_demuxer_set_notify(ts, &notify, NULL);

    // init srt
    if (FAILED == init_srt(srt_cxt))
    {
        return FAILED;
    }

    /**
     * @brief Create 2 thread for send and receive
     *
     */
    // pthread_t sender_thread;
    pthread_t receiver_thread;

    // pthread_mutex_init(&srt_cxt->srt_mutex, NULL);


    st = pthread_create(&receiver_thread, NULL, recv_handler, (void *)srt_cxt);
    if (st)
    {
        LOG_ERROR("Error creating thread for receiver: %d\n", st);
        return FAILED;
    }

    // st = pthread_create(&sender_thread, NULL, snd_handler, (void *)srt_cxt);
    // if (st)
    // {
    //     LOG_ERROR("Error creating thread for receiver: %d\n", st);
    //     return FAILED;
    // }
    send_mpegts_file(argv[4], srt_cxt);

end:
    destroy_srt(srt_cxt);
    ts_demuxer_flush(ts);
    ts_demuxer_destroy(ts);
    fclose(vfp);
    fclose(afp);
    return 0;
}