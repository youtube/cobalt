// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/audio/audio_sink_parameters.h"

namespace media_m96 {

AudioSinkParameters::AudioSinkParameters() = default;
AudioSinkParameters::AudioSinkParameters(
    const base::UnguessableToken& session_id,
    const std::string& device_id)
    : session_id(session_id), device_id(device_id) {}
AudioSinkParameters::AudioSinkParameters(const AudioSinkParameters& params) =
    default;
AudioSinkParameters::~AudioSinkParameters() = default;

}  // namespace media_m96
