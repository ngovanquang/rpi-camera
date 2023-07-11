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

static int out_buf = 0;
static int frame_count = 0;
static int frame_number = 0;
int frame_type = 0;
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

static void process_image(const void *p, int size, h264_process_frame_t callback)
{
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
		if (mpeg_h264_find_keyframe((const uint8_t *)p, size))
		{
			frame_type = 1;
			// key_frame++;

			// Calculate the current time in milliseconds
			clock_gettime(CLOCK_MONOTONIC, &currentTime);
			long milliseconds = currentTime.tv_sec * 1000 + currentTime.tv_nsec / 1000000;
			keyframe_duration = milliseconds - last_milisecond;
			LOG_INFOR("\n\n\nKey Frame >> Frame Number: %d Size: %d >> %d\n\n\n\n", frame_number, size, keyframe_duration);
			last_milisecond = milliseconds;
		}
		else
		{
			frame_type = 0;
			// LOG_INFO("Frame Number: %d Size: %d", frame_number, size);
		}

		callback(p, size, H264_FRAME);
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

static int read_frame(h264_dev_cfg *dev_cfg, h264_process_frame_t callback)
{
	struct v4l2_buffer buf;
	unsigned int i;

	switch (dev_cfg->io)
	{
	case IO_METHOD_READ:
		if (-1 == read(dev_cfg->dev_fd, dev_cfg->buffers[0].start, dev_cfg->buffers[0].length))
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

		process_image(dev_cfg->buffers[0].start, dev_cfg->buffers[0].length, callback);
		break;

	case IO_METHOD_MMAP:
		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_DQBUF, &buf))
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

		assert(buf.index < dev_cfg->n_buffers);

		process_image(dev_cfg->buffers[buf.index].start, buf.bytesused, callback);

		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_QBUF, &buf))
		{
			LOG_ERROR("VIDIOC_QBUF error %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;

	case IO_METHOD_USERPTR:
		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;

		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_DQBUF, &buf))
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

		for (i = 0; i < dev_cfg->n_buffers; ++i)
			if (buf.m.userptr == (unsigned long)dev_cfg->buffers[i].start && buf.length == dev_cfg->buffers[i].length)
				break;

		assert(i < dev_cfg->n_buffers);

		process_image((void *)buf.m.userptr, buf.bytesused, callback);

		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_QBUF, &buf))
		{
			LOG_ERROR("VIDIOC_QBUF error %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;
	}

	return 1;
}

void h264_mainloop(h264_dev_cfg *dev_cfg, h264_process_frame_t callback)
{
	unsigned int count;
	unsigned int loopIsInfinite = 0;

	if (frame_count == 0)
		loopIsInfinite = 1; // infinite loop
	count = frame_count;

	while ((count-- > 0) || loopIsInfinite)
	{
		for (;;)
		{
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO(&fds);
			FD_SET(dev_cfg->dev_fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(dev_cfg->dev_fd + 1, &fds, NULL, NULL, &tv);

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

			if (read_frame(dev_cfg, callback))
				break;
			/* EAGAIN - continue select loop. */
		}
	}
}

void stop_h264_capturing(h264_dev_cfg *dev_cfg)
{
	enum v4l2_buf_type type;

	switch (dev_cfg->io)
	{
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_STREAMOFF, &type))
		{
			LOG_ERROR("VIDIOC_STREAMOFF %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;
	}
}

void start_h264_capturing(h264_dev_cfg *dev_cfg)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (dev_cfg->io)
	{
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < dev_cfg->n_buffers; ++i)
		{
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;

			if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_QBUF, &buf))
			{
				LOG_ERROR("VIDIOC_QBUF %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_STREAMON, &type))
		{
			LOG_ERROR("VIDIOC_STREAMON %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < dev_cfg->n_buffers; ++i)
		{
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.index = i;
			buf.m.userptr = (unsigned long)dev_cfg->buffers[i].start;
			buf.length = dev_cfg->buffers[i].length;

			if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_QBUF, &buf))
			{
				LOG_ERROR("VIDIOC_QBUF %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_STREAMON, &type))
		{
			LOG_ERROR("VIDIOC_STREAMON %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		break;
	}
}

static void init_read(h264_dev_cfg *dev_cfg, unsigned int buffer_size)
{
	dev_cfg->buffers = (h264_frame_buffer *)calloc(1, sizeof(*dev_cfg->buffers));

	if (!dev_cfg->buffers)
	{
		LOG_ERROR("Out of memmory\n");
		exit(EXIT_FAILURE);
	}

	dev_cfg->buffers[0].length = buffer_size;
	dev_cfg->buffers[0].start = malloc(buffer_size);

	if (!dev_cfg->buffers[0].start)
	{
		LOG_ERROR("Out of memmory\n");
		exit(EXIT_FAILURE);
	}
}

static void init_mmap(h264_dev_cfg *dev_cfg)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno)
		{
			LOG_ERROR("%s does not support memory mapping\n", dev_cfg->dev_name);
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
		LOG_ERROR("Insufficient buffer memory on %s\n", dev_cfg->dev_name);
		exit(EXIT_FAILURE);
	}

	dev_cfg->buffers = (h264_frame_buffer *)calloc(req.count, sizeof(*dev_cfg->buffers));

	if (!dev_cfg->buffers)
	{
		LOG_ERROR("Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (dev_cfg->n_buffers = 0; dev_cfg->n_buffers < req.count; ++dev_cfg->n_buffers)
	{
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = dev_cfg->n_buffers;

		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_QUERYBUF, &buf))
		{
			LOG_ERROR("VIDIOC_QUERYBUF %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		dev_cfg->buffers[dev_cfg->n_buffers].length = buf.length;
		dev_cfg->buffers[dev_cfg->n_buffers].start =
			mmap(NULL /* start anywhere */,
				 buf.length,
				 PROT_READ | PROT_WRITE /* required */,
				 MAP_SHARED /* recommended */,
				 dev_cfg->dev_fd, buf.m.offset);

		if (MAP_FAILED == dev_cfg->buffers[dev_cfg->n_buffers].start)
		{
			LOG_ERROR("mmap %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

static void init_userp(h264_dev_cfg *dev_cfg, unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno)
		{
			LOG_ERROR("%s does not support user pointer i/o\n", dev_cfg->dev_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			LOG_ERROR("VIDIOC_REQBUFS %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	dev_cfg->buffers = (h264_frame_buffer *)calloc(4, sizeof(*dev_cfg->buffers));

	if (!dev_cfg->buffers)
	{
		LOG_ERROR("Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (dev_cfg->n_buffers = 0; dev_cfg->n_buffers < 4; ++dev_cfg->n_buffers)
	{
		dev_cfg->buffers[dev_cfg->n_buffers].length = buffer_size;
		dev_cfg->buffers[dev_cfg->n_buffers].start = malloc(buffer_size);

		if (!dev_cfg->buffers[dev_cfg->n_buffers].start)
		{
			LOG_ERROR("Out of memory\n");
			exit(EXIT_FAILURE);
		}
	}
}

static void open_device(h264_dev_cfg *dev_cfg)
{
	struct stat st;

	if (-1 == stat(dev_cfg->dev_name, &st))
	{
		LOG_ERROR("Cannot identify '%s': %d, %s\n", dev_cfg->dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR(st.st_mode))
	{
		LOG_ERROR("%s is no device\n", dev_cfg->dev_name);
		exit(EXIT_FAILURE);
	}

	// dev_cfg->dev_fd = open(dev_cfg->dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
	dev_cfg->dev_fd = open(dev_cfg->dev_name, O_RDWR | O_NONBLOCK /* required */, 0);

	if (-1 == dev_cfg->dev_fd)
	{
		LOG_ERROR("Cannot open '%s': %d, %s\n", dev_cfg->dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	LOG_INFOR("Open device: %s success\n", dev_cfg->dev_name);
}

void init_h264_device(h264_dev_cfg *dev_cfg)
{
	open_device(dev_cfg);

	// init
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			LOG_ERROR("%s is no V4L2 device\n", dev_cfg->dev_name);
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
		LOG_ERROR("%s is no video capture device\n", dev_cfg->dev_name);
		exit(EXIT_FAILURE);
	}

	switch (dev_cfg->io)
	{
	case IO_METHOD_READ:
		if (!(cap.capabilities & V4L2_CAP_READWRITE))
		{
			LOG_ERROR("%s does not support read i/o\n", dev_cfg->dev_name);
			exit(EXIT_FAILURE);
		}
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING))
		{
			LOG_ERROR("%s does not support streaming i/o\n", dev_cfg->dev_name);
			exit(EXIT_FAILURE);
		}
		break;
	}

	/* Select video input, video standard and tune here. */

	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(dev_cfg->dev_fd, VIDIOC_CROPCAP, &cropcap))
	{
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_S_CROP, &crop))
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
	LOG_INFOR("Force Format %d\n", dev_cfg->force_format);
	if (dev_cfg->force_format)
	{
		if (dev_cfg->force_format == 2)
		{
			fmt.fmt.pix.width = 1280;
			fmt.fmt.pix.height = 720;
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
			fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
		}
		else if (dev_cfg->force_format == 1)
		{
			fmt.fmt.pix.width = 640;
			fmt.fmt.pix.height = 482;
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
			fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
		}
		else if (dev_cfg->force_format == 3)
		{
			fmt.fmt.pix.width = 640;
			fmt.fmt.pix.height = 482;
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
			fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
		}

		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_S_FMT, &fmt))
		{
			LOG_ERROR("VIDIOC_S_FMT %d, %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		/* Note VIDIOC_S_FMT may change width and height. */
	}
	else
	{
		/* Preserve original settings as set by v4l2-ctl for example */
		if (-1 == xioctl(dev_cfg->dev_fd, VIDIOC_G_FMT, &fmt))
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

	switch (dev_cfg->io)
	{
	case IO_METHOD_READ:
		init_read(dev_cfg, fmt.fmt.pix.sizeimage);
		break;

	case IO_METHOD_MMAP:
		init_mmap(dev_cfg);
		break;

	case IO_METHOD_USERPTR:
		init_userp(dev_cfg, fmt.fmt.pix.sizeimage);
		break;
	}
}

void uninit_h264_device(h264_dev_cfg *dev_cfg)
{
	unsigned int i;

	switch (dev_cfg->io)
	{
	case IO_METHOD_READ:
		free(dev_cfg->buffers[0].start);
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < dev_cfg->n_buffers; ++i)
			if (-1 == munmap(dev_cfg->buffers[i].start, dev_cfg->buffers[i].length))
			{
				LOG_ERROR("munmap %d, %s\n", errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < dev_cfg->n_buffers; ++i)
			free(dev_cfg->buffers[i].start);
		break;
	}

	free(dev_cfg->buffers);

	// close device
	if (-1 == close(dev_cfg->dev_fd))
	{
		LOG_ERROR("close %d, %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	dev_cfg->dev_fd = -1;
}

void v4l2_h264_flip(h264_dev_cfg *dev_cfg)
{
	// Set the camera flip control to flip vertically
	struct v4l2_control ctrl;
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_VFLIP;
	ctrl.value = 1;
	if (xioctl(dev_cfg->dev_fd, VIDIOC_S_CTRL, &ctrl) == -1)
	{
		LOG_ERROR("Failed to set camera flip");
		close(dev_cfg->dev_fd);
		exit(EXIT_FAILURE);
	}
}

void v4l2_h264_set_fps(h264_dev_cfg *dev_cfg)
{
	struct v4l2_streamparm stream_param;
	memset(&stream_param, 0, sizeof(struct v4l2_streamparm));
	stream_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(dev_cfg->dev_fd, VIDIOC_G_PARM, &stream_param) == -1)
	{
		perror("Failed to get stream parameters");
		exit(EXIT_FAILURE);
	}

	stream_param.parm.capture.timeperframe.numerator = 1;
	stream_param.parm.capture.timeperframe.denominator = 25;

	if (ioctl(dev_cfg->dev_fd, VIDIOC_S_PARM, &stream_param) == -1)
	{
		perror("Failed to set stream parameters");
		exit(EXIT_FAILURE);
	}
}

void v4l2_h264_set_cbr(h264_dev_cfg *dev_cfg)
{
	// Set CBR (Constant Bit Rate) instead of VBR (Variable Bit Rate)

	struct v4l2_control ctrl;
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
	ctrl.value = 2 * 1024 * 1024;
	if (xioctl(dev_cfg->dev_fd, VIDIOC_S_CTRL, &ctrl) == -1)
	{
		LOG_ERROR("Failed to set cbr");
		close(dev_cfg->dev_fd);
		exit(EXIT_FAILURE);
	}

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE;
	ctrl.value = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
	if (xioctl(dev_cfg->dev_fd, VIDIOC_S_CTRL, &ctrl) == -1)
	{
		LOG_ERROR("Failed to set cbr");
		close(dev_cfg->dev_fd);
		exit(EXIT_FAILURE);
	}
}
