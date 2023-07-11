/**
 * @file rpi_send_mpegts_srt_vlc.cpp
 * @author your name (you@domain.com)
 * @brief Test gửi luồng ts qua SRT sử dụng chế độ Caller
 * @version 0.1
 * @date 2023-06-09
 * 
 * Sử dụng trên RPI
 *  
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include "mpegts/mpeg-ts.h"
#include "h264_capture.h"
#include "logger.h"
#include "srt/srt.h"

int packet_priod = 0;
void *ts;
int stream;
int ss = 0;
int st = 0;
int their_fd = -1;
FILE *fp;
char buffer[1316];
int sock_fd, send_bytes;
struct sockaddr_in server_addr;


static void* ts_alloc(void* /*param*/, size_t bytes)
{
	static char s_buffer[188];
	assert(bytes <= sizeof(s_buffer));
	return s_buffer;
}

static void ts_free(void* /*param*/, void* /*packet*/)
{
	return;
}

static int ts_write(void* param, const void* packet, size_t bytes)
{
	// return 1 == fwrite(packet, bytes, 1, (FILE*)param) ? 0 : ferror((FILE*)param);
    static int i = 0;
    if (i == 7) i = 0;
	// printf("byte: %d\n", bytes);
	memcpy(buffer + (i%7)*bytes, packet, bytes);
	if (i % 7 == 6) {
		printf("i = %d\n", i);
		st = srt_sendmsg2(ss, (char *)buffer, sizeof(buffer), NULL);
		if (st == SRT_ERROR)
		{
			fprintf(stderr, "srt_sendmsg: %s\n", srt_getlasterror_str());
			ss = -1;
			exit(1);
		}
		memset(buffer, 0, sizeof(buffer));
		usleep(packet_priod);
		// return 0;
	}
	i++;
	
	// usleep(500);
		return 0;
}




void pack_mpegts(const void *buffer, int size, int media_type)
{
    long pts = 0;
	long dts = 0;

	if (pts == 0 && dts == 0) {

    	struct timespec currentTime;
    	clock_gettime(CLOCK_MONOTONIC, &currentTime);
    	
    	// Calculate the current time in milliseconds
		long milliseconds = currentTime.tv_sec * 1000 + currentTime.tv_nsec / 1000000;
    	pts = milliseconds*90;
	}
	else {
		pts += 3000;
	}

    dts = pts;

    mpeg_ts_write(ts, stream, MPEG_FLAG_IDR_FRAME,pts, dts, buffer, size);
	// dts += 3000;
}



void sending_frame(const void *buffer, int size)
{
	// send_bytes = sendto(sock_fd, buffer, size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // if (send_bytes == -1) {
    //     perror("sendto");
    //     exit(1);
    // }



	int st;
    if (their_fd == -1)
        exit(1);
    st = srt_sendmsg2(their_fd, (char *)buffer, size, NULL);
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

    
    struct mpeg_ts_func_t tshandler;
	tshandler.alloc = ts_alloc;
	tshandler.write = ts_write;
	tshandler.free = ts_free;
    void *fp;
    ts = mpeg_ts_create(&tshandler, fp);
    stream = mpeg_ts_add_stream(ts, PSI_STREAM_H264, NULL, 0);

	h264_frame_buffer *buffers;

	h264_dev_cfg dev_cfg = {
		.dev_fd = -1,
		.dev_name = "/dev/video0",
		.io = IO_METHOD_MMAP,
		.force_format = 3,
		.buffers = buffers,
		.n_buffers = 0,
	};

	init_h264_device(&dev_cfg);
	v4l2_h264_set_fps(&dev_cfg);
    


	

	// create socket
    // sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    // if (sock_fd == -1) {
    //     perror("socket");
    //     exit(1);
    // }

    // set server address
    // memset(&server_addr, 0, sizeof(server_addr));
    // server_addr.sin_family = AF_INET;
    // server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    // server_addr.sin_port = htons(SERVER_PORT);


	/// srt
	 int st;
    struct sockaddr_in sa;
    int yes = 1;
    struct sockaddr_storage their_addr;
    bool no = false;
    int srt_latency = 300;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <cost> <port>\n", argv[0]);
        return 1;
    }

    // printf("SRT startup\n");
    // srt_startup();

    // printf("SRT socket\n");
    // ss = srt_create_socket();
    // if (ss == SRT_ERROR)
    // {
    //     fprintf(stderr, "srt_socket: %s\n", srt_getlasterror_str());
    //     return 1;
    // }

    // printf("SRT bind address\n");
    // sa.sin_family = AF_INET;
    // sa.sin_port = htons(atoi(argv[2]));
    // if (inet_pton(AF_INET, argv[1], &sa.sin_addr) != 1)
    // {
    //     return 1;
    // }

    // printf("SRT setsockflag\n");
    // if (SRT_ERROR == srt_setsockflag(ss, SRTO_SENDER, &yes, sizeof(yes)))
    // {
    //     fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return 1;
    // }

    // //  printf("SRT setsockflag\n");
    // // if (SRT_ERROR == srt_setsockflag(ss, SRTO_TLPKTDROP, &yes, sizeof(yes)))
    // // {
    //     // fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
    //     // return 1;
    // // }

	// printf("SRT setsockflag\n");
    // if (SRT_ERROR == srt_setsockflag(ss, SRTO_LATENCY, &srt_latency, sizeof(srt_latency)))
    // {
    //     fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
    //     return 1;
    // }


    // printf("SRT bind\n");
    // st = srt_bind(ss, (struct sockaddr *)&sa, sizeof sa);
    // if (st == SRT_ERROR)
    // {
    //     fprintf(stderr, "srt_bind: %s\n", srt_getlasterror_str());
    //     return 1;
    // }

    // printf("SRT listen\n");
    // st = srt_listen(ss, 2);
    // if (st == SRT_ERROR)
    // {
    //     fprintf(stderr, "srt_listen: %s\n", srt_getlasterror_str());
    //     return 1;
    // }

    // printf("SRT accept\n");
    // int addr_size = sizeof their_addr;
    // their_fd = srt_accept(ss, (struct sockaddr *)&their_addr, &addr_size);
    // if (their_fd == SRT_INVALID_SOCK)
    // {
    //     fprintf(stderr, "srt_accept: %s\n", srt_getlasterror_str);
    //     return 1;
    // }
printf("srt startup\n");
    srt_startup();

    printf("srt socket\n");
    ss = srt_create_socket();
    if (ss == SRT_ERROR)
    {
        fprintf(stderr, "srt_socket: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("srt remote address\n");
    sa.sin_family = AF_INET;
    sa.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &sa.sin_addr) != 1)
    {
        return 1;
    }

    printf("srt setsockflag\n");
    if (SRT_ERROR == srt_setsockflag(ss, SRTO_SENDER, &yes, sizeof yes))
    {
        fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
        return 1;
    }

    // Test deprecated
    //srt_setsockflag(ss, SRTO_STRICTENC, &yes, sizeof yes);

    printf("srt connect\n");
    st = srt_connect(ss, (struct sockaddr*)&sa, sizeof sa);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_connect: %s\n", srt_getlasterror_str());
        return 1;
    }




	start_h264_capturing(&dev_cfg);
	h264_mainloop(&dev_cfg, pack_mpegts);
	stop_h264_capturing(&dev_cfg);
	uninit_h264_device(&dev_cfg);


    mpeg_ts_destroy(ts);

	printf("srt close\n");
    st = srt_close(ss);
    if (st == SRT_ERROR)
    {
        fprintf(stderr, "srt_close: %s\n", srt_getlasterror_str());
        return 1;
    }

    printf("srt cleanup\n");
    srt_cleanup();

	close(sock_fd);
	return 0;
}