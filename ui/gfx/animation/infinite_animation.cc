// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/infinite_animation.h"

#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {

InfiniteAnimation::InfiniteAnimation(AnimationDelegate* delegate,
                                     base::TimeDelta timer_interval)
    : Animation(timer_interval) {
  set_delegate(delegate);
}

InfiniteAnimation::~InfiniteAnimation() = default;

double InfiniteAnimation::GetCurrentValue() const {
  return 0.0;
}

void InfiniteAnimation::Step(base::TimeTicks time_now) {
  if (delegate()) {
    delegate()->AnimationProgressed(this);
  }
}

}  // namespace gfx
