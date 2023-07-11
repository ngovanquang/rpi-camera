#include "aac_encode.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>


static const char *aac_get_error(AACENC_ERROR err)
{
    switch (err) {
    case AACENC_OK:
        return "No error";
    case AACENC_INVALID_HANDLE:
        return "Invalid handle";
    case AACENC_MEMORY_ERROR:
        return "Memory allocation error";
    case AACENC_UNSUPPORTED_PARAMETER:
        return "Unsupported parameter";
    case AACENC_INVALID_CONFIG:
        return "Invalid config";
    case AACENC_INIT_ERROR:
        return "Initialization error";
    case AACENC_INIT_AAC_ERROR:
        return "AAC library initialization error";
    case AACENC_INIT_SBR_ERROR:
        return "SBR library initialization error";
    case AACENC_INIT_TP_ERROR:
        return "Transport library initialization error";
    case AACENC_INIT_META_ERROR:
        return "Metadata library initialization error";
    case AACENC_ENCODE_ERROR:
        return "Encoding error";
    case AACENC_ENCODE_EOF:
        return "End of file";
    default:
        return "Unknown error";
    }
}

aac_encode_context_s *allocate_aac_encode_context()
{
    aac_encode_context_s *ctx = (aac_encode_context_s *)malloc(sizeof(aac_encode_context_s));
    if (ctx == NULL)
    {
        LOG_ERROR("Unable to allocate memory for aac encode");
        return NULL;
    }

    ctx->inputArgs = {0};
    ctx->inputBufDesc = {0};
    ctx->outputArgs = {0};
    ctx->outputBufDesc = {0};
    ctx->aac_enc_handle = NULL;
    ctx->aac_output_buffer = NULL;
    ctx->pcm_input_buffer = NULL;
    ctx->aac_max_output_buffer_size = 0;
    ctx->pcm_input_buffer_size = 0;
    ctx->aac_bitrate = 0;

    return ctx;
}

void deallocate_aac_encode_context(aac_encode_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_DEBUG("Deallocate aac encode failed, ctx is already NULL");
    }

    free(ctx);
    ctx = NULL;
}

static void printf_info(aac_encode_context_s *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    printf("----------- AAC INFO ----------\n");
    printf("address of pcm_input_buffer %p\n", (void *)&(ctx->pcm_input_buffer));
    printf("address of aac_ouput_buffer %p\n", (void *)&(ctx->aac_output_buffer));
    printf("value of pcm_input_buffer %p\n", (void *)(ctx->pcm_input_buffer));
    printf("value of aac_ouput_buffer %p\n", (void *)(ctx->aac_output_buffer));
    printf("sample rate: %d\n", ctx->sample_rate);
    printf("input buffer size: %d\n", ctx->pcm_input_buffer_size);
    printf("max output buffer size: %d\n", ctx->aac_max_output_buffer_size);
    printf("input buffer desc:\n");
    printf("numBufs: %d\n", ctx->inputBufDesc.numBufs);
    printf("buffer: %p\n", ctx->inputBufDesc.bufs);
    printf("bufSizes: %d\n", *(ctx->inputBufDesc.bufSizes));
    printf("buffElSizes: %d\n", *(ctx->inputBufDesc.bufElSizes));
    printf("output buffer desc:\n");
    printf("numBufs: %d\n", ctx->outputBufDesc.numBufs);
    printf("buffer: %p\n", ctx->outputBufDesc.bufs);
    printf("bufSizes: %d\n", *(ctx->outputBufDesc.bufSizes));
    printf("buffElSizes: %d\n", *(ctx->outputBufDesc.bufElSizes));
    printf("-------------------------------\n");
}

int init_aac_encode(aac_encode_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_ERROR("aac encode isn't allocate");
        return AAC_ENC_FAILED;
    }

    int ret;
    // Initialize the AAC Encoder
    ret = aacEncOpen(&ctx->aac_enc_handle, 0, 1);
    if (ret != AACENC_OK)
    {
        LOG_ERROR("Failed to open the AAC encoder: %d", ret);
        return AAC_ENC_FAILED;
    }

    // Set AAC encoder parameters
    ret = aacEncoder_SetParam(ctx->aac_enc_handle, AACENC_AOT, AOT_AAC_LC);
    if (ret != AACENC_OK)
    {
        LOG_ERROR("Unable to set AACENC_AOT: %d", ret);
        return AAC_ENC_FAILED;
    }

    ret = aacEncoder_SetParam(ctx->aac_enc_handle, AACENC_SAMPLERATE, ctx->sample_rate);
    if (ret != AACENC_OK)
    {
        LOG_ERROR("AAC Error: %s", aac_get_error((AACENC_ERROR)ret));
        return AAC_ENC_FAILED;
    }

    ret = aacEncoder_SetParam(ctx->aac_enc_handle, AACENC_CHANNELMODE, MODE_1);
    if (ret != AACENC_OK)
    {
        LOG_ERROR("AAC Error: %s", aac_get_error((AACENC_ERROR)ret));
        return AAC_ENC_FAILED;
    }

    ret = aacEncoder_SetParam(ctx->aac_enc_handle, AACENC_TRANSMUX, TT_MP4_ADTS);
    if (ret != AACENC_OK)
    {
        LOG_ERROR("AAC Error: %s", aac_get_error((AACENC_ERROR)ret));
        return AAC_ENC_FAILED;
    }

    // Initialize the encoder
    ret = aacEncEncode(ctx->aac_enc_handle, NULL, NULL, NULL, NULL);
    if (ret != AACENC_OK)
    {
        LOG_ERROR("AAC Error: %s", aac_get_error((AACENC_ERROR)ret));
        return AAC_ENC_FAILED;
    }

    // Allocate input and output buffers
    AACENC_InfoStruct info = {0};
    ret = aacEncInfo(ctx->aac_enc_handle, &info);
    if (ret != AACENC_OK)
    {
        LOG_ERROR("AAC Error: %s", aac_get_error((AACENC_ERROR)ret));
        return -1;
    }

    ctx->pcm_input_buffer_size = 1 * info.frameLength;
    ctx->aac_max_output_buffer_size = info.maxOutBufBytes;
    ctx->pcm_input_buffer = malloc(ctx->pcm_input_buffer_size * sizeof(uint16_t));
    if (ctx->pcm_input_buffer == NULL)
    {
        LOG_ERROR("Unable to allocate pcm_input_buffer");
        return AAC_ENC_FAILED;
    }
    ctx->aac_output_buffer = malloc(ctx->aac_max_output_buffer_size * sizeof(uint8_t));
    if (ctx->pcm_input_buffer == NULL)
    {
        LOG_ERROR("Unable to allocate aac ouput buffer");
        return AAC_ENC_FAILED;
    }

    ctx->in_elem_size = 2;
    ctx->in_identifier = IN_AUDIO_DATA;
    ctx->out_elem_size = 1;
    ctx->out_identifier = OUT_BITSTREAM_DATA;


    // ctx->inputArgs.numInSamples = ctx->inpub/2;
    
    ctx->inputBufDesc.numBufs = 1;
    ctx->inputBufDesc.bufs = &(ctx->pcm_input_buffer);
    ctx->inputBufDesc.bufferIdentifiers = &(ctx->in_identifier);
    ctx->inputBufDesc.bufSizes = &(ctx->pcm_input_buffer_size);
    ctx->inputBufDesc.bufElSizes = &(ctx->in_elem_size);
    ctx->inputArgs.numInSamples = 1024;

    ctx->outputBufDesc.numBufs = 1;
    ctx->outputBufDesc.bufs = &(ctx->aac_output_buffer);
    ctx->outputBufDesc.bufferIdentifiers = &(ctx->out_identifier);
    ctx->outputBufDesc.bufSizes = &(ctx->aac_max_output_buffer_size);
    ctx->outputBufDesc.bufElSizes = &(ctx->out_elem_size);

    printf_info(ctx);
    return AAC_ENC_OK;
}

int destroy_aac_encode(aac_encode_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_INFOR("aac encode already destroyed");
        return AAC_ENC_OK;
    }

    free(ctx->pcm_input_buffer);
    ctx->pcm_input_buffer = NULL;
    free(ctx->aac_output_buffer);
    ctx->aac_output_buffer = NULL;
    aacEncClose(&ctx->aac_enc_handle);
    deallocate_aac_encode_context(ctx);
    return AAC_ENC_OK;
}

int start_aac_encode_pcm_data(aac_encode_context_s *ctx, void *input_buffer, unsigned int input_len, void *param)
{
    int ret;
    memset(ctx->pcm_input_buffer, 0, ctx->pcm_input_buffer_size * 2);
    memset(ctx->aac_output_buffer, 0, ctx->aac_max_output_buffer_size);

    // copy pcm data
    memcpy(ctx->pcm_input_buffer, input_buffer, input_len);

    ret = aacEncEncode(ctx->aac_enc_handle, &ctx->inputBufDesc, &ctx->outputBufDesc, &ctx->inputArgs, &ctx->outputArgs);
    if (ret != AACENC_OK)
    {
        LOG_ERROR("AAC Error: %s", aac_get_error((AACENC_ERROR)ret));
        return AAC_ENC_FAILED;
    }

    // handle aac enc data
    ctx->aac_handle_cb(param, (unsigned char *)ctx->aac_output_buffer, ctx->outputArgs.numOutBytes);
    return AAC_ENC_OK;
}