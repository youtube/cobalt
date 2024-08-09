// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_VP8_PICTURE_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_VP8_PICTURE_H_

#include "base/macros.h"
#include "third_party/chromium/media/gpu/codec_picture.h"
#include "third_party/chromium/media/parsers/vp8_parser.h"

namespace media_m96 {

class V4L2VP8Picture;
class VaapiVP8Picture;

class VP8Picture : public CodecPicture {
 public:
  VP8Picture();

  virtual V4L2VP8Picture* AsV4L2VP8Picture();
  virtual VaapiVP8Picture* AsVaapiVP8Picture();

  std::unique_ptr<Vp8FrameHeader> frame_hdr;

 protected:
  ~VP8Picture() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VP8Picture);
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_VP8_PICTURE_H_
