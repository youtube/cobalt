// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/formats/mpeg/adts_constants.h"

#include <array>

#include "base/cxx17_backports.h"

namespace media_m96 {

// The following conversion table is extracted from ISO 14496 Part 3 -
// Table 1.16 - Sampling Frequency Index.
const int kADTSFrequencyTable[] = {96000, 88200, 64000, 48000, 44100,
                                   32000, 24000, 22050, 16000, 12000,
                                   11025, 8000,  7350};
const size_t kADTSFrequencyTableSize = std::size(kADTSFrequencyTable);

// The following conversion table is extracted from ISO 14496 Part 3 -
// Table 1.17 - Channel Configuration.
const media_m96::ChannelLayout kADTSChannelLayoutTable[] = {
    media_m96::CHANNEL_LAYOUT_NONE,     media_m96::CHANNEL_LAYOUT_MONO,
    media_m96::CHANNEL_LAYOUT_STEREO,   media_m96::CHANNEL_LAYOUT_SURROUND,
    media_m96::CHANNEL_LAYOUT_4_0,      media_m96::CHANNEL_LAYOUT_5_0_BACK,
    media_m96::CHANNEL_LAYOUT_5_1_BACK, media_m96::CHANNEL_LAYOUT_7_1};
const size_t kADTSChannelLayoutTableSize = std::size(kADTSChannelLayoutTable);

}  // namespace media_m96
