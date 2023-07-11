/**
 * @file test_send_mpegts_from_file_using_srt_listenter_encrypt.cpp
 * @author your name (you@domain.com)
 * @brief Test stream mpegts từ file sử dụng giao thức SRT ở chế độ listener và sử dụng encrypt
 * @version 0.1
 * @date 2023-06-09
 *
 *  (Sử dụng trên PC or RPI)
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "mpegts/mpeg-ps.h"
#include "mpegts/mpeg-ts.h"
#include "mpegts/mpeg-types.h"
#include "logger.h"
#include "srt/srt.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <map>

int st = 0;
int ss = 0;
int their_fd = -1;
char buffer[1316];
int pkt_len = 0;
struct timespec currentTime;
int diff = 0;
int av_len = 0;

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
	static int i = 0;
	// printf("byte: %d\n", bytes);
	// printf("AV_LEN: %d\n", av_len);

	memcpy(buffer + pkt_len, packet, bytes);
	pkt_len += bytes;
	if (pkt_len == 1316)
	{
		st = srt_sendmsg2(their_fd, (char *)buffer, sizeof(buffer), NULL);
		if (st == SRT_ERROR)
		{
			fprintf(stderr, "srt_sendmsg: %s\n", srt_getlasterror_str());
			their_fd = -1;
			return 1;
		}
		pkt_len = 0;
		memset(buffer, 0, sizeof(buffer));
		usleep(2000);
	}

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
	// printf("[%s] pts: %08lu, dts: %08lu%s\n", ts_type(avtype), (unsigned long)pts, (unsigned long)dts, flags ? " [I]":"");

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
		av_len = bytes;
	}
	else if (PSI_STREAM_H264 == avtype || PSI_STREAM_H265 == avtype)
	{
		static int64_t v_pts = 0, v_dts = 0;
		static long last_video_time = 0;
		// assert(0 == v_dts || dts >= v_dts);
		printf("[V][%d:%d] pts: %s(%lld), dts: %s(%lld), diff: %03d/%03d, bytes: %u%s%s%s\n", program, stream, ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - v_pts) / 90, (int)(dts - v_dts) / 90, (unsigned int)bytes, flags & MPEG_FLAG_IDR_FRAME ? " [I]" : "", flags & MPEG_FLAG_PACKET_CORRUPT ? " [X]" : "", flags & MPEG_FLAG_PACKET_LOST ? " [-]" : "");

		clock_gettime(CLOCK_MONOTONIC, &currentTime);
		// Calculate the current time in milliseconds
		long current_video_time = currentTime.tv_sec * 1000 + currentTime.tv_nsec / 1000000;
		diff = current_video_time - last_video_time;
		// printf("%d\n", diff);
		long temp = (dts - v_dts) / 90;
		if (diff < temp)
		{
			usleep((temp - diff) * 1000);
		}
		last_video_time = current_video_time;

		v_pts = pts;
		v_dts = dts;
		av_len = bytes;
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
void mpeg_ts_test(const char *input)
{
	struct mpeg_ts_func_t tshandler;
	tshandler.alloc = ts_alloc;
	tshandler.write = ts_write;
	tshandler.free = ts_free;

	void *ts = mpeg_ts_create(&tshandler, NULL);

	mpeg_ts_file(input, ts);

	mpeg_ts_destroy(ts);
}


void removeQuotes(char* str) {
    int length = strlen(str);
    
    if (length >= 2 && str[0] == '"' && str[length - 1] == '"') {
        // Shift the characters to the left to remove the quotes
        for (int i = 0; i < length - 1; i++) {
            str[i] = str[i + 1];
        }
        
        // Null-terminate the string
        str[length - 2] = '\0';
    }
}

int main(int argc, char const *argv[])
{
	if (argc != 5)
	{
		fprintf(stderr, "Usage: %s <host> <port> <filename> <passphrase>\n", argv[0]);
		return 1;
	}

	/// srt
	int st;
	struct sockaddr_in sa;
	int yes = 1;
	struct sockaddr_storage their_addr;
	bool no = false;
	int srt_latency = 300;

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
	
	char password[100] = {0};
	memcpy(password, argv[4], strlen(argv[4]));

	removeQuotes(password);
	printf("%s\n", password);
	if (strlen(password) < 10) {
		LOG_ERROR("password len is less than 10");
		return 1;
	}
	if (SRT_ERROR == srt_setsockflag(ss, SRTO_PASSPHRASE, password, strlen(password)))
	{
		fprintf(stderr, "srt_setsockflag: %s\n", srt_getlasterror_str());
		return 1;
	}


	printf("SRT set AES\n");
	// 0 =PBKEYLEN (default value)
	//16 = AES-128 (effective value)
	//24 = AES-192
	//32 = AES-256
	int32_t pbkeylen = 32;
	if (SRT_ERROR == srt_setsockflag(ss, SRTO_PBKEYLEN, &pbkeylen, sizeof(pbkeylen)))
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

	mpeg_ts_test(argv[3]);

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