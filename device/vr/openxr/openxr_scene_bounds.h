// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_OPENXR_SCENE_BOUNDS_H_
#define DEVICE_VR_OPENXR_OPENXR_SCENE_BOUNDS_H_

#include "device/vr/openxr/openxr_util.h"

namespace device {

struct OpenXrSceneBounds {
  OpenXrSceneBounds();
  ~OpenXrSceneBounds();
  XrSpace space_;
  XrTime time_;
  std::vector<XrSceneSphereBoundMSFT> sphere_bounds_;
  std::vector<XrSceneFrustumBoundMSFT> frustum_bounds_;
  std::vector<XrSceneOrientedBoxBoundMSFT> box_bounds_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SCENE_BOUNDS_H_