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

}  // namespace media
