// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_WEBM_OPUS_PACKET_BUILDER_H_
#define COBALT_MEDIA_FORMATS_WEBM_OPUS_PACKET_BUILDER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"

namespace media {

// From Opus RFC. See https://tools.ietf.org/html/rfc6716#page-14
enum OpusConstants {
  kNumPossibleOpusConfigs = 32,
  kMinOpusPacketFrameCount = 1,
  kMaxOpusPacketFrameCount = 48
};

class OpusPacket {
 public:
  OpusPacket(uint8_t config, uint8_t frame_count, bool is_VBR);
  ~OpusPacket();

  const uint8_t* data() const;
  int size() const;
  double duration_ms() const;

 private:
  std::vector<uint8_t> data_;
  double duration_ms_;

  DISALLOW_COPY_AND_ASSIGN(OpusPacket);
};

// Builds an exhaustive collection of Opus packet configurations.
ScopedVector<OpusPacket> BuildAllOpusPackets();

}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_WEBM_OPUS_PACKET_BUILDER_H_
