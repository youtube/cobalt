// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/simple_sources.h"

#include <algorithm>
#include <math.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_io.h"
#include "media/base/data_buffer.h"

//////////////////////////////////////////////////////////////////////////////
// SineWaveAudioSource implementation.

SineWaveAudioSource::SineWaveAudioSource(Format format, int channels,
                                         double freq, double sample_freq)
    : format_(format),
      channels_(channels),
      freq_(freq),
      sample_freq_(sample_freq),
      time_state_(0) {
  // TODO(cpu): support other formats.
  DCHECK((format_ == FORMAT_16BIT_LINEAR_PCM) && (channels_ == 1));
}

// The implementation could be more efficient if a lookup table is constructed
// but it is efficient enough for our simple needs.
uint32 SineWaveAudioSource::OnMoreData(
    AudioOutputStream* stream, uint8* dest, uint32 max_size,
    AudioBuffersState audio_buffers) {
  const double kTwoPi = 2.0 * 3.141592653589793;
  double f = freq_ / sample_freq_;
  int16* sin_tbl = reinterpret_cast<int16*>(dest);
  uint32 len = max_size / sizeof(int16);

  // The table is filled with s(t) = kint16max*sin(Theta*t),
  // where Theta = 2*PI*fs.
  // We store the discrete time value |t| in a member to ensure that the
  // next pass starts at a correct state.
  for (uint32 n = 0; n < len; ++n) {
    double theta = kTwoPi * f;
    sin_tbl[n] = static_cast<int16>(kint16max * sin(theta * time_state_));
    ++time_state_;
  }
  return max_size;
}

void SineWaveAudioSource::OnError(AudioOutputStream* stream, int code) {
  NOTREACHED();
}

//////////////////////////////////////////////////////////////////////////////
// PushSource implementation.

PushSource::PushSource()
    : buffer_(0, 0) {
}

PushSource::~PushSource() { }

uint32 PushSource::OnMoreData(
    AudioOutputStream* stream, uint8* dest, uint32 max_size,
    AudioBuffersState buffers_state) {
  return buffer_.Read(dest, max_size);
}

void PushSource::OnError(AudioOutputStream* stream, int code) {
  NOTREACHED();
}

// TODO(cpu): Manage arbitrary large sizes.
bool PushSource::Write(const void *data, uint32 len) {
  if (len == 0) {
    NOTREACHED();
    return false;
  }
  buffer_.Append(static_cast<const uint8*>(data), len);
  return true;
}

uint32 PushSource::UnProcessedBytes() {
  return buffer_.forward_bytes();
}

void PushSource::ClearAll() {
  // Cleanup() will discard all the data.
  CleanUp();
}

void PushSource::CleanUp() {
  buffer_.Clear();
}
