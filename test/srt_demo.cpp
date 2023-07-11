#include <stdio.h>
#include "h264_capture.h"
#include "logger.h"
#include "srt/srt.h"

int ss = 0;
int their_fd = -1;
FILE *fp;

void sending_frame(const void *buffer, int size, int frame_type, long latency)
{
    int st;
    if (their_fd == -1)
        return;
    st = srt_sendmsg2(their_fd, (char *)buffer, size, NULL);
    // memset(&buffer, 0, size);
    if (size < 1316)
        printf("packet: %d\n", size);
    // usleep(600);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_sendmsg: %s\n", srt_getlasterror_str());
        their_fd = -1;
    }
}

void write_frame(const void *buffer, int size)
{
    /* write to file */

    fwrite(buffer, size, 1, fp);
    fflush(fp);
}

int main(int argc, char **argv)
{
    h264_frame_buffer *buffers;

    h264_dev_cfg dev_cfg = {
        .dev_fd = -1,
        .dev_name = "/dev/video0",
        .io = IO_METHOD_MMAP,
        .force_format = 1,
        .buffers = buffers,
        .n_buffers = 0
    };

    init_h264_device(&dev_cfg);

    int st;
    struct sockaddr_in sa;
    int yes = 1;
    struct sockaddr_storage their_addr;
    bool no = false;
    int srt_latency = 400;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <post> <port>\n", argv[0]);
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
    if (SRT_ERROR == srt_setsockflag(ss, SRTO_SENDER, &yes, sizeof(yes)))
    {
        fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("SRT setsockflag\n");
    if (SRT_ERROR == srt_setsockflag(ss, SRTO_LATENCY, &srt_latency, sizeof(srt_latency)))
    {
        fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
        return 1;
    }

    //  printf("SRT setsockflag\n");
    // if (SRT_ERROR == srt_setsockflag(ss, SRTO_TLPKTDROP, &no, sizeof(yes)))
    // {
    //     fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return 1;
    // }

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
    their_fd = srt_accept(ss, (struct sockaddr *)&their_addr, &addr_size);
    if (their_fd == SRT_INVALID_SOCK)
    {
        fprintf(stderr, "srt_accept: %s\n", srt_getlasterror_str);
        return 1;
    }

    // fp = fopen("output.h264", "ab");

    start_h264_capturing(&dev_cfg);

    h264_mainloop(&dev_cfg, sending_frame);
    // h264_mainloop(&dev_cfg, write_frame);
    stop_h264_capturing(&dev_cfg);
    uninit_h264_device(&dev_cfg);

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