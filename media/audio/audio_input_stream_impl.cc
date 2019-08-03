// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/audio/audio_input_stream_impl.h"

namespace media {

static const int kMinIntervalBetweenVolumeUpdatesMs = 1000;

AudioInputStreamImpl::AudioInputStreamImpl()
    : agc_is_enabled_(false),
      max_volume_(0.0),
      normalized_volume_(0.0) {
}

AudioInputStreamImpl::~AudioInputStreamImpl() {}

void AudioInputStreamImpl::SetAutomaticGainControl(bool enabled) {
  agc_is_enabled_ = enabled;
}

bool AudioInputStreamImpl::GetAutomaticGainControl() {
  return agc_is_enabled_;
}

void AudioInputStreamImpl::UpdateAgcVolume() {
  base::AutoLock lock(lock_);

  // We take new volume samples once every second when the AGC is enabled.
  // To ensure that a new setting has an immediate effect, the new volume
  // setting is cached here. It will ensure that the next OnData() callback
  // will contain a new valid volume level. If this approach was not taken,
  // we could report invalid volume levels to the client for a time period
  // of up to one second.
  if (agc_is_enabled_) {
    GetNormalizedVolume();
  }
}

void AudioInputStreamImpl::QueryAgcVolume(double* normalized_volume) {
  base::AutoLock lock(lock_);

  // Only modify the |volume| output reference if AGC is enabled and if
  // more than one second has passed since the volume was updated the last time.
  if (agc_is_enabled_) {
    base::Time now = base::Time::Now();
    if ((now - last_volume_update_time_).InMilliseconds() >
      kMinIntervalBetweenVolumeUpdatesMs) {
        GetNormalizedVolume();
        last_volume_update_time_ = now;
    }
    *normalized_volume = normalized_volume_;
  }
}

void AudioInputStreamImpl::GetNormalizedVolume() {
  if (max_volume_ == 0.0) {
    // Cach the maximum volume if this is the first time we ask for it.
    max_volume_ = GetMaxVolume();
  }

  if (max_volume_ != 0.0) {
    // Retrieve the current volume level by asking the audio hardware.
    // Range is normalized to [0.0,1.0] or [0.0, 1.5] on Linux.
    normalized_volume_ = GetVolume() / max_volume_;
  }
}

}  // namespace media
