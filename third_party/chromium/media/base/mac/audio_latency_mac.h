// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_MAC_AUDIO_LATENCY_MAC_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_MAC_AUDIO_LATENCY_MAC_H_

#include "base/macros.h"
#include "third_party/chromium/media/base/media_shmem_export.h"

namespace media_m96 {

MEDIA_SHMEM_EXPORT int GetMinAudioBufferSizeMacOS(int min_buffer_size,
                                                  int sample_rate);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_MAC_AUDIO_LATENCY_MAC_H_
