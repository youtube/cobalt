// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_VP9_REFERENCE_FRAME_VECTOR_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_VP9_REFERENCE_FRAME_VECTOR_H_

#include <array>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "third_party/chromium/media/filters/vp9_parser.h"

namespace media_m96 {

class VP9Picture;

// This class encapsulates VP9-specific reference frame management code. This
// class is thread afine.
class Vp9ReferenceFrameVector {
 public:
  Vp9ReferenceFrameVector();

  Vp9ReferenceFrameVector(const Vp9ReferenceFrameVector&) = delete;
  Vp9ReferenceFrameVector& operator=(const Vp9ReferenceFrameVector&) = delete;

  ~Vp9ReferenceFrameVector();

  void Refresh(scoped_refptr<VP9Picture> pic);
  void Clear();

  scoped_refptr<VP9Picture> GetFrame(size_t index) const;

 private:
  std::array<scoped_refptr<VP9Picture>, kVp9NumRefFrames> reference_frames_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_VP9_REFERENCE_FRAME_VECTOR_H_
