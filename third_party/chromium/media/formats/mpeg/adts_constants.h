// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_

#include <stddef.h>

#include "third_party/chromium/media/base/channel_layout.h"
#include "third_party/chromium/media/base/media_export.h"

namespace media_m96 {

enum {
  kADTSHeaderMinSize = 7,
  kADTSHeaderSizeNoCrc = 7,
  kADTSHeaderSizeWithCrc = 9,
  kSamplesPerAACFrame = 1024,
};

MEDIA_EXPORT extern const int kADTSFrequencyTable[];
MEDIA_EXPORT extern const size_t kADTSFrequencyTableSize;

MEDIA_EXPORT extern const media_m96::ChannelLayout kADTSChannelLayoutTable[];
MEDIA_EXPORT extern const size_t kADTSChannelLayoutTableSize;

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
