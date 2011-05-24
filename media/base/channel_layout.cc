// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/channel_layout.h"

static const int kLayoutToChannels[] = {
    0,   // CHANNEL_LAYOUT_NONE
    0,   // CHANNEL_LAYOUT_UNSUPPORTED
    1,   // CHANNEL_LAYOUT_MONO
    2,   // CHANNEL_LAYOUT_STEREO
    3,   // CHANNEL_LAYOUT_2_1
    3,   // CHANNEL_LAYOUT_SURROUND
    4,   // CHANNEL_LAYOUT_4POINT0
    4,   // CHANNEL_LAYOUT_2_2
    4,   // CHANNEL_LAYOUT_QUAD
    5,   // CHANNEL_LAYOUT_5POINT0
    6,   // CHANNEL_LAYOUT_5POINT1
    5,   // CHANNEL_LAYOUT_5POINT0_BACK
    6,   // CHANNEL_LAYOUT_5POINT1_BACK
    7,   // CHANNEL_LAYOUT_7POINT0
    8,   // CHANNEL_LAYOUT_7POINT1
    8,   // CHANNEL_LAYOUT_7POINT1_WIDE
    2};  // CHANNEL_LAYOUT_STEREO_DOWNMIX

int ChannelLayoutToChannelCount(ChannelLayout layout) {
  return kLayoutToChannels[layout];
}
