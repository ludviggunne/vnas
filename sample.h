#ifndef SAMPLE_H
#define SAMPLE_H

struct sample;

extern const char *const sample_mt;

void resample(unsigned long sample_rate);
const char *sample_path(struct sample *sample);
int sample_channels(struct sample *sample);
unsigned long sample_frames(struct sample *sample);
float *sample_data(struct sample *sample, int channel);

#endif
