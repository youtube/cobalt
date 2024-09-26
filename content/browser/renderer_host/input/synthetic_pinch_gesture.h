// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PINCH_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PINCH_GESTURE_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"

namespace content {

// SyntheticPinchGesture is a thin wrapper around either
// SyntheticTouchscreenPinchGesture or SyntheticTouchpadPinchGesture, depending
// on the SyntheticGestureParam's |input_type| and the default input type of the
// target.
class CONTENT_EXPORT SyntheticPinchGesture : public SyntheticGesture {
 public:
  explicit SyntheticPinchGesture(const SyntheticPinchGestureParams& params);

  SyntheticPinchGesture(const SyntheticPinchGesture&) = delete;
  SyntheticPinchGesture& operator=(const SyntheticPinchGesture&) = delete;

  ~SyntheticPinchGesture() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;
  void WaitForTargetAck(base::OnceClosure callback,
                        SyntheticGestureTarget* target) const override;

 private:
  SyntheticPinchGestureParams params_;
  std::unique_ptr<SyntheticGesture> lazy_gesture_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PINCH_GESTURE_H_
