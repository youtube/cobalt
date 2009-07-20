// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_UTIL_H_
#define MEDIA_AUDIO_AUDIO_UTIL_H_

#include "base/basictypes.h"

namespace media {

// AdjustVolume() does a software volume adjustment of a sample buffer.
// The samples are multiplied by the volume, which should range from
// 0.0 (mute) to 1.0 (full volume).
// Using software allows each audio and video to have its own volume without
// affecting the master volume.
// In the future the function may be used to adjust the sample format to
// simplify hardware requirements and to support a wider variety of input
// formats.
// The buffer is modified in-place to avoid memory management, as this
// function may be called in performance critical code.
bool AdjustVolume(void* buf,
                  size_t buflen,
                  int channels,
                  int bytes_per_sample,
                  float volume);

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_UTIL_H_
