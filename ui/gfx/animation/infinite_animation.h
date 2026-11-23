// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_INFINITE_ANIMATION_H_
#define UI_GFX_ANIMATION_INFINITE_ANIMATION_H_

#include "base/time/time.h"
#include "ui/gfx/animation/animation.h"

namespace gfx {

class AnimationDelegate;

// An animation that runs infinitely at a set interval until Stop() is called.
// Unlike LinearAnimation, it has no end state and does not stop automatically.
class ANIMATION_EXPORT InfiniteAnimation : public Animation {
 public:
  explicit InfiniteAnimation(
      AnimationDelegate* delegate,
      base::TimeDelta timer_interval = base::Milliseconds(16));

  InfiniteAnimation(const InfiniteAnimation&) = delete;
  InfiniteAnimation& operator=(const InfiniteAnimation&) = delete;

  ~InfiniteAnimation() override;

  // Animation overrides:
  double GetCurrentValue() const override;

 protected:
  // Animation overrides:
  void Step(base::TimeTicks time_now) override;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_INFINITE_ANIMATION_H_
