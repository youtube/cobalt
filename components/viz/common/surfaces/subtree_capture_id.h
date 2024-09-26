// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_H_

#include <cstdint>
#include <string>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// A SubtreeCaptureId uniquely identifies a layer subtree within a
// CompositorFrameSink, which can be captured independently from the root
// CompositorFrameSink by the FrameSinkVideoCapturer.
//
// Use the SubtreeCaptureIdAllocator to allocate a valid instace of this class.
class VIZ_COMMON_EXPORT SubtreeCaptureId {
 public:
  constexpr SubtreeCaptureId() = default;
  constexpr explicit SubtreeCaptureId(uint32_t subtree_id)
      : subtree_id_(subtree_id) {}
  constexpr SubtreeCaptureId(const SubtreeCaptureId&) = default;
  SubtreeCaptureId& operator=(const SubtreeCaptureId&) = default;
  ~SubtreeCaptureId() = default;

  constexpr bool is_valid() const { return subtree_id_ != 0; }
  constexpr uint32_t subtree_id() const { return subtree_id_; }

  bool operator==(const SubtreeCaptureId& rhs) const {
    return subtree_id_ == rhs.subtree_id_;
  }
  bool operator!=(const SubtreeCaptureId& rhs) const { return !(*this == rhs); }
  bool operator<(const SubtreeCaptureId& rhs) const {
    return subtree_id_ < rhs.subtree_id_;
  }

  std::string ToString() const;

 private:
  uint32_t subtree_id_ = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_H_
