// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CHANNEL_LAYOUT_H_
#define MEDIA_BASE_CHANNEL_LAYOUT_H_

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
  CHANNEL_LAYOUT_STEREO_DOWNMIX
};

int ChannelLayoutToChannelCount(ChannelLayout layout);

#endif  // MEDIA_BASE_CHANNEL_LAYOUT_H_
