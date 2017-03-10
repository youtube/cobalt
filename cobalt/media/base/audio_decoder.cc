// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/audio_decoder.h"

#include "cobalt/media/base/audio_buffer.h"

namespace cobalt {
namespace media {

AudioDecoder::AudioDecoder() {}

AudioDecoder::~AudioDecoder() {}

bool AudioDecoder::NeedsBitstreamConversion() const { return false; }

}  // namespace media
}  // namespace cobalt
