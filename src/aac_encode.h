#ifndef __AAC_ENCODE__
#define __AAC_ENCODE__

#include <fdk-aac/aacenc_lib.h>

#define AAC_ENC_OK (0)
#define AAC_ENC_FAILED (-1)

typedef void (*aac_handle_cb_t)(void *param, unsigned char *data, unsigned int data_len);

typedef struct aac_encode_context_s
{
    HANDLE_AACENCODER aac_enc_handle;
    unsigned int channels;
    unsigned int sample_rate;
    unsigned int aac_bitrate;
    int pcm_input_buffer_size;
    int aac_max_output_buffer_size;
    void *pcm_input_buffer;
    void *aac_output_buffer;
    AACENC_BufDesc inputBufDesc;
    AACENC_BufDesc outputBufDesc;
    AACENC_InArgs inputArgs;
    AACENC_OutArgs outputArgs;
    int in_elem_size;
    int in_identifier;
    int out_identifier;
    int out_elem_size;
    aac_handle_cb_t aac_handle_cb;
} aac_encode_context_s;

aac_encode_context_s *allocate_aac_encode_context();

void deallocate_aac_encode_context(aac_encode_context_s *ctx);

int init_aac_encode(aac_encode_context_s *ctx);

int destroy_aac_encode(aac_encode_context_s *ctx);

int start_aac_encode_pcm_data(aac_encode_context_s *ctx, void *input_buffer, unsigned int input_len, void *param);
#endif