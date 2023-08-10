#ifndef __H264_CAPTURE__
#define __H264_CAPTURE__

#include "media_stream.h"

#define H264_SPS_FRAME_TYPE  (0x27)
#define H264_PPS_FRAME_TYPE  (0x28)
#define H264_I_FRAME_TYPE    (0x25)
#define H264_P_FRAME_TYPE    (0x21)



typedef enum io_method {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
} v4l2_io_method_e;

typedef enum VIDEO_RESOLUTION_E {
	VIDEO_RES_SD = 0,
	VIDEO_RES_HD,
	VIDEO_RES_FULL_HD
} video_resolution_e;

typedef enum {
    V4L2_STATE_STOP = 0,
    V4L2_STATE_START
} v4l2_state_e;

typedef struct buffer {
    void    *start;
    size_t  length;
} h264_frame_buffer;

#define V4L2_CAPTURE_RETURN_OK (0)
#define V4L2_CAPTURE_RETURN_FAILED (-1)

// typedef struct h264_dev_cfg {
//     int dev_fd;
//     const char* dev_name;
//     enum io_method io;
//     int force_format;
//     h264_frame_buffer *buffers;
//     unsigned int n_buffers;
// } h264_dev_cfg;

typedef struct v4l2_capture_context_t * v4l2_capture_context;

typedef void (*h264_process_frame_t)(const void *buffer, int size, media_frame_e media_type);

v4l2_capture_context get_v4l2_capture_instance(void);

void init_v4l2_h264_capture(v4l2_capture_context ctx);
void uninit_v4l2_h264_capture(v4l2_capture_context ctx);

void stop_v4l2_h264_capture(v4l2_capture_context ctx);
void start_v4l2_h264_capture(v4l2_capture_context ctx);

void v4l2_h264_mainloop(v4l2_capture_context ctx, h264_process_frame_t callback);

// void process_h264_frame(const void *p, int size);
void v4l2_h264_flip(v4l2_capture_context ctx);
void v4l2_h264_set_fps(v4l2_capture_context ctx, int fps);
void v4l2_h264_set_cbr(v4l2_capture_context ctx);

int v4l2_h264_set_device_name(v4l2_capture_context ctx, char* dev_name);
int v4l2_h264_set_io_method(v4l2_capture_context ctx, v4l2_io_method_e io_method);
int v4l2_h264_set_video_resolution(v4l2_capture_context ctx, video_resolution_e io_method);
int get_v4l2_state(v4l2_capture_context ctx, v4l2_state_e *v4l2_state);
int set_v4l2_state(v4l2_capture_context ctx, v4l2_state_e v4l2_state);
#endif