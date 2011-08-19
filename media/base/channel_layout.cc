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

const int kChannelOrderings[CHANNEL_LAYOUT_MAX][CHANNELS_MAX] = {
  // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR | StL | StR

  // CHANNEL_LAYOUT_NONE
  {  -1 , -1 , -1 , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_UNSUPPORTED
  {  -1 , -1 , -1 , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_MONO
  {  -1 , -1 , 0  , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_STEREO
  {  0  , 1  , -1 , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_2_1
  {  0  , 1  , -1 , -1  , -1 , -1 , -1    , -1    , 2  , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_SURROUND
  {  0  , 1  , 2  , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_4POINT0
  {  0  , 1  , 2  , -1  , -1 , -1 , -1    , -1    , 3  , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_2_2
  {  0  , 1  , -1 , -1  , -1 , -1 , -1    , -1    , -1 , 2  , 3  , -1  , -1 },

  // CHANNEL_LAYOUT_QUAD
  {  0  , 1  , -1 , -1  , 2  , 3  , -1    , -1    , -1 , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_5POINT0
  {  0  , 1  , 2  , -1  , -1 , -1 , -1    , -1    , -1 , 3  , 4  , -1  , -1 },

  // CHANNEL_LAYOUT_5POINT1
  {  0  , 1  , 2  , 3   , -1 , -1 , -1    , -1    , -1 , 4  , 5  , -1  , -1 },

  // CHANNEL_LAYOUT_5POINT0_BACK
  {  0  , 1  , 2  , -1  , 3  , 4  , -1    , -1    , -1 , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_5POINT1_BACK
  {  0  , 1  , 2  , 3   , 4  , 5  , -1    , -1    , -1 , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_7POINT0
  {  0  , 1  , 2  , -1  , 5  , 6  , -1    , -1    , -1 , 3  , 4  , -1  , -1 },

  // CHANNEL_LAYOUT_7POINT1
  {  0  , 1  , 2  , 3   , 6  , 7  , -1    , -1    , -1 , 4  , 5  , -1  , -1 },

  // CHANNEL_LAYOUT_7POINT1_WIDE
  {  0  , 1  , 2  , 3   , 4  , 5  , 6     , 7     , -1 , -1 , -1 , -1  , -1 },

  // CHANNEL_LAYOUT_STEREO_DOWNMIX
  {  -1 , -1 , -1 , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 , 0   , 1  },

  // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR | StL | StR
  };

int ChannelLayoutToChannelCount(ChannelLayout layout) {
  return kLayoutToChannels[layout];
}
