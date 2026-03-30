#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ao/ao.h>

#define SAMPLE_RATE 44100

int main(int argc, char **argv){
    ao_device *device;
    ao_sample_format format;
    int default_driver;
    char *buffer;
    int buf_size;
    int sample;
    float freq = 440.0;
    int i;

    ao_initialize();
    default_driver = ao_default_driver_id();

    memset(&format, 0, sizeof(format));
    format.bits = 16;
    format.channels = 2;
    format.rate = SAMPLE_RATE;
    format.byte_format = AO_FMT_LITTLE;

    device = ao_open_live(default_driver, &format, NULL);
    if (device == NULL) {
        fprintf(stderr, "Error opening device.\n");
        return 1;
    }

    buf_size = format.bits/8 * format.channels * SAMPLE_RATE;
    buffer = calloc(buf_size, sizeof(char));

    for (i = 0; i < SAMPLE_RATE; i++) {
        sample = (int)(0.75 * 32768.0 * sin(2 * M_PI * freq * i / SAMPLE_RATE));
        buffer[4*i]   = buffer[4*i+2] = sample & 0xff;
        buffer[4*i+1] = buffer[4*i+3] = (sample >> 8) & 0xff;
    }

    ao_play(device, buffer, buf_size);
    ao_close(device);
    free(buffer);
    ao_shutdown();
    return 0;
}
