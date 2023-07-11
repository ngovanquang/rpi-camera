#include "pcm_capture.h"
#include "logger.h"
#include <pthread.h>
#include <stdlib.h>

pcm_context_s *allocate_pcm_context(int pcm_context_type)
{

    pcm_context_s *ctx = (pcm_context_s *)malloc(sizeof(pcm_context_s));
    if (ctx == NULL)
    {
        LOG_ERROR("Unable to allocate memory for pcm");
        return NULL;
    }

    ctx->handle = NULL;
    ctx->pcm_device = PCM_DEFAULT_DEVICE;

    ctx->channels = MONO;
    ctx->data = NULL;
    ctx->data_element = 0;
    ctx->format = SND_PCM_FORMAT_S16_LE;
    ctx->param = NULL;
    ctx->sample_rate = 0;

    if (pcm_context_type == PCM_CONTEXT_TYPE_CAPTURE)
    {
        ctx->pcm_context_type = PCM_CONTEXT_TYPE_CAPTURE;
        ctx->pcm_capture_context = (pcm_capture_context_s *)malloc(sizeof(pcm_capture_context_s));
        if (ctx == NULL)
        {
            LOG_ERROR("Unable to allocate memory for pcm capture context");
            return NULL;
        }

        pcm_capture_thread_params_s *thread_params = (pcm_capture_thread_params_s *)malloc(sizeof(pcm_capture_thread_params_s));
        if (thread_params == NULL)
        {
            LOG_ERROR("Unable to allocate memory for pcm thread params");
            return NULL;
        }

        ctx->pcm_capture_context->capture_pcm_thread;
        ctx->pcm_capture_context->thead_params = thread_params;
        ctx->pcm_capture_context->thead_params->handle_pcm_context = NULL;
        ctx->pcm_capture_context->thead_params->pcm_context = ctx;
        ctx->pcm_capture_context->pcm_capture_handle_cb = NULL;
    }
    else
    {
        ctx->pcm_context_type = PCM_CONTEXT_TYPE_PLAYBACK;
        ctx->pcm_playback_context = (pcm_playback_context_s *)malloc(sizeof(pcm_playback_context_s));
        if (ctx == NULL)
        {
            LOG_ERROR("Unable to allocate memory for pcm capture context");
            return NULL;
        }
        ctx->pcm_playback_context->frames = 0;
    }

    return ctx;
}

void deallocate_pcm_context(pcm_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_DEBUG("deallocate pcm failed, ctx is already NULL");
    }

    // free(ctx->thead_params);
    free(ctx);
    ctx = NULL;
}

int init_pcm(pcm_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_ERROR("pcm isn't allocate");
        return PCM_FAILED;
    }

    // Open the PCM device for recording
    int ret;
    if (ctx->pcm_context_type == PCM_CONTEXT_TYPE_CAPTURE)
    {
        ret = snd_pcm_open(&ctx->handle, ctx->pcm_device, SND_PCM_STREAM_CAPTURE, 0);
    }
    else
    {
        ret = snd_pcm_open(&ctx->handle, ctx->pcm_device, SND_PCM_STREAM_PLAYBACK, 0);
    }

    if (ret < 0)
    {
        LOG_ERROR("Unable to open PCM device: %s", snd_strerror(ret));
        return PCM_FAILED;
    }

    // Allocate and initialize hardware parameters structure
    snd_pcm_hw_params_malloc(&ctx->param);
    snd_pcm_hw_params_any(ctx->handle, ctx->param);

    // Set hardware parameters
    ret = snd_pcm_hw_params_set_access(ctx->handle, ctx->param, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0)
    {
        LOG_ERROR("Error setting access type: %s", snd_strerror(ret));
        return PCM_FAILED;
    }
    ret = snd_pcm_hw_params_set_format(ctx->handle, ctx->param, ctx->format);
    if (ret < 0)
    {
        LOG_ERROR("Error setting sample format: %s", snd_strerror(ret));
        return PCM_FAILED;
    }
    ret = snd_pcm_hw_params_set_channels(ctx->handle, ctx->param, ctx->channels);
    if (ret < 0)
    {
        LOG_ERROR("Error setting channel count: %s", snd_strerror(ret));
        return PCM_FAILED;
    }
    ret = snd_pcm_hw_params_set_rate_near(ctx->handle, ctx->param, &ctx->sample_rate, 0);
    if (ret < 0)
    {
        LOG_ERROR("Error setting sample rate: %s", snd_strerror(ret));
        return PCM_FAILED;
    }
    if (ctx->pcm_context_type == PCM_CONTEXT_TYPE_PLAYBACK)
    {
        ret = snd_pcm_hw_params_set_period_size_near(ctx->handle, ctx->param, &(ctx->pcm_playback_context->frames), 0);
    }
    else
    {
        ret = snd_pcm_hw_params_set_period_size(ctx->handle, ctx->param, ctx->data_element, 0);
    }
    if (ret < 0)
    {
        LOG_ERROR("Error setting period size: %s", snd_strerror(ret));
        return PCM_FAILED;
    }

    if (ctx->pcm_context_type == PCM_CONTEXT_TYPE_PLAYBACK)
    {
        ctx->data_element = (ctx->pcm_playback_context->frames) * (ctx->channels);
    }

    if (ctx->data_element > 0 && ctx->data == NULL)
    {
        ctx->data = (short *)malloc(ctx->data_element * sizeof(short));
        if (ctx->data == NULL)
        {
            LOG_ERROR("Unable to allocate pcm buffer");
            return PCM_FAILED;
        }
    }

    // Apply hardware parameters
    snd_pcm_hw_params(ctx->handle, ctx->param);
    if (ret < 0)
    {
        LOG_ERROR("Error setting hardware parameters: %s\n", snd_strerror(ret));
        return PCM_FAILED;
    }
    snd_pcm_hw_params_free(ctx->param);

    // Prepare for recording
    ret = snd_pcm_prepare(ctx->handle);
    if (ret < 0)
    {
        LOG_ERROR("Unable to prepare PCM device: %s", snd_strerror(ret));
        return PCM_FAILED;
    }

    return PCM_OK;
}

int destroy_pcm(pcm_context_s *ctx)
{
    if (ctx == NULL)
    {
        LOG_INFOR("pcm already destroyed");
        return PCM_OK;
    }

    // Close the PCM device
    free(ctx->data);
    ctx->data = NULL;
    snd_pcm_drain(ctx->handle);
    snd_pcm_close(ctx->handle);

    deallocate_pcm_context(ctx);
    return PCM_OK;
}

void *capture_thread(void *param)
{
    int ret;
    pcm_capture_thread_params_s *params = (pcm_capture_thread_params_s *)param;
    pcm_context_s *ctx = (pcm_context_s *)params->pcm_context;
    if (ctx == NULL)
    {
        LOG_ERROR("pcm context is NULL");
        pthread_exit(NULL);
    }
    while (1)
    {
        ret = snd_pcm_readi(ctx->handle, ctx->data, ctx->data_element);
        if (ret != ctx->data_element)
        {
            LOG_ERROR("Error reading from PCM devices: %s", snd_strerror(ret));
            break;
        }
        ctx->pcm_capture_context->pcm_capture_handle_cb(params->handle_pcm_context, ctx->data, ctx->data_element);
        // Process the captured data
    }
    pthread_exit(NULL);
}

int start_pcm_capture(pcm_context_s *ctx, void *param)
{
    if (ctx == NULL)
    {
        LOG_ERROR("Unable to start pcm capture, ctx is NULL");
        return PCM_FAILED;
    }
    ctx->pcm_capture_context->thead_params->pcm_context = ctx;
    ctx->pcm_capture_context->thead_params->handle_pcm_context = param;

    LOG_INFOR("Create a thread to capture pcm data");
    pthread_create(&(ctx->pcm_capture_context->capture_pcm_thread), NULL, capture_thread, (void *)(ctx->pcm_capture_context->thead_params));
    // Wait for the thread to finish
    // pthread_join(ctx->capture_pcm_thread, NULL);
    return PCM_OK;
}

int start_pcm_playback(pcm_context_s *ctx, char *data, uint16_t data_len)
{
    int len = data_len;
    int cnt = 0;
    int err;
    while (len != 0)
    {
        memcpy(ctx->data, data + (cnt * (ctx->data_element * 2)), (ctx->data_element * 2));
        len -= (ctx->data_element * 2);
        printf("LEN: %d\n", len);
        cnt++;
        // printf("frames: %d\n", ctx->pcm_playback_context->frames);
        err = snd_pcm_writei(ctx->handle, ctx->data, (ctx->pcm_playback_context->frames));
        if (err == -EPIPE)
        {
            LOG_ERROR("Buffer underrun occurred\n");
            snd_pcm_prepare(ctx->handle);
        }
        else if (err < 0)
        {
            LOG_ERROR("Error writing PCM data: %s", snd_strerror(err));
            break;
        }
    }
}