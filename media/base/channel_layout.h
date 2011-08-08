// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CHANNEL_LAYOUT_H_
#define MEDIA_BASE_CHANNEL_LAYOUT_H_

#include "media/base/media_export.h"

enum ChannelLayout {
  CHANNEL_LAYOUT_NONE = 0,
  CHANNEL_LAYOUT_UNSUPPORTED,

  // Front C
  CHANNEL_LAYOUT_MONO,

  // Front L, Front R
  CHANNEL_LAYOUT_STEREO,

  // Front L, Front R, Back C
  CHANNEL_LAYOUT_2_1,

  // Front L, Front R, Front C
  CHANNEL_LAYOUT_SURROUND,

  // Front L, Front R, Front C, Back C
  CHANNEL_LAYOUT_4POINT0,

  // Front L, Front R, Side L, Side R
  CHANNEL_LAYOUT_2_2,

  // Front L, Front R, Back L, Back R
  CHANNEL_LAYOUT_QUAD,

  // Front L, Front R, Front C, Side L, Side R
  CHANNEL_LAYOUT_5POINT0,

  // Front L, Front R, Front C, Side L, Side R, LFE
  CHANNEL_LAYOUT_5POINT1,

  // Front L, Front R, Front C, Back L, Back R
  CHANNEL_LAYOUT_5POINT0_BACK,

  // Front L, Front R, Front C, Back L, Back R, LFE
  CHANNEL_LAYOUT_5POINT1_BACK,

  // Front L, Front R, Front C, Side L, Side R, Back L, Back R
  CHANNEL_LAYOUT_7POINT0,

  // Front L, Front R, Front C, Side L, Side R, LFE, Back L, Back R
  CHANNEL_LAYOUT_7POINT1,

  // Front L, Front R, Front C, Back L, Back R, LFE, Front LofC, Front RofC
  CHANNEL_LAYOUT_7POINT1_WIDE,

  // Stereo L, Stereo R
  CHANNEL_LAYOUT_STEREO_DOWNMIX,

  // Total number of layouts.
  CHANNEL_LAYOUT_MAX
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
extern const int kChannelOrderings[CHANNEL_LAYOUT_MAX][CHANNELS_MAX];

// Returns the number of channels in a given ChannelLayout.
MEDIA_EXPORT int ChannelLayoutToChannelCount(ChannelLayout layout);

#endif  // MEDIA_BASE_CHANNEL_LAYOUT_H_
