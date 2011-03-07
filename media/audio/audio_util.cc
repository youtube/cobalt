// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Software adjust volume of samples, allows each audio stream its own
// volume without impacting master volume for chrome and other applications.

// Implemented as templates to allow 8, 16 and 32 bit implementations.
// 8 bit is unsigned and biased by 128.

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_util.h"

namespace media {

// TODO(fbarchard): Convert to intrinsics for better efficiency.
template<class Fixed>
static int ScaleChannel(int channel, int volume) {
  return static_cast<int>((static_cast<Fixed>(channel) * volume) >> 16);
}

template<class Format, class Fixed, int bias>
static void AdjustVolume(Format* buf_out,
                         int sample_count,
                         int fixed_volume) {
  for (int i = 0; i < sample_count; ++i) {
    buf_out[i] = static_cast<Format>(ScaleChannel<Fixed>(buf_out[i] - bias,
                                                         fixed_volume) + bias);
  }
}

static const int kChannel_L = 0;
static const int kChannel_R = 1;
static const int kChannel_C = 2;

template<class Fixed, int min_value, int max_value>
static int AddChannel(int val, int adder) {
  Fixed sum = static_cast<Fixed>(val) + static_cast<Fixed>(adder);
  if (sum > max_value)
    return max_value;
  if (sum < min_value)
    return min_value;
  return static_cast<int>(sum);
}

// FoldChannels() downmixes multichannel (ie 5.1 Surround Sound) to Stereo.
// Left and Right channels are preserved asis, and Center channel is
// distributed equally to both sides.  To be perceptually 1/2 volume on
// both channels, 1/sqrt(2) is used instead of 1/2.
// Fixed point math is used for efficiency.  16 bits of fraction and 8,16 or 32
// bits of integer are used.
// 8 bit samples are unsigned and 128 represents 0, so a bias is removed before
// doing calculations, then readded for the final output.
template<class Format, class Fixed, int min_value, int max_value, int bias>
static void FoldChannels(Format* buf_out,
                         int sample_count,
                         const float volume,
                         int channels) {
  Format* buf_in = buf_out;
  const int center_volume = static_cast<int>(volume * 0.707f * 65536);
  const int fixed_volume = static_cast<int>(volume * 65536);

  for (int i = 0; i < sample_count; ++i) {
    int center = static_cast<int>(buf_in[kChannel_C] - bias);
    int left = static_cast<int>(buf_in[kChannel_L] - bias);
    int right = static_cast<int>(buf_in[kChannel_R] - bias);

    center = ScaleChannel<Fixed>(center, center_volume);
    left = ScaleChannel<Fixed>(left, fixed_volume);
    right = ScaleChannel<Fixed>(right, fixed_volume);

    buf_out[0] = static_cast<Format>(
        AddChannel<Fixed, min_value, max_value>(left, center) + bias);
    buf_out[1] = static_cast<Format>(
        AddChannel<Fixed, min_value, max_value>(right, center) + bias);

    buf_out += 2;
    buf_in += channels;
  }
}

// AdjustVolume() does an in place audio sample change.
bool AdjustVolume(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume) {
  DCHECK(buf);
  if (volume < 0.0f || volume > 1.0f)
    return false;
  if (volume == 1.0f) {
    return true;
  } else if (volume == 0.0f) {
    memset(buf, 0, buflen);
    return true;
  }
  if (channels > 0 && channels <= 8 && bytes_per_sample > 0) {
    int sample_count = buflen / bytes_per_sample;
    const int fixed_volume = static_cast<int>(volume * 65536);
    if (bytes_per_sample == 1) {
      AdjustVolume<uint8, int32, 128>(reinterpret_cast<uint8*>(buf),
                                      sample_count,
                                      fixed_volume);
      return true;
    } else if (bytes_per_sample == 2) {
      AdjustVolume<int16, int32, 0>(reinterpret_cast<int16*>(buf),
                                    sample_count,
                                    fixed_volume);
      return true;
    } else if (bytes_per_sample == 4) {
      AdjustVolume<int32, int64, 0>(reinterpret_cast<int32*>(buf),
                                    sample_count,
                                    fixed_volume);
      return true;
    }
  }
  return false;
}

bool FoldChannels(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume) {
  DCHECK(buf);
  if (volume < 0.0f || volume > 1.0f)
    return false;
  if (channels > 2 && channels <= 8 && bytes_per_sample > 0) {
    int sample_count = buflen / (channels * bytes_per_sample);
    if (bytes_per_sample == 1) {
      FoldChannels<uint8, int32, -128, 127, 128>(
          reinterpret_cast<uint8*>(buf),
          sample_count,
          volume,
          channels);
      return true;
    } else if (bytes_per_sample == 2) {
      FoldChannels<int16, int32, -32768, 32767, 0>(
          reinterpret_cast<int16*>(buf),
          sample_count,
          volume,
          channels);
      return true;
    } else if (bytes_per_sample == 4) {
      FoldChannels<int32, int64, 0x80000000, 0x7fffffff, 0>(
          reinterpret_cast<int32*>(buf),
          sample_count,
          volume,
          channels);
      return true;
    }
  }
  return false;
}

bool DeinterleaveAudioChannel(void* source,
                              float* destination,
                              int channels,
                              int channel_index,
                              int bytes_per_sample,
                              size_t number_of_frames) {
  switch (bytes_per_sample) {
    case 1:
    {
      uint8* source8 = static_cast<uint8*>(source) + channel_index;
      const float kScale = 1.0f / 128.0f;
      for (unsigned i = 0; i < number_of_frames; ++i) {
        destination[i] = kScale * static_cast<int>(*source8 + 128);
        source8 += channels;
      }
      return true;
    }

    case 2:
    {
      int16* source16 = static_cast<int16*>(source) + channel_index;
      const float kScale = 1.0f / 32768.0f;
      for (unsigned i = 0; i < number_of_frames; ++i) {
        destination[i] = kScale * *source16;
        source16 += channels;
      }
      return true;
    }

    case 4:
    {
      int32* source32 = static_cast<int32*>(source) + channel_index;
      const float kScale = 1.0f / (1L << 31);
      for (unsigned i = 0; i < number_of_frames; ++i) {
        destination[i] = kScale * *source32;
        source32 += channels;
      }
      return true;
    }

    default:
     break;
  }
  return false;
}

void InterleaveFloatToInt16(const std::vector<float*>& source,
                            int16* destination,
                            size_t number_of_frames) {
  const float kScale = 32768.0f;
  int channels = source.size();
  for (int i = 0; i < channels; ++i) {
    float* channel_data = source[i];
    for (size_t j = 0; j < number_of_frames; ++j) {
      float sample = kScale * channel_data[j];
      if (sample < -32768.0)
        sample = -32768.0;
      else if (sample > 32767.0)
        sample = 32767.0;

      destination[j * channels + i] = static_cast<int16>(sample);
    }
  }
}

}  // namespace media
