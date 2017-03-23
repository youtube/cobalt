// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/filters/source_buffer_platform.h"

#include "starboard/configuration.h"

namespace cobalt {
namespace media {

// TODO: These limits should be determinated during runtime for individual video
//       when multiple video tags are supported.
const size_t kSourceBufferAudioMemoryLimit =
    SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT;
const size_t kSourceBufferVideoMemoryLimit =
    SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT;

}  // namespace media
}  // namespace cobalt
