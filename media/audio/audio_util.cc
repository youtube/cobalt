// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Software adjust volume of samples, allows each audio stream its own
// volume without impacting master volume for chrome and other applications.

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_util.h"

namespace media {

// Done as C for future assembly optimization and/or to move to different class.
// TODO(fbarchard): add channels_in to allow number of channels to be reduced.

template<class Format>
void AdjustVolumeInternal(Format* buf_out,
                          int sample_count,
                          float volume) {
  for (int i = 0; i < sample_count; ++i) {
    buf_out[i] = static_cast<Format>(buf_out[i] * volume);
  }
}

// AdjustVolume() does an in place audio sample change.
bool AdjustVolume(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume) {
  DCHECK(buf);
  DCHECK(volume >= 0.0f && volume <= 1.0f);
  if (volume == 1.0f) {
    return true;
  } else if (volume == 0.0f) {
    memset(buf, 0, buflen);
    return true;
  }
  if (channels > 0 && channels <= 6 && bytes_per_sample > 0) {
    int sample_count = buflen / bytes_per_sample;
    if (bytes_per_sample == 1) {
      AdjustVolumeInternal(reinterpret_cast<uint8*>(buf),
                           sample_count,
                           volume);
      return true;
    } else if (bytes_per_sample == 2) {
      AdjustVolumeInternal(reinterpret_cast<int16*>(buf),
                           sample_count,
                           volume);
      return true;
    } else if (bytes_per_sample == 4) {
      AdjustVolumeInternal(reinterpret_cast<int32*>(buf),
                           sample_count,
                           volume);
      return true;
    }
  }
  return false;
}

// Channel order for AAC
// From http://www.hydrogenaudio.org/forums/lofiversion/index.php/t40046.html
// And Quicktime Pro, Movie Inspector
const int kChannel_C = 0;
const int kChannel_L = 1;
const int kChannel_R = 2;
const int kChannel_SL = 3;
const int kChannel_SR = 4;
const int kChannel_LFE = 5;

template<class Format>
Format ChannelsClampInternal(float val,
                             const Format min_value,
                             const Format max_value) {
  if (val > static_cast<float>(max_value)) {
    return max_value;
  }
  if (val < static_cast<float>(min_value)) {
    return min_value;
  }
  return static_cast<Format>(val);
}

template<class Format>
void FoldChannelsInternal(Format* buf_out,
                          int sample_count,
                          float volume,
                          int channels,
                          const int min_value,
                          const int max_value) {
  Format* buf_in = buf_out;
  // mid_value is to adjust excess 128 notation in unsigned 8 bit samples
  // to signed before adding channels.
  const int mid_value = (max_value + min_value + 1) / 2;
  const float kHalfPerceived = 0.707f; // 1/sqrt(2)
  for (int i = 0; i < sample_count; ++i) {
    float center_half = static_cast<float>(buf_in[kChannel_C] - mid_value) * kHalfPerceived;
    float left = static_cast<float>(buf_in[kChannel_L] - mid_value);
    float right = static_cast<float>(buf_in[kChannel_R] - mid_value);
    float surround_left = static_cast<float>(buf_in[kChannel_SL] - mid_value);
    float surround_right = static_cast<float>(buf_in[kChannel_SR] - mid_value);
    buf_out[0] = ChannelsClampInternal(
      (left + surround_left + center_half) * volume + mid_value,
      static_cast<Format>(min_value), static_cast<Format>(max_value));
    buf_out[1] = ChannelsClampInternal(
      (right + surround_right + center_half) * volume + mid_value,
      static_cast<Format>(min_value), static_cast<Format>(max_value));
    buf_out += 2;
    buf_in += channels;
  }
}

bool FoldChannels(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume) {
  DCHECK(buf);
  DCHECK(volume >= 0.0f && volume <= 1.0f);
  if (channels >= 5 && channels <= 6 && bytes_per_sample > 0) {
    int sample_count = buflen / (channels * bytes_per_sample);
    if (bytes_per_sample == 1) {
      FoldChannelsInternal(reinterpret_cast<uint8*>(buf),
                           sample_count,
                           volume,
                           channels,
                           0, 255);
      return true;
    } else if (bytes_per_sample == 2) {
      FoldChannelsInternal(reinterpret_cast<int16*>(buf),
                           sample_count,
                           volume,
                           channels,
                           -32768,
                           32767);
      return true;
    } else if (bytes_per_sample == 4) {
      FoldChannelsInternal(reinterpret_cast<int32*>(buf),
                           sample_count,
                           volume,
                           channels,
                           0x80000000,
                           0x7fffffff);
      return true;
    }
  }
  return false;
}

}  // namespace media
