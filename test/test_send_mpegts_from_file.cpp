/**
 * @file test_send_mpegts_from_file_using_srt_listenter.cpp
 * @author your name (you@domain.com)
 * @brief Test stream mpegts từ file sử dụng giao thức SRT ở chế độ listener
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
#include "logger.h"
#include "srt_proto.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <map>

int st = 0;
int ss = 0;
int their_fd = -1;
char buffer[1316];

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
	memcpy(buffer + (i%7)*bytes, packet, bytes);
	if (i % 7 == 6) {
		printf("i = %d\n", i);
		st = srt_sendmsg2(their_fd, (char *)buffer, sizeof(buffer), NULL);
		if (st == SRT_ERROR)
		{
			fprintf(stderr, "srt_sendmsg: %s\n", srt_getlasterror_str());
			their_fd = -1;
			return 1;
		}
		memset(buffer, 0, sizeof(buffer));
		usleep(3000);
		// return 0;
	}
	i++;
	
	// usleep(500);
		return 0;

	// return 1 == fwrite(packet, bytes, 1, (FILE *)param) ? 0 : ferror((FILE *)param);
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
	char output[256] = {0};
	snprintf(output, sizeof(output) - 1, "%s.ts", input);

	struct mpeg_ts_func_t tshandler;
	tshandler.alloc = ts_alloc;
	tshandler.write = ts_write;
	tshandler.free = ts_free;

	FILE *fp = fopen(output, "wb");
	void *ts = mpeg_ts_create(&tshandler, fp);

	mpeg_ts_file(input, ts);

	mpeg_ts_destroy(ts);
	fclose(fp);
}

int main(int argc, char const *argv[])
{
	if (argc != 4)
	{
		fprintf(stderr, "Usage: %s <host> <port> <filename>\n", argv[0]);
		return 1;
	}

	/// srt
	
	srt_context_s* ctx = allocate_srt_context();
	ctx->ip_address = argv[1];
	ctx->ip_port = atoi(argv[2]);
	ctx->srt_mode = SRT_MODE_CALLER;
	init_srt_proto(ctx);

	their_fd = ctx->srt_socket;

	mpeg_ts_test(argv[3]);

	destroy_srt_proto(ctx);
	return 0;
}