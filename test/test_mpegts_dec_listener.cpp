/**
 * @file test_mpegts_dec_listener.cpp
 * @author your name (you@domain.com)
 * @brief Test nhận luồng mpegts từ SRT sử dụng chế độ listener và demux sang H264 và AAC
 * @version 0.1
 * @date 2023-06-09
 *  (sử dụng trên RPI or PC)
 * @copyright Copyright (c) 2023
 * 
 */
#include "mpegts/mpeg-ps.h"
#include "mpegts/mpeg-ts.h"
#include "mpegts/mpeg-types.h"
#include "flv/mpeg4-aac.h"
#include "srt/srt.h"
#include <stdio.h>
#include <assert.h>

static FILE *vfp;
static FILE *afp;
ts_demuxer_t *ts;

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

static int on_ts_packet(void * /*param*/, int program, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void *data, size_t bytes)
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

        fwrite(data, 1, bytes, vfp);
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
    return 0;
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

int main(int argc, char const *argv[])
{
    vfp = fopen("v.h264", "wb");
    afp = fopen("a.aac", "wb");

    struct ts_demuxer_notify_t notify = {
        mpeg_ts_dec_testonstream,
    };

    ts = ts_demuxer_create(on_ts_packet, NULL);
    ts_demuxer_set_notify(ts, &notify, NULL);

    int ss, st;
    struct sockaddr_in sa;
    int yes = 1;
    struct sockaddr_storage their_addr;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    printf("SRT startup\n");
    srt_startup();

    printf("SRT socket\n");
    ss = srt_create_socket();
    if (ss == SRT_ERROR)
    {
        fprintf(stderr, "srt_socket: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("SRT bind address\n");
    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &sa.sin_addr) != 1)
    {
        return 1;
    }

    printf("SRT setsockflag\n");
    if (SRT_ERROR == srt_setsockflag(ss, SRTO_RCVSYN, &yes, sizeof(yes)))
    {
        fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("SRT bind\n");
    st = srt_bind(ss, (struct sockaddr *)&sa, sizeof sa);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_bind: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("SRT listen\n");
    st = srt_listen(ss, 2);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_listen: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("SRT accept\n");
    int addr_size = sizeof their_addr;
    int their_fd = srt_accept(ss, (struct sockaddr *)&their_addr, &addr_size);
    if (their_fd == SRT_INVALID_SOCK)
    {
        fprintf(stderr, "srt_accept: %s\n", srt_getlasterror_str);
        return 1;
    }

    char buffer[1316];
    while (1)
    {
        st = srt_recvmsg(their_fd, buffer, sizeof(buffer));
        if (st == SRT_ERROR)
        {
            fprintf(stderr, "srt_recvmsg: %s\n", srt_getlasterror_str());
            goto end;
        }
        // printf("Got msg len: %d \n", st);
        // write to file
        mpeg_ts_dec_test(buffer, st);
    }

end:
    ts_demuxer_flush(ts);
    ts_demuxer_destroy(ts);
    fclose(vfp);
    fclose(afp);

    printf("srt close\n");
    st = srt_close(ss);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_close: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("srt cleanup\n");
    srt_cleanup();
    return 0;
}