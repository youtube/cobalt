// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_ENCODED_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_ENCODED_VIDEO_FRAME_H_

#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {

class MockEncodedVideoFrame : public EncodedVideoFrame {
 public:
  MOCK_CONST_METHOD0(Data, base::span<const uint8_t>());
  MOCK_CONST_METHOD0(Codec, media::VideoCodec());
  MOCK_CONST_METHOD0(IsKeyFrame, bool());
  MOCK_CONST_METHOD0(ColorSpace, absl::optional<media::VideoColorSpace>());
  MOCK_CONST_METHOD0(Resolution, gfx::Size());
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_ENCODED_VIDEO_FRAME_H_
