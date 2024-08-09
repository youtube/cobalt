// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_VP8_REFERENCE_FRAME_VECTOR_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_VP8_REFERENCE_FRAME_VECTOR_H_

#include <array>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "third_party/chromium/media/parsers/vp8_parser.h"

namespace media_m96 {

class VP8Picture;

class Vp8ReferenceFrameVector {
 public:
  Vp8ReferenceFrameVector();

  Vp8ReferenceFrameVector(const Vp8ReferenceFrameVector&) = delete;
  Vp8ReferenceFrameVector& operator=(const Vp8ReferenceFrameVector&) = delete;

  ~Vp8ReferenceFrameVector();

  void Refresh(scoped_refptr<VP8Picture> pic);
  void Clear();

  scoped_refptr<VP8Picture> GetFrame(Vp8RefType type) const;

 private:
  std::array<scoped_refptr<VP8Picture>, kNumVp8ReferenceBuffers>
      reference_frames_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_VP8_REFERENCE_FRAME_VECTOR_H_
