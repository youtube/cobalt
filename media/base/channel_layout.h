// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CHANNEL_LAYOUT_H_
#define MEDIA_BASE_CHANNEL_LAYOUT_H_

#include "media/base/media_export.h"

// Enumerates the various representations of the ordering of audio channels.
// Logged to UMA, so never reuse a value, always add new/greater ones!
enum ChannelLayout {
  CHANNEL_LAYOUT_NONE = 0,
  CHANNEL_LAYOUT_UNSUPPORTED = 1,

  // Front C
  CHANNEL_LAYOUT_MONO = 2,

  // Front L, Front R
  CHANNEL_LAYOUT_STEREO = 3,

  // Front L, Front R, Back C
  CHANNEL_LAYOUT_2_1 = 4,

  // Front L, Front R, Front C
  CHANNEL_LAYOUT_SURROUND = 5,

  // Front L, Front R, Front C, Back C
  CHANNEL_LAYOUT_4_0 = 6,

  // Front L, Front R, Side L, Side R
  CHANNEL_LAYOUT_2_2 = 7,

  // Front L, Front R, Back L, Back R
  CHANNEL_LAYOUT_QUAD = 8,

  // Front L, Front R, Front C, Side L, Side R
  CHANNEL_LAYOUT_5_0 = 9,

  // Front L, Front R, Front C, Side L, Side R, LFE
  CHANNEL_LAYOUT_5_1 = 10,

  // Front L, Front R, Front C, Back L, Back R
  CHANNEL_LAYOUT_5_0_BACK = 11,

  // Front L, Front R, Front C, Back L, Back R, LFE
  CHANNEL_LAYOUT_5_1_BACK = 12,

  // Front L, Front R, Front C, Side L, Side R, Back L, Back R
  CHANNEL_LAYOUT_7_0 = 13,

  // Front L, Front R, Front C, Side L, Side R, LFE, Back L, Back R
  CHANNEL_LAYOUT_7_1 = 14,

  // Front L, Front R, Front C, Back L, Back R, LFE, Front LofC, Front RofC
  CHANNEL_LAYOUT_7_1_WIDE = 15,

  // Stereo L, Stereo R
  CHANNEL_LAYOUT_STEREO_DOWNMIX = 16,

  // Total number of layouts.
  CHANNEL_LAYOUT_MAX  // Must always be last!
};

enum Channels {
  LEFT = 0,
  RIGHT,
  CENTER,
  LFE,
  BACK_LEFT,
  BACK_RIGHT,
  LEFT_OF_CENTER,
  RIGHT_OF_CENTER,
  BACK_CENTER,
  SIDE_LEFT,
  SIDE_RIGHT,
  STEREO_LEFT,
  STEREO_RIGHT,
  CHANNELS_MAX
};

// The channel orderings for each layout as specified by FFmpeg.
// Values represent the index of each channel in each layout. For example, the
// left side surround sound channel in FFmpeg's 5.1 layout is in the 5th
// position (because the order is L, R, C, LFE, LS, RS), so
// kChannelOrderings[CHANNEL_LAYOUT_5POINT1][SIDE_LEFT] = 4;
// Values of -1 mean the channel at that index is not used for that layout.
MEDIA_EXPORT extern const int
kChannelOrderings[CHANNEL_LAYOUT_MAX][CHANNELS_MAX];

// Returns the number of channels in a given ChannelLayout.
MEDIA_EXPORT int ChannelLayoutToChannelCount(ChannelLayout layout);

#endif  // MEDIA_BASE_CHANNEL_LAYOUT_H_
