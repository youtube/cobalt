// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_

#include "content/browser/renderer_host/input/synthetic_smooth_move_gesture.h"
#include "content/common/content_export.h"

#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"

namespace content {
class CONTENT_EXPORT SyntheticSmoothScrollGesture : public SyntheticGesture {
 public:
  explicit SyntheticSmoothScrollGesture(
      const SyntheticSmoothScrollGestureParams& params);
  ~SyntheticSmoothScrollGesture() override;

  // SyntheticGesture implementation:
  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;
  void WaitForTargetAck(base::OnceClosure callback,
                        SyntheticGestureTarget* target) const override;

 private:
  static SyntheticSmoothMoveGestureParams::InputType GetInputSourceType(
      content::mojom::GestureSourceType gesture_source_type);

  bool InitializeMoveGesture(content::mojom::GestureSourceType gesture_type,
                             SyntheticGestureTarget* target);

  std::unique_ptr<SyntheticSmoothMoveGesture> move_gesture_;
  SyntheticSmoothScrollGestureParams params_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_SMOOTH_SCROLL_GESTURE_H_
