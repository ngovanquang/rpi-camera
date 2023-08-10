#ifndef __MEDIA_STREAM__
#define __MEDIA_STREAM__


typedef enum {
    H264_SPS_FRAME = 1,
    H264_PPS_FRAME,
    H264_I_FRAME,
    H264_P_FRAME,
    AAC_FRAME
} media_frame_e;

#endif