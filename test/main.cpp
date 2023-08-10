#include "srt_proto.h"
#include "pcm_capture.h"
#include "aac_encode.h"
#include "h264_capture.h"
#include "mpegts/mpeg-ts.h"
#include "mpegts/mpeg-types.h"
#include "media_stream.h"
#include "media_queue.h"
#include "tcp_control.h"
#include "ptz_control.h"
#include <stdint.h>

FILE *wfp = NULL;

static srt_context srt_ctx = NULL;
static pcm_context_s *pcm_ctx = NULL;
static tcp_context tcp_ctx = NULL;
static v4l2_capture_context v4l2_ctx = NULL;
static media_frame_e frame_type;

#define ENABLE_AUDIO (0)
#define ENABLE_VIDEO (1)
#define VIDEO_FPS (20)
#define AUDIO_SAMPLE_RATE (8000)
#define PCM_FILE_TIME (10000)

#define TS_PAYLOAD_UNIT_START_INDICATOR (0x40)

char buffer[1316];
int buffer_len = 0;
int st = 0;
void *ts;
ts_demuxer_t *recv_ts;
int h264_stream;
int aac_stream;
struct timespec currentTime;
pcm_context_s *pcm_recv_ctx = NULL;

media_queue_s *media_queue = NULL;
media_queue_s *audio_queue = NULL;
media_queue_s *video_queue = NULL;
FILE *fp;

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
    srt_context srt_ctx_s = (srt_context)param;
    if (srt_ctx_s == NULL)
    {
        LOG_ERROR("SRT context is invalid");
        exit(1);
    }

    memcpy(buffer + buffer_len, packet, bytes);
    buffer_len += bytes;

    if (buffer_len == 1316)
    {
        srt_send_buffer(srt_ctx_s, buffer, buffer_len, frame_type);
        memset(buffer, 0, sizeof(buffer));
        buffer_len = 0;
    }

    return 0;
}

void pack_mpegts(const void *buffer, int size, media_frame_e media_type, uint64_t av_dts)
{
    if (srt_ctx == NULL)
        return;
    srt_state_e srt_state;
    if (get_srt_state(srt_ctx, &srt_state) == SRT_PROTO_FAILED)
        return;
    if (srt_state == SRT_STATE_IDLE || srt_state == SRT_STATE_LISTEN || srt_state == SRT_STATE_DISCONNECTED)
        return;

    static int64_t a_pts = 0;
    static int64_t a_dts = 0;
    static int64_t v_pts = 0;
    static int64_t v_dts = 0;
    static int cnt_a = 0;
    static int cnt_v = 0;
    static uint64_t a_increase = (92160000) / AUDIO_SAMPLE_RATE;
    static uint64_t v_increase = 90000 / VIDEO_FPS;

    if (ts != NULL)
    {
        if (media_type == AAC_FRAME && aac_stream != 0)
        {
            if (cnt_a == 0)
            {
                cnt_a++;
                clock_gettime(CLOCK_MONOTONIC, &currentTime);
                long milliseconds = currentTime.tv_sec * 1000 + currentTime.tv_nsec / 1000000;
                a_dts = milliseconds * 90;
            }
            else
            {
                a_dts += a_increase;
            }
            a_pts = a_dts;
            frame_type = media_type;
            int ret = mpeg_ts_write(ts, aac_stream, 0, a_pts, a_dts, buffer, size);
            if (ret != 0)
            {
                printf("AAC ERROR\n");
            }
        }
        else if (h264_stream != 0 && (media_type == H264_I_FRAME || media_type == H264_P_FRAME || media_type == H264_PPS_FRAME || media_type == H264_SPS_FRAME))
        {
            if (cnt_v == 0)
            {
                cnt_v++;
                clock_gettime(CLOCK_MONOTONIC, &currentTime);
                long milliseconds = currentTime.tv_sec * 1000 + currentTime.tv_nsec / 1000000;
                v_dts = milliseconds * 90;
            }
            else
            {
                v_dts += v_increase;
            }
            v_pts = v_dts;
            frame_type = media_type;
            int ret = mpeg_ts_write(ts, h264_stream, MPEG_FLAG_IDR_FRAME, v_pts, v_dts, buffer, size);
            if (ret != 0)
            {
                printf("H264 ERROR\n");
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &currentTime);
}

void *process_data_queue(void *param)
{
    if (media_queue == NULL)
    {
        return NULL;
    }

    while (1)
    {
        media_frame_s *media_frame = dequeue(media_queue);
        if (media_frame != NULL)
        {
            pack_mpegts(media_frame->frame_data, media_frame->frame_size, media_frame->frame_type, 0);
            free(media_frame->frame_data);
            free(media_frame);
        }
    }

    return NULL;
}

void *process_data_queue2(void *param)
{

    if (audio_queue == NULL || video_queue == NULL)
    {
        return NULL;
    }
    uint64_t video_dts = 0;
    uint64_t audio_dts = 0;

    while (1)
    {
        media_frame_s *audio_frame = dequeue(audio_queue);
        media_frame_s *video_frame = dequeue(video_queue);
        if (audio_frame != NULL && video_frame != NULL)
        {
            while (1)
            {
                video_dts = (video_frame->frame_number) / VIDEO_FPS;
                audio_dts = (audio_frame->frame_number * 1024) / AUDIO_SAMPLE_RATE;
                if (audio_dts <= video_dts)
                {
                    pack_mpegts(audio_frame->frame_data, audio_frame->frame_size, audio_frame->frame_type, 0);
                    free(audio_frame->frame_data);
                    free(audio_frame);
                    audio_frame = dequeue(audio_queue);
                    if (audio_frame == NULL)
                    {
                        pack_mpegts(video_frame->frame_data, video_frame->frame_size, video_frame->frame_type, 0);
                        free(video_frame->frame_data);
                        free(video_frame);
                        break;
                    }
                    continue;
                }
                else
                {
                    pack_mpegts(video_frame->frame_data, video_frame->frame_size, video_frame->frame_type, 0);
                    free(video_frame->frame_data);
                    free(video_frame);
                    video_frame = dequeue(video_queue);
                    if (video_frame == NULL)
                    {
                        pack_mpegts(audio_frame->frame_data, audio_frame->frame_size, audio_frame->frame_type, 0);
                        free(audio_frame->frame_data);
                        free(audio_frame);
                        break;
                    }
                    continue;
                }
            }
        }
        else
        {
            if (audio_frame != NULL)
            {
                pack_mpegts(audio_frame->frame_data, audio_frame->frame_size, audio_frame->frame_type, 0);
                free(audio_frame->frame_data);
                free(audio_frame);
            }
            else if (video_frame != NULL)
            {
                pack_mpegts(video_frame->frame_data, video_frame->frame_size, video_frame->frame_type, 0);
                free(video_frame->frame_data);
                free(video_frame);
            }
        }
    }

    return NULL;
}

void *receiveTsStream(void *param)
{
    srt_context ctx = (srt_context)param;
    if (ctx == NULL)
        return NULL;
    srt_state_e srt_state;

    int srt_primary_rcv_socket;
    char *buffer = (char *)calloc(SRT_PAYLOAD_SIZE, 1);
    char ptr[188] = {0};
    int cnt = 0;
    int ret;
    int buffer_len = 0;

    while (1)
    {
        get_srt_state(ctx, &srt_state);
        if (srt_state == SRT_STATE_DISCONNECTED || srt_state == SRT_STATE_IDLE || srt_state == SRT_STATE_LISTEN) continue;
        ret = srt_receive_data_from_primary_client(ctx, &buffer, &buffer_len);
        if (ret == SRT_PROTO_FAILED || buffer_len < 0 || (buffer_len % 188 != 0))
        {
            continue;
        }

        if (buffer_len == 188)
        {
            ts_demuxer_input(recv_ts, (const uint8_t *)buffer, buffer_len);
            memset(buffer, 0, buffer_len);
            continue;
        }
        cnt = 0;
        while (buffer_len != 0)
        {
            memcpy(ptr, buffer + cnt * 188, 188);
            buffer_len -= 188;
            cnt++;
            ts_demuxer_input(recv_ts, (const uint8_t *)ptr, sizeof(ptr));
        }
        memset(buffer, 0, SRT_PAYLOAD_SIZE);
    }
    free(buffer);
    pthread_exit(NULL);
}

void enqueue_h264_data(const void *buffer, int size, media_frame_e media_type)
{
    static uint64_t frame_num = 0;
    if (ENABLE_AUDIO && ENABLE_VIDEO)
    {
        enqueue(video_queue, create_media_frame(media_type, frame_num, (char *)buffer, size));
    }
    else
    {
        enqueue(media_queue, create_media_frame(media_type, frame_num, (char *)buffer, size));
    }

    frame_num++;
}

void enqueue_aac_data(void *param, unsigned char *data, unsigned int data_len)
{

    static uint64_t frame_num = 0;
    if (ENABLE_AUDIO && ENABLE_VIDEO)
    {
        enqueue(audio_queue, create_media_frame(AAC_FRAME, frame_num, (char *)data, data_len));
    }
    else
    {
        enqueue(media_queue, create_media_frame(AAC_FRAME, frame_num, (char *)data, data_len));
    }

    frame_num++;
}

void enqueue_pcm_data(void *param, short *data, unsigned int data_len)
{

    static uint64_t frame_num = 0;
    if (ENABLE_AUDIO && ENABLE_VIDEO)
    {
        enqueue(audio_queue, create_media_frame(AAC_FRAME, frame_num, (char *)data, data_len * 2));
    }
    else
    {
        enqueue(media_queue, create_media_frame(AAC_FRAME, frame_num, (char *)data, data_len * 2));
    }

    frame_num++;
}

void process_pcm_data(void *param, short *buffer, unsigned int buffer_size)
{
    aac_encode_context_s *aac_enc_ctx = (aac_encode_context_s *)param;
    if (aac_enc_ctx == NULL)
    {
        LOG_ERROR("process pcm data failed, param is NULL");
        exit(1);
    }

    start_aac_encode_pcm_data(aac_enc_ctx, (void *)buffer, buffer_size * 2, NULL);
}

const char *ftimestamp(int64_t t, char *buf)
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

static void mpeg_ts_dec_testonstream(void *param, int stream, int codecid, const void *extra, int bytes, int finish)
{
    LOG_INFOR("stream %d, codecid: %d, finish: %s\n", stream, codecid, finish ? "true" : "false");
}

void processAudioStream(char *data, int bytes)
{
    start_pcm_playback(pcm_recv_ctx, data, bytes);
}

static int on_ts_packet(void * /*param*/, int program, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void *data, size_t bytes)
{
    static char s_pts[64], s_dts[64];

    if (PSI_STREAM_AAC == avtype || PSI_STREAM_AUDIO_OPUS == avtype || PSI_STREAM_AUDIO_G711U == avtype)
    {
        static int64_t a_pts = 0;
        static int64_t a_dts = 0;
        if (PTS_NO_VALUE == dts)
            dts = pts;
        // assert(0 == a_dts || dts >= a_dts);
        // LOG_INFOR("[A][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d, bytes: %u%s%s\n", program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - a_pts) / 90, (int)(dts - a_dts) / 90, (unsigned int)bytes, flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");
        a_pts = pts;
        a_dts = dts;
        // processAudioStream((char *)data, bytes);
        // static int pcm_time = 0;
        // if (pcm_time > PCM_FILE_TIME) return 0;
        // if (pcm_time == PCM_FILE_TIME) {
        //     LOG_INFOR("Write pcm file successfully!");
        //     pcm_time++;
        //     fclose(wfp);
        // } else {
        //     LOG_INFOR("pcm_time: %d ms", pcm_time);
        //     fwrite(data, 1, bytes, wfp);
        //     pcm_time += 25;
        // }

    }
    else if (PSI_STREAM_H264 == avtype || PSI_STREAM_H265 == avtype)
    {
        static int64_t v_pts = 0;
        static int64_t v_dts = 0;
        static int32_t frame_index = 0;
        // assert(0 == v_dts || dts >= v_dts);
        LOG_INFOR("[V][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d, bytes: %u%s%s%s\n", program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - v_pts) / 90, (int)(dts - v_dts) / 90, (unsigned int)bytes, flags & MPEG_FLAG_IDR_FRAME ? " [I]" : "", flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");
        v_pts = pts;
        v_dts = dts;
        // processVideoFrame(data, bytes, frame_index);
        frame_index++;
        //        fwrite(data, 1, bytes, vfp);
    }
    else
    {
        static int64_t x_pts = 0;
        static int64_t x_dts = 0;
        // assert(0 == x_dts || dts >= x_dts);
        LOG_INFOR("[%d][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d%s%s%s\n", avtype, program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - x_pts) / 90, (int)(dts - x_dts) / 90, flags & MPEG_FLAG_IDR_FRAME ? " [I]" : "", flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");
        x_pts = pts;
        x_dts = dts;
        // assert(0);
    }
    return 0;
}

int startReceiveTsStream(void)
{
    int err;
    struct ts_demuxer_notify_t notify = {
        mpeg_ts_dec_testonstream,
    };

    recv_ts = ts_demuxer_create(on_ts_packet, NULL);
    ts_demuxer_set_notify(recv_ts, &notify, NULL);
}

void *receiveControlMessage(void *param)
{
    char *buffer = (char *)calloc(BUFFER_SIZE, 1);
    int len = 0;
    int ret = 0;
    tcp_state_e tcp_state;
    tcp_context ctx = (tcp_context)param;
    if (ctx == NULL)
        return NULL;
    while (1)
    {
        ret = recv_control_message(ctx, &buffer, &len);
        if (ret != TCP_RETURN_FAILED)
        {
            // printf("LEN: %d\n", len);
            handle_ptz_control_message(buffer, len);
        }
        else
        {
            // tcp is not accepted state
            assert(get_tcp_state(ctx, &tcp_state) == TCP_RETURN_OK);
            if (tcp_state != TCP_STATE_ACCEPTED)
            {
                change_tcp_to_accepted_state(ctx);
            }
        }
    }
    if (buffer != NULL)
    {
        free(buffer);
        buffer = NULL;
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{

    // Allocate contexts
    // tcp_ctx = get_tcp_instance();
    // assert(tcp_ctx != NULL);
    srt_ctx = get_srt_instance();
    assert(srt_ctx != NULL);

    if (ENABLE_AUDIO)
    {
        pcm_ctx = allocate_pcm_context(PCM_CONTEXT_TYPE_CAPTURE);

        // aac_enc_ctx = allocate_aac_encode_context();

        // if (aac_enc_ctx == NULL || pcm_ctx == NULL)
        // {
        //     return 1;
        // }
    }

    pcm_recv_ctx = allocate_pcm_context(PCM_CONTEXT_TYPE_PLAYBACK);
    if (pcm_recv_ctx == NULL)
        return 1;

    pthread_t process_data_queue_thread;
    // create a queue to save media frame
    if (ENABLE_AUDIO && ENABLE_VIDEO)
    {

        video_queue = create_media_queue();
        audio_queue = create_media_queue();
        // process data queue
        pthread_create(&process_data_queue_thread, NULL, process_data_queue2, NULL);
    }
    else
    {

        media_queue = create_media_queue();
        // process data queue
        pthread_create(&process_data_queue_thread, NULL, process_data_queue, NULL);
    }

    // set context
    // assert(set_tcp_ipport(tcp_ctx, 5001) == TCP_RETURN_OK);
    assert(set_srt_ipport(srt_ctx, 5000) == SRT_PROTO_OK);
    assert(set_srt_mode(srt_ctx, SRT_MODE_LISTENER) == SRT_PROTO_OK);
    // assert(change_tcp_to_listen_state(tcp_ctx) == TCP_RETURN_OK);
    assert(change_srt_to_listen_state(srt_ctx) == SRT_PROTO_OK);

    // mpegts
    struct mpeg_ts_func_t tshandler;
    tshandler.alloc = ts_alloc;
    tshandler.write = ts_write;
    tshandler.free = ts_free;
    ts = mpeg_ts_create(&tshandler, (void *)srt_ctx);
    if (ENABLE_AUDIO)
    {
        aac_stream = mpeg_ts_add_stream(ts, PSI_STREAM_AUDIO_G711U, NULL, 0);

        pcm_ctx->sample_rate = AUDIO_SAMPLE_RATE;

        pcm_ctx->channels = MONO;
        pcm_ctx->format = SND_PCM_FORMAT_S16_LE;
        pcm_ctx->data_element = 1024 * (pcm_ctx->channels);
        // pcm_ctx->pcm_capture_context->pcm_capture_handle_cb = process_pcm_data;
        pcm_ctx->pcm_capture_context->pcm_capture_handle_cb = enqueue_pcm_data;

        // aac_enc_ctx->sample_rate = AUDIO_SAMPLE_RATE;
        // aac_enc_ctx->aac_handle_cb = enqueue_aac_data;

        // init all
        // init_aac_encode(aac_enc_ctx);

        init_pcm(pcm_ctx);
    }

    // pcm_recv_ctx->sample_rate = AUDIO_SAMPLE_RATE;
    // pcm_recv_ctx->channels = MONO;
    // pcm_recv_ctx->format = SND_PCM_FORMAT_S16_LE;
    // pcm_recv_ctx->data_element = 1024 * (pcm_recv_ctx->channels);

    // init_pcm(pcm_recv_ctx);
    startReceiveTsStream();

    if (ENABLE_VIDEO)
    {
        h264_stream = mpeg_ts_add_stream(ts, PSI_STREAM_H264, NULL, 0);

        v4l2_ctx = get_v4l2_capture_instance();
        assert(v4l2_ctx != NULL);
        assert(v4l2_h264_set_device_name(v4l2_ctx, "/dev/video0") == V4L2_CAPTURE_RETURN_OK);
        assert(v4l2_h264_set_io_method(v4l2_ctx, IO_METHOD_MMAP) == V4L2_CAPTURE_RETURN_OK);
        assert(v4l2_h264_set_video_resolution(v4l2_ctx, VIDEO_RES_HD) == V4L2_CAPTURE_RETURN_OK);
    }

    // assert(change_tcp_to_accepted_state(tcp_ctx) == TCP_RETURN_OK);
    // while until has a client connected
    srt_state_e current_srt_state;
    while (1)
    {
        get_srt_state(srt_ctx, &current_srt_state);
        if (current_srt_state == SRT_STATE_ACCEPTED || current_srt_state == SRT_STATE_CLIENT_INCREASE || current_srt_state == SRT_STATE_CLIENT_DECREASE || current_srt_state == SRT_STATE_CLIENT_MAX)
        {
            break;
        }
        usleep(20000);
    }

    // start audio receiver
    pthread_t receive_thread;
    // pthread_t receive_control_thread;
    pthread_create(&receive_thread, NULL, receiveTsStream, srt_ctx);
    // pthread_create(&receive_control_thread, NULL, receiveControlMessage, tcp_ctx);

    // start audio capture
    if (ENABLE_AUDIO)
    {

        // start_pcm_capture(pcm_ctx, (void *)aac_enc_ctx);
        start_pcm_capture(pcm_ctx, NULL);
    }
    // start video capture
    if (ENABLE_VIDEO)
    {
        init_v4l2_h264_capture(v4l2_ctx);
        v4l2_h264_set_fps(v4l2_ctx, VIDEO_FPS);
        v4l2_h264_flip(v4l2_ctx);
        start_v4l2_h264_capture(v4l2_ctx);
        v4l2_h264_mainloop(v4l2_ctx, enqueue_h264_data);
    }

    while (1)
    {
        usleep(20000);
    }

    // Destroy all
    pthread_join(process_data_queue_thread, NULL);
    pthread_join(receive_thread, NULL);
    // pthread_join(receive_control_thread, NULL);
    destroy_pcm(pcm_recv_ctx);
    if (ENABLE_AUDIO)
    {
        destroy_pcm(pcm_ctx);
        // destroy_aac_encode(aac_enc_ctx);
    }
    if (ENABLE_VIDEO)
    {
        stop_v4l2_h264_capture(v4l2_ctx);
        uninit_v4l2_h264_capture(v4l2_ctx);
    }

    mpeg_ts_destroy(ts);
    destroy_srt_proto(srt_ctx);
    if (ENABLE_AUDIO && ENABLE_VIDEO)
    {
        pthread_mutex_destroy(&(audio_queue->lock));
        pthread_mutex_destroy(&(video_queue->lock));
    }
    else
    {
        pthread_mutex_destroy(&(media_queue->lock));
    }
    fclose(wfp);
    return 0;
}
