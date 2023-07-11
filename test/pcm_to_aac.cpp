#include "pcm_capture.h"
#include "aac_encode.h"
#include "logger.h"
#include <stdio.h>

FILE *wfp = NULL;

void write_aac_data(void *param, unsigned char *data, unsigned int data_len)
{
    int ret;
    if (wfp != NULL)
    {
        ret = fwrite(data, 1, data_len, wfp);
        if (ret > 0)
        {
            LOG_INFOR("write: %d byte", ret);
        }
    }
}

void process_pcm_data(void *param, short *buffer, unsigned int buffer_size)
{
    aac_encode_context_s *aac_enc_ctx = (aac_encode_context_s *)param;
    if (aac_enc_ctx == NULL)
    {
        LOG_ERROR("process pcm data failed, param is NULL");
        exit(1);
    }

    start_aac_encode_pcm_data(aac_enc_ctx, (void *)buffer, buffer_size * 2, NULL);
}

int main(int argc, char **argv)
{
    wfp = fopen("output.aac", "wb+");
    if (wfp == NULL)
    {
        LOG_ERROR("Failed to open output.aac");
        return -1;
    }
    pcm_context_s *pcm_ctx = NULL;
    aac_encode_context_s *aac_enc_ctx = NULL;

    // Allocate contexts
    pcm_ctx = allocate_pcm_context();
    if (pcm_ctx == NULL)
    {
        return 1;
    }
    aac_enc_ctx = allocate_aac_encode_context();
    if (aac_enc_ctx == NULL)
    {
        return 1;
    }

    // set context
    pcm_ctx->sample_rate = 44100;
    pcm_ctx->buffer_size = 1024;
    pcm_ctx->channels = MONO;
    pcm_ctx->format = SND_PCM_FORMAT_S16_LE;
    pcm_ctx->pcm_handle_cb = process_pcm_data;

    aac_enc_ctx->sample_rate = pcm_ctx->sample_rate;
    aac_enc_ctx->aac_handle_cb = write_aac_data;

    // init all
    init_aac_encode(aac_enc_ctx);

    init_pcm_capture(pcm_ctx);

    // start capture
    start_pcm_capture(pcm_ctx, (void *)aac_enc_ctx);
    while (1)
    {
        usleep(20000);
    }

    // Destroy all
    fclose(wfp);
    destroy_pcm_capture(pcm_ctx);
    destroy_aac_encode(aac_enc_ctx);

    return 0;
}