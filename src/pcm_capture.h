#ifndef __PCM_CAPTURE__
#define __PCM_CAPTURE__

#include <alsa/asoundlib.h>

#define PCM_DEFAULT_DEVICE "default"

#define PCM_FAILED (-1)
#define PCM_OK (0)

typedef enum
{
    MONO = 1,
    STEREO
} pcm_channels_e;

typedef enum
{
    PCM_CONTEXT_TYPE_CAPTURE = 1,
    PCM_CONTEXT_TYPE_PLAYBACK
} pcm_context_type_e;

typedef void (*pcm_capture_handle_t)(void *param, short *buffer, unsigned int buffer_size);

// Structure to hold the thread parameters
typedef struct pcm_capture_thread_params_s
{
    void *pcm_context;
    void *handle_pcm_context;
} pcm_capture_thread_params_s;

typedef struct pcm_capture_context_s
{
    pthread_t capture_pcm_thread;
    pcm_capture_thread_params_s *thead_params;
    pcm_capture_handle_t pcm_capture_handle_cb;
} pcm_capture_context_s;

typedef struct pcm_playback_context_s
{
    snd_pcm_uframes_t frames;
} pcm_playback_context_s;

typedef struct pcm_context_s
{
    const char *pcm_device;
    int pcm_context_type;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *param;
    unsigned int sample_rate;   // Sample rate
    unsigned int channels;      // Number of channels (1 for mono, 2 for stereo)
    snd_pcm_format_t format;    // pcm format
    short *data;              // buffer to hold pcm data
    unsigned int data_element;
    pcm_playback_context_s* pcm_playback_context;
    pcm_capture_context_s* pcm_capture_context;

} pcm_context_s;

/**
 * @brief allocate pcm context
 *
 * @return pcm_context_s*
 */
pcm_context_s *allocate_pcm_context(int pcm_context_type);

/**
 * @brief deallocate pcm context
 *
 * @param ctx
 */
void deallocate_pcm_context(pcm_context_s *ctx);

int init_pcm(pcm_context_s *ctx);

int destroy_pcm(pcm_context_s *ctx);

int start_pcm_capture(pcm_context_s *ctx, void *param);

int start_pcm_playback(pcm_context_s *ctx, char *data, uint16_t data_len);

#endif