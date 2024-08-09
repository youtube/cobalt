// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_FILTERS_FFMPEG_BITSTREAM_CONVERTER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_FILTERS_FFMPEG_BITSTREAM_CONVERTER_H_

#include "third_party/chromium/media/base/media_export.h"

struct AVPacket;

namespace media_m96 {

// Interface for classes that allow reformating of FFmpeg bitstreams
class MEDIA_EXPORT FFmpegBitstreamConverter {
 public:
  virtual ~FFmpegBitstreamConverter() {}

  // Reads the data in packet, and then overwrites this data with the
  // converted version of packet
  //
  // Returns false if conversion failed. In this case, |packet| should not be
  // changed.
  virtual bool ConvertPacket(AVPacket* packet) = 0;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_FILTERS_FFMPEG_BITSTREAM_CONVERTER_H_
