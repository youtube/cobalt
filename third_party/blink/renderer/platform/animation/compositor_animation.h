// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/worklet_animation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace gfx {
class AnimationCurve;
}

namespace blink {

class CompositorAnimationDelegate;

// A compositor representation for Animation.
class PLATFORM_EXPORT CompositorAnimation : public cc::AnimationDelegate {
 public:
  static std::unique_ptr<CompositorAnimation> Create();
  static std::unique_ptr<CompositorAnimation> CreateWorkletAnimation(
      cc::WorkletAnimationId,
      const String& name,
      double playback_rate,
      std::unique_ptr<cc::AnimationOptions>,
      std::unique_ptr<cc::AnimationEffectTimings> effect_timings);

  explicit CompositorAnimation(scoped_refptr<cc::Animation>);
  CompositorAnimation(const CompositorAnimation&) = delete;
  CompositorAnimation& operator=(const CompositorAnimation&) = delete;
  ~CompositorAnimation() override;

  cc::Animation* CcAnimation() const;

  // An animation delegate is notified when animations are started and stopped.
  // The CompositorAnimation does not take ownership of the delegate, and
  // it is the responsibility of the client to reset the layer's delegate before
  // deleting the delegate.
  void SetAnimationDelegate(CompositorAnimationDelegate*);

  void AttachElement(const CompositorElementId&);
  void AttachPaintWorkletElement();
  void DetachElement();
  bool IsElementAttached() const;

  void AddKeyframeModel(std::unique_ptr<cc::KeyframeModel>);
  void RemoveKeyframeModel(int keyframe_model_id);
  void PauseKeyframeModel(int keyframe_model_id, base::TimeDelta time_offset);
  void AbortKeyframeModel(int keyframe_model_id);

  void UpdatePlaybackRate(double playback_rate);

 private:
  // cc::AnimationDelegate implementation.
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override;
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override;
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override;
  void NotifyAnimationTakeover(base::TimeTicks monotonic_time,
                               int target_property,
                               base::TimeTicks animation_start_time,
                               std::unique_ptr<gfx::AnimationCurve>) override;
  void NotifyLocalTimeUpdated(
      absl::optional<base::TimeDelta> local_time) override;

  scoped_refptr<cc::Animation> animation_;
  CompositorAnimationDelegate* delegate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_H_
