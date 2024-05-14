
/* ------------------------------------------------------------------
 * Copyright (C) 2009 Martin Storsjo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#include "wav/dep_wavwriter.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dep_wav_writer {
  FILE* wav;
  int data_length;

  int format;
  int sample_rate;
  int bits_per_sample;
  int channels;
};

static void write_string(struct dep_wav_writer* ww, const char* str) {
  fputc(str[0], ww->wav);
  fputc(str[1], ww->wav);
  fputc(str[2], ww->wav);
  fputc(str[3], ww->wav);
}

static void write_int32(struct dep_wav_writer* ww, int value) {
  fputc((value >> 0) & 0xff, ww->wav);
  fputc((value >> 8) & 0xff, ww->wav);
  fputc((value >> 16) & 0xff, ww->wav);
  fputc((value >> 24) & 0xff, ww->wav);
}

static void write_int16(struct dep_wav_writer* ww, int value) {
  fputc((value >> 0) & 0xff, ww->wav);
  fputc((value >> 8) & 0xff, ww->wav);
}

static void write_header(struct dep_wav_writer* ww, int length) {
  int bytes_per_frame, bytes_per_sec;
  write_string(ww, "RIFF");
  write_int32(ww, 4 + 8 + 16 + 8 + length);
  write_string(ww, "WAVE");

  write_string(ww, "fmt ");
  write_int32(ww, 16);

  bytes_per_frame = ww->bits_per_sample / 8 * ww->channels;
  bytes_per_sec = bytes_per_frame * ww->sample_rate;
  write_int16(ww, ww->format);           // Format
  write_int16(ww, ww->channels);         // Channels
  write_int32(ww, ww->sample_rate);      // Samplerate
  write_int32(ww, bytes_per_sec);        // Bytes per sec
  write_int16(ww, bytes_per_frame);      // Bytes per frame
  write_int16(ww, ww->bits_per_sample);  // Bits per sample

  write_string(ww, "data");
  write_int32(ww, length);
}

void* dep_wav_write_open(const char* filename, int sample_rate, int bits_per_sample,
                     int channels) {
  return (dep_wav_write_open2(filename, DEP_WAVE_FORMAT_PCM, sample_rate,
                          bits_per_sample, channels));
}

void* dep_wav_write_open2(const char* filename, int format, int sample_rate,
                      int bits_per_sample, int channels) {
  struct dep_wav_writer* ww = (struct dep_wav_writer*)malloc(sizeof(*ww));
  memset(ww, 0, sizeof(*ww));
  ww->wav = fopen(filename, "wb");
  if (ww->wav == NULL) {
    free(ww);
    return NULL;
  }
  ww->data_length = 0;
  ww->format = format;
  ww->sample_rate = sample_rate;
  ww->bits_per_sample = bits_per_sample;
  ww->channels = channels;

  write_header(ww, ww->data_length);
  return ww;
}

void dep_wav_write_close(void* obj) {
  struct dep_wav_writer* ww = (struct dep_wav_writer*)obj;
  if (ww->wav == NULL) {
    free(ww);
    return;
  }
  fseek(ww->wav, 0, SEEK_SET);
  write_header(ww, ww->data_length);
  fclose(ww->wav);
  free(ww);
}

void dep_wav_write_data(void* obj, const unsigned char* data, int length) {
  struct dep_wav_writer* ww = (struct dep_wav_writer*)obj;
  if (ww->wav == NULL) return;
  fwrite(data, length, 1, ww->wav);
  ww->data_length += length;
}
