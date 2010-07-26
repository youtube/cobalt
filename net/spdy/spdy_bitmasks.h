// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_BITMASKS_H_
#define NET_SPDY_SPDY_BITMASKS_H_
#pragma once

namespace spdy {

// StreamId mask from the SpdyHeader
const unsigned int kStreamIdMask = 0x7fffffff;

// Control flag mask from the SpdyHeader
const unsigned int kControlFlagMask = 0x8000;

// Priority mask from the SYN_FRAME
const unsigned int kPriorityMask = 0xc0;

// Mask the lower 24 bits.
const unsigned int kLengthMask = 0xffffff;

// Mask the Id from a SETTINGS id.
const unsigned int kSettingsIdMask = 0xffffff;

// Legal flags on data packets.
const int kDataFlagsMask = 0x03;

// Legal flags on control packets.
const int kControlFlagsMask = 0x03;

}  // namespace spdy

#endif  // NET_SPDY_SPDY_BITMASKS_H_
