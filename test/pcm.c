#include <alsa/asoundlib.h>
#include <stdio.h>

#define PCM_DEVICE "default"  // Change this if needed

int main() {
    
    FILE *file;
    char *filename = "abc.pcm";
    int err;
    // open file
    file = fopen(filename, "wb");
    if (file == NULL)
    {
	printf("Unable to open the file.\n");
	return 1;
    }

    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int sample_rate = 44100;  // Sample rate (adjust as necessary)
    unsigned int channels = 1;         // Number of channels (1 for mono, 2 for stereo)
    unsigned int buffer_size = 1024;   // Buffer size (adjust as necessary)
    short buffer[buffer_size];

    // Open the PCM device for recording
    err = snd_pcm_open(&handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        printf("Unable to open PCM device: %s\n", snd_strerror(err));
        return 1;
    }

    // Configure parameters for recording
    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, channels);
    snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, 0);
    snd_pcm_hw_params_set_period_size(handle, params, buffer_size, 0);
    snd_pcm_hw_params(handle, params);
    snd_pcm_hw_params_free(params);

    // Start recording
    err = snd_pcm_prepare(handle);
    if (err < 0) {
        printf("Unable to prepare PCM device: %s\n", snd_strerror(err));
        return 1;
    }

    // Read PCM data in a loop
    while (1) {
        err = snd_pcm_readi(handle, buffer, buffer_size);
        if (err != buffer_size) {
            printf("Error reading from PCM device: %s\n", snd_strerror(err));
        }

        // Process the captured data here
        // ...
	fwrite(buffer, 2, buffer_size, file);
    }

    // Close the PCM device
    snd_pcm_close(handle);
    fclose(file);
    
    return 0;
}
