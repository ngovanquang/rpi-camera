#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include "h264_capture.h"
#include "logger.h"

#define H264_NAL_IDR 5

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264 v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */
#endif

typedef struct v4l2_capture_context_t
{
	int dev_fd;
	char *dev_name;
	v4l2_io_method_e io;
	video_resolution_e force_format;
	h264_frame_buffer *buffers;
	unsigned int n_buffers;
	v4l2_state_e v4l2_capture_state;
} v4l2_capture_context_t;

static v4l2_capture_context_t *v4l2_ctx = NULL;

v4l2_capture_context get_v4l2_capture_instance(void)
{
	if (v4l2_ctx == NULL)
	{
		v4l2_ctx = (v4l2_capture_context_t *)calloc(sizeof(v4l2_capture_context_t), 1);
		assert(v4l2_ctx != NULL);

		// init
		v4l2_ctx->dev_fd = -1;
		v4l2_ctx->dev_name = NULL;
		v4l2_ctx->buffers = NULL;
		v4l2_ctx->n_buffers = 0;
		v4l2_ctx->v4l2_capture_state = V4L2_STATE_STOP;

		LOG_INFOR("V4L2 Capture allocate OK");
	}
	return v4l2_ctx;
}

static int out_buf = 0;
static int frame_count = 0;
static int frame_number = 0;

static int xioctl(int fh, int request, void *arg)
{
	int r;

	do
	{
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

static int mpeg_h264_find_keyframe(const uint8_t *p, size_t bytes)
{
	size_t i;
	uint8_t type;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2])
		{
			type = p[i + 1] & 0x1f;
			if (H264_NAL_IDR >= type && 1 <= type)
				return H264_NAL_IDR == type ? 1 : 0;
		}
	}

	return 0;
}

// static void process_image(const void *p, int size, h264_process_frame_t callback)
// {
// 	static long video_last_ts_ms = 0;
// 	static long last_milisecond = 0;
// 	long video_ts = 0;
// 	long duration = 0;
// 	long keyframe_duration = 0;
// 	int status;
// 	struct timespec currentTime;
// 	clock_gettime(CLOCK_MONOTONIC, &currentTime);
// 	long time_stamp = currentTime.tv_sec * 1000 + currentTime.tv_nsec / 1000000;

// 	// Calculate timestamp
// 	if (video_last_ts_ms != 0 && time_stamp > video_last_ts_ms)
// 	{
// 		// video_ts = video_ts + 90000/(1000/(time_stamp - video_last_ts_ms));
// 		duration = time_stamp - video_last_ts_ms;
// 		video_ts = video_ts + 90 * duration;
// 	}

// 	frame_number++;
// 	if (out_buf == 0)
// 	{
// 		if (mpeg_h264_find_keyframe((const uint8_t *)p, size))
// 		{
// 			frame_type = 1;
// 			// key_frame++;

// 			// Calculate the current time in milliseconds
// 			clock_gettime(CLOCK_MONOTONIC, &currentTime);
// 			long milliseconds = currentTime.tv_sec * 1000 + currentTime.tv_nsec / 1000000;
// 			keyframe_duration = milliseconds - last_milisecond;
// 			LOG_INFOR("\n\n\nKey Frame >> Frame Number: %d Size: %d >> %d\n\n\n\n", frame_number, size, keyframe_duration);
// 			last_milisecond = milliseconds;
// 		}
// 		else
// 		{
// 			frame_type = 0;
// 			// LOG_INFO("Frame Number: %d Size: %d", frame_number, size);
// 		}

// 		callback(p, size, H264_FRAME);
// 	}
// 	else
// 	{
// 		/* write to stdout */
// 		status = write(1, p, size);
// 		if (status == -1)
// 			LOG_ERROR("write error %d, %s\n", errno, strerror(errno));
// 	}

// 	video_last_ts_ms = time_stamp;
// }

static int is_h264_frame_type(const char *frame, int size, media_frame_e *frame_type)
{
	/* Check h264 start indicator*/
	if (!(frame[0] == 0x00 && frame[1] == 0x00 && frame[2] == 0x00 && frame[3] == 0x01))
	{
		return V4L2_CAPTURE_RETURN_FAILED;
	}
	/* Check the frame type by the first byte of *buf after remove 00 00 00 01 */
	switch (frame[4])
	{
	case H264_SPS_FRAME_TYPE:
		*frame_type = H264_SPS_FRAME;
		break; // SPS
	case H264_PPS_FRAME_TYPE:
		*frame_type = H264_PPS_FRAME;
		break; // PPS
	case H264_P_FRAME_TYPE:
		*frame_type = H264_P_FRAME;
		break; // P Frame
	case H264_I_FRAME_TYPE:
		*frame_type = H264_I_FRAME;
		break; // I Frame
	}
	return V4L2_CAPTURE_RETURN_OK;
}

static void process_image(const void *p, int size, h264_process_frame_t callback)
{
	static char* sps_pps_frame_buffer = NULL;
	static int sps_pps_frame_len = 0;
	if (p == NULL)
	{
		LOG_INFOR("Video streaming is NULL buffer");
		return;
	}

	media_frame_e frame_type;
	if (is_h264_frame_type((char*)p, size, &frame_type) == V4L2_CAPTURE_RETURN_FAILED)
	{
		return;
	}
	if (frame_type == H264_SPS_FRAME || frame_type == H264_PPS_FRAME) {
		if (sps_pps_frame_buffer != NULL) {
			free(sps_pps_frame_buffer);
		}
		sps_pps_frame_buffer = (char*)malloc(size);
		memcpy(sps_pps_frame_buffer, p, size);
		sps_pps_frame_len = size;
	}
	static long video_last_ts_ms = 0;
	static long last_milisecond = 0;
	long video_ts = 0;
	long duration = 0;
	long keyframe_duration = 0;
	int status;
	struct timespec currentTime;
	clock_gettime(CLOCK_MONOTONIC, &currentTime);
	long time_stamp = currentTime.tv_sec * 1000 + currentTime.tv_nsec / 1000000;

	// Calculate timestamp
	if (video_last_ts_ms != 0 && time_stamp > video_last_ts_ms)
	{
		// video_ts = video_ts + 90000/(1000/(time_stamp - video_last_ts_ms));
		duration = time_stamp - video_last_ts_ms;
		video_ts = video_ts + 90 * duration;
	}

	frame_number++;
	if (out_buf == 0)
	{
		if (frame_type == H264_I_FRAME)
		{
			// key_frame++;
			callback(sps_pps_frame_buffer, sps_pps_frame_len, H264_SPS_FRAME);
			// Calculate the current time in milliseconds
			clock_gettime(CLOCK_MONOTONIC, &currentTime);
			long milliseconds = currentTime.tv_sec * 1000 + currentTime.tv_nsec / 1000000;
			keyframe_duration = milliseconds - last_milisecond;
			// LOG_INFOR("\n\n\nKey Frame >> Frame Number: %d Size: %d >> %d\n\n\n\n", frame_number, size, keyframe_duration);
			last_milisecond = milliseconds;
		}
		else
		{
			// LOG_INFOR("Frame Type: %s%s%s%s, Frame Number: %d Size: %d", (frame_type == H264_I_FRAME ? "I" : ""),
			// (frame_type == H264_P_FRAME ? "P" : ""), (frame_type == H264_SPS_FRAME ? "SPS" : ""), (frame_type == H264_PPS_FRAME ? "PPS" : ""), frame_number, size);
		}
		callback(p, size, frame_type);
	}
	else
	{
		/* write to stdout */
		status = write(1, p, size);
		if (status == -1)
			LOG_ERROR("write error %d, %s\n", errno, strerror(errno));
	}

	video_last_ts_ms = time_stamp;
}

static int read_frame(v4l2_capture_context ctx, h264_process_frame_t callback)
{
	struct v4l2_buffer buf;
	unsigned int i;

	switch (ctx->io)
	{
	case IO_METHOD_READ:
		if (-1 == read(ctx->dev_fd, ctx->buffers[0].start, ctx->buffers[0].length))
		{
			switch (errno)
			{
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				LOG_ERROR("read error %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

		process_image(ctx->buffers[0].start, ctx->buffers[0].length, callback);
		break;

	case IO_METHOD_MMAP:
		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl(ctx->dev_fd, VIDIOC_DQBUF, &buf))
		{
			switch (errno)
			{
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				LOG_ERROR("VIDIOC_DQBUF error %d, %s\n", errno, strerror(errno));
				if (ctx->v4l2_capture_state == V4L2_STATE_STOP)
				{
					return 1;
				}
				else
				{
					exit(EXIT_FAILURE);
				}
			}
		}

		assert(buf.index < ctx->n_buffers);

		process_image(ctx->buffers[buf.index].start, buf.bytesused, callback);

		if (-1 == xioctl(ctx->dev_fd, VIDIOC_QBUF, &buf))
		{
			LOG_ERROR("VIDIOC_QBUF error %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;

	case IO_METHOD_USERPTR:
		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;

		if (-1 == xioctl(ctx->dev_fd, VIDIOC_DQBUF, &buf))
		{
			switch (errno)
			{
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				LOG_ERROR("VIDIOC_DQBUF error %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

		for (i = 0; i < ctx->n_buffers; ++i)
			if (buf.m.userptr == (unsigned long)ctx->buffers[i].start && buf.length == ctx->buffers[i].length)
				break;

		assert(i < ctx->n_buffers);

		process_image((void *)buf.m.userptr, buf.bytesused, callback);

		if (-1 == xioctl(ctx->dev_fd, VIDIOC_QBUF, &buf))
		{
			LOG_ERROR("VIDIOC_QBUF error %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;
	}

	return 1;
}

void v4l2_h264_mainloop(v4l2_capture_context ctx, h264_process_frame_t callback)
{
	unsigned int count;
	unsigned int loopIsInfinite = 0;

	if (frame_count == 0)
		loopIsInfinite = 1; // infinite loop
	count = frame_count;

	while (loopIsInfinite)
	{
		if (ctx->v4l2_capture_state == V4L2_STATE_STOP)
		{
			break;
		}
		for (;;)
		{

			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO(&fds);
			FD_SET(ctx->dev_fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(ctx->dev_fd + 1, &fds, NULL, NULL, &tv);

			if (-1 == r)
			{
				if (EINTR == errno)
					continue;
				LOG_ERROR("select error %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}

			if (0 == r)
			{
				LOG_ERROR("select timeout %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}

			if (ctx->v4l2_capture_state == V4L2_STATE_STOP)
			{
				break;
			}
			if (read_frame(ctx, callback))
				break;
			/* EAGAIN - continue select loop. */
		}
	}
}

void stop_v4l2_h264_capture(v4l2_capture_context ctx)
{
	enum v4l2_buf_type type;

	switch (ctx->io)
	{
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(ctx->dev_fd, VIDIOC_STREAMOFF, &type))
		{
			LOG_ERROR("VIDIOC_STREAMOFF %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;
	}
	ctx->v4l2_capture_state = V4L2_STATE_STOP;
}

void start_v4l2_h264_capture(v4l2_capture_context ctx)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (ctx->io)
	{
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < ctx->n_buffers; ++i)
		{
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;

			if (-1 == xioctl(ctx->dev_fd, VIDIOC_QBUF, &buf))
			{
				LOG_ERROR("VIDIOC_QBUF %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(ctx->dev_fd, VIDIOC_STREAMON, &type))
		{
			LOG_ERROR("VIDIOC_STREAMON %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < ctx->n_buffers; ++i)
		{
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.index = i;
			buf.m.userptr = (unsigned long)ctx->buffers[i].start;
			buf.length = ctx->buffers[i].length;

			if (-1 == xioctl(ctx->dev_fd, VIDIOC_QBUF, &buf))
			{
				LOG_ERROR("VIDIOC_QBUF %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(ctx->dev_fd, VIDIOC_STREAMON, &type))
		{
			LOG_ERROR("VIDIOC_STREAMON %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;
	}

	ctx->v4l2_capture_state = V4L2_STATE_START;
}

static void init_read(v4l2_capture_context ctx, unsigned int buffer_size)
{
	ctx->buffers = (h264_frame_buffer *)calloc(1, sizeof(*ctx->buffers));

	if (!ctx->buffers)
	{
		LOG_ERROR("Out of memmory\n");
		exit(EXIT_FAILURE);
	}

	ctx->buffers[0].length = buffer_size;
	ctx->buffers[0].start = malloc(buffer_size);

	if (!ctx->buffers[0].start)
	{
		LOG_ERROR("Out of memmory\n");
		exit(EXIT_FAILURE);
	}
}

static void init_mmap(v4l2_capture_context ctx)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(ctx->dev_fd, VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno)
		{
			LOG_ERROR("%s does not support memory mapping\n", ctx->dev_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			LOG_ERROR("VIDIOC_REQBUFS %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	if (req.count < 2)
	{
		LOG_ERROR("Insufficient buffer memory on %s\n", ctx->dev_name);
		exit(EXIT_FAILURE);
	}

	ctx->buffers = (h264_frame_buffer *)calloc(req.count, sizeof(*ctx->buffers));

	if (!ctx->buffers)
	{
		LOG_ERROR("Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (ctx->n_buffers = 0; ctx->n_buffers < req.count; ++ctx->n_buffers)
	{
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = ctx->n_buffers;

		if (-1 == xioctl(ctx->dev_fd, VIDIOC_QUERYBUF, &buf))
		{
			LOG_ERROR("VIDIOC_QUERYBUF %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		ctx->buffers[ctx->n_buffers].length = buf.length;
		ctx->buffers[ctx->n_buffers].start =
			mmap(NULL /* start anywhere */,
				 buf.length,
				 PROT_READ | PROT_WRITE /* required */,
				 MAP_SHARED /* recommended */,
				 ctx->dev_fd, buf.m.offset);

		if (MAP_FAILED == ctx->buffers[ctx->n_buffers].start)
		{
			LOG_ERROR("mmap %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

static void init_userp(v4l2_capture_context ctx, unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(ctx->dev_fd, VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno)
		{
			LOG_ERROR("%s does not support user pointer i/o\n", ctx->dev_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			LOG_ERROR("VIDIOC_REQBUFS %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	ctx->buffers = (h264_frame_buffer *)calloc(4, sizeof(*ctx->buffers));

	if (!ctx->buffers)
	{
		LOG_ERROR("Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (ctx->n_buffers = 0; ctx->n_buffers < 4; ++ctx->n_buffers)
	{
		ctx->buffers[ctx->n_buffers].length = buffer_size;
		ctx->buffers[ctx->n_buffers].start = malloc(buffer_size);

		if (!ctx->buffers[ctx->n_buffers].start)
		{
			LOG_ERROR("Out of memory\n");
			exit(EXIT_FAILURE);
		}
	}
}

static void open_device(v4l2_capture_context ctx)
{
	struct stat st;

	if (-1 == stat(ctx->dev_name, &st))
	{
		LOG_ERROR("Cannot identify '%s': %d, %s\n", ctx->dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR(st.st_mode))
	{
		LOG_ERROR("%s is no device\n", ctx->dev_name);
		exit(EXIT_FAILURE);
	}

	ctx->dev_fd = open(ctx->dev_name, O_RDWR | O_NONBLOCK, 0);

	if (-1 == ctx->dev_fd)
	{
		LOG_ERROR("Cannot open '%s': %d, %s\n", ctx->dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	LOG_INFOR("Open device: %s success\n", ctx->dev_name);
}

void init_v4l2_h264_capture(v4l2_capture_context ctx)
{
	open_device(ctx);
	// init
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl(ctx->dev_fd, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			LOG_ERROR("%s is no V4L2 device\n", ctx->dev_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			LOG_ERROR("VIDIOC_QUERYCAP %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		LOG_ERROR("%s is no video capture device\n", ctx->dev_name);
		exit(EXIT_FAILURE);
	}

	switch (ctx->io)
	{
	case IO_METHOD_READ:
		if (!(cap.capabilities & V4L2_CAP_READWRITE))
		{
			LOG_ERROR("%s does not support read i/o\n", ctx->dev_name);
			exit(EXIT_FAILURE);
		}
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING))
		{
			LOG_ERROR("%s does not support streaming i/o\n", ctx->dev_name);
			exit(EXIT_FAILURE);
		}
		break;
	}

	/* Select video input, video standard and tune here. */

	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(ctx->dev_fd, VIDIOC_CROPCAP, &cropcap))
	{
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(ctx->dev_fd, VIDIOC_S_CROP, &crop))
		{
			switch (errno)
			{
			case EINVAL:
				/* Cropping not supported. */
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	}
	else
	{
		/* Errors ignored. */
	}

	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	LOG_INFOR("Force Format %d\n", ctx->force_format);
	if (ctx->force_format)
	{
		if (ctx->force_format == VIDEO_RES_HD)
		{
			fmt.fmt.pix.width = 1280;
			fmt.fmt.pix.height = 720;
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
			fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
		}
		else if (ctx->force_format == VIDEO_RES_SD)
		{
			fmt.fmt.pix.width = 640;
			fmt.fmt.pix.height = 480;
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
			fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
		}
		else if (ctx->force_format == VIDEO_RES_FULL_HD)
		{
			fmt.fmt.pix.width = 1920;
			fmt.fmt.pix.height = 1080;
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
			fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
		}

		if (-1 == xioctl(ctx->dev_fd, VIDIOC_S_FMT, &fmt))
		{
			LOG_ERROR("VIDIOC_S_FMT %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		/* Note VIDIOC_S_FMT may change width and height. */
	}
	else
	{
		/* Preserve original settings as set by v4l2-ctl for example */
		if (-1 == xioctl(ctx->dev_fd, VIDIOC_G_FMT, &fmt))
		{
			LOG_ERROR("VIDIOC_G_FMT %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	switch (ctx->io)
	{
	case IO_METHOD_READ:
		init_read(ctx, fmt.fmt.pix.sizeimage);
		break;

	case IO_METHOD_MMAP:
		init_mmap(ctx);
		break;

	case IO_METHOD_USERPTR:
		init_userp(ctx, fmt.fmt.pix.sizeimage);
		break;
	}
}

void uninit_v4l2_h264_capture(v4l2_capture_context ctx)
{
	unsigned int i;

	switch (ctx->io)
	{
	case IO_METHOD_READ:
		free(ctx->buffers[0].start);
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < ctx->n_buffers; ++i)
			if (-1 == munmap(ctx->buffers[i].start, ctx->buffers[i].length))
			{
				LOG_ERROR("munmap %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < ctx->n_buffers; ++i)
			free(ctx->buffers[i].start);
		break;
	}

	free(ctx->buffers);

	// close device
	if (-1 == close(ctx->dev_fd))
	{
		LOG_ERROR("close %d, %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	ctx->dev_fd = -1;
	// free(ctx->dev_name);
	// free(ctx);
}

void v4l2_h264_flip(v4l2_capture_context ctx)
{
	// Set the camera flip control to flip vertically
	struct v4l2_control ctrl;
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_VFLIP;
	ctrl.value = 1;
	if (xioctl(ctx->dev_fd, VIDIOC_S_CTRL, &ctrl) == -1)
	{
		LOG_ERROR("Failed to set camera flip");
		close(ctx->dev_fd);
		exit(EXIT_FAILURE);
	}
}

void v4l2_h264_set_fps(v4l2_capture_context ctx, int fps)
{
	struct v4l2_streamparm stream_param;
	memset(&stream_param, 0, sizeof(struct v4l2_streamparm));
	stream_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(ctx->dev_fd, VIDIOC_G_PARM, &stream_param) == -1)
	{
		perror("Failed to get stream parameters");
		exit(EXIT_FAILURE);
	}

	stream_param.parm.capture.timeperframe.numerator = 1;
	stream_param.parm.capture.timeperframe.denominator = fps;

	if (ioctl(ctx->dev_fd, VIDIOC_S_PARM, &stream_param) == -1)
	{
		perror("Failed to set stream parameters");
		exit(EXIT_FAILURE);
	}
}

void v4l2_h264_set_cbr(v4l2_capture_context ctx)
{
	// Set CBR (Constant Bit Rate) instead of VBR (Variable Bit Rate)

	struct v4l2_control ctrl;
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
	ctrl.value = 2 * 1024 * 1024;
	if (xioctl(ctx->dev_fd, VIDIOC_S_CTRL, &ctrl) == -1)
	{
		LOG_ERROR("Failed to set cbr");
		close(ctx->dev_fd);
		exit(EXIT_FAILURE);
	}

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE;
	ctrl.value = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
	if (xioctl(ctx->dev_fd, VIDIOC_S_CTRL, &ctrl) == -1)
	{
		LOG_ERROR("Failed to set cbr");
		close(ctx->dev_fd);
		exit(EXIT_FAILURE);
	}
}

int v4l2_h264_set_device_name(v4l2_capture_context ctx, char *dev_name)
{
	if (ctx != NULL)
	{
		LOG_INFOR("Set v4l2 device name to: %s", dev_name);
		if (ctx->dev_name != NULL)
		{
			memcpy(ctx->dev_name, dev_name, strlen(dev_name));
		}
		else
		{
			ctx->dev_name = strdup(dev_name);
		}
	}
	else
	{
		LOG_ERROR("Could not set v4l2 dev name, you have to get v4l2 instance first");
		return V4L2_CAPTURE_RETURN_FAILED;
	}
	return V4L2_CAPTURE_RETURN_OK;
}
int v4l2_h264_set_io_method(v4l2_capture_context ctx, v4l2_io_method_e io_method)
{
	if (ctx != NULL)
	{
		LOG_INFOR("Set v4l2 device name to: %s%s%s", (io_method == IO_METHOD_MMAP ? "mmap" : ""),
				  (io_method == IO_METHOD_READ ? "read" : ""), (io_method == IO_METHOD_USERPTR ? "userptr" : ""));
		ctx->io = io_method;
	}
	else
	{
		LOG_ERROR("Could not set v4l2 io method, you have to get v4l2 instance first");
		return V4L2_CAPTURE_RETURN_FAILED;
	}
	return V4L2_CAPTURE_RETURN_OK;
}
int v4l2_h264_set_video_resolution(v4l2_capture_context ctx, video_resolution_e video_resolution)
{
	if (ctx != NULL)
	{
		LOG_INFOR("Set v4l2 video resolution to: %s%s%s", (video_resolution == VIDEO_RES_SD ? "SD" : ""),
				  (video_resolution == VIDEO_RES_HD ? "HD" : ""), (video_resolution == VIDEO_RES_FULL_HD ? "FullHD" : ""));
		ctx->force_format = video_resolution;
	}
	else
	{
		LOG_ERROR("Could not set v4l2 video resolution, you have to get v4l2 instance first");
		return V4L2_CAPTURE_RETURN_FAILED;
	}
	return V4L2_CAPTURE_RETURN_OK;
}

int get_v4l2_state(v4l2_capture_context ctx, v4l2_state_e *v4l2_state)
{
	if (ctx != NULL)
	{
		*v4l2_state = ctx->v4l2_capture_state;
	}
	else
	{
		LOG_ERROR("Could not get srt state, you have to get srt instance first");
		return V4L2_CAPTURE_RETURN_FAILED;
	}
	return V4L2_CAPTURE_RETURN_OK;
}

int set_v4l2_state(v4l2_capture_context ctx, v4l2_state_e v4l2_state)
{
	if (ctx != NULL)
	{
		if (v4l2_state == V4L2_STATE_START || v4l2_state == V4L2_STATE_STOP)
		{
			ctx->v4l2_capture_state = v4l2_state;
			LOG_INFOR("V4L2 state is set to : %s%s", (v4l2_state == V4L2_STATE_START ? "Start" : ""),
					  (v4l2_state == V4L2_STATE_STOP ? "Stop" : ""));
		}
		else
		{
			LOG_ERROR("v4l2 set state failed, v4l2_state is not correct");
			return V4L2_CAPTURE_RETURN_FAILED;
		}
	}
	else
	{
		LOG_ERROR("Could not set v4l2 state, you have to get v4l2 instance first");
		return V4L2_CAPTURE_RETURN_FAILED;
	}
	return V4L2_CAPTURE_RETURN_OK;
}