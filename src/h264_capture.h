#ifndef __H264_CAPTURE__
#define __H264_CAPTURE__

#include "media_stream.h"

enum io_method {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
};

typedef struct buffer {
    void    *start;
    size_t  length;
} h264_frame_buffer;

typedef struct h264_dev_cfg {
    int dev_fd;
    const char* dev_name;
    enum io_method io;
    int force_format;
    h264_frame_buffer *buffers;
    unsigned int n_buffers;
} h264_dev_cfg;

typedef void (*h264_process_frame_t)(const void *buffer, int size, int media_type);
void init_h264_device(h264_dev_cfg *dev_cfg);
void uninit_h264_device(h264_dev_cfg *dev_cfg);

void stop_h264_capturing(h264_dev_cfg* dev_cfg);
void start_h264_capturing(h264_dev_cfg* dev_cfg);

void h264_mainloop(h264_dev_cfg *dev_cfg, h264_process_frame_t callback);

void process_h264_frame(const void *p, int size);
void v4l2_h264_flip(h264_dev_cfg *dev_cfg);
void v4l2_h264_set_fps(h264_dev_cfg *dev_cfg);
void v4l2_h264_set_cbr(h264_dev_cfg *dev_cfg);
#endif