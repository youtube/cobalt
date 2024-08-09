// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/base/audio_decoder.h"

#include "third_party/chromium/media/base/audio_buffer.h"

namespace media_m96 {

AudioDecoder::AudioDecoder() = default;

AudioDecoder::~AudioDecoder() = default;

bool AudioDecoder::NeedsBitstreamConversion() const {
  return false;
}

}  // namespace media_m96
