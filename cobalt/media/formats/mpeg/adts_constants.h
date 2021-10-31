// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
#define COBALT_MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_

#include "cobalt/media/base/channel_layout.h"
#include "cobalt/media/base/media_export.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

enum {
  kADTSHeaderMinSize = 7,
  kSamplesPerAACFrame = 1024,
};

MEDIA_EXPORT extern const int kADTSFrequencyTable[];
MEDIA_EXPORT extern const size_t kADTSFrequencyTableSize;

MEDIA_EXPORT extern const media::ChannelLayout kADTSChannelLayoutTable[];
MEDIA_EXPORT extern const size_t kADTSChannelLayoutTableSize;

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
