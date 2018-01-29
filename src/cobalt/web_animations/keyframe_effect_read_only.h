// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_WEB_ANIMATIONS_KEYFRAME_EFFECT_READ_ONLY_H_
#define COBALT_WEB_ANIMATIONS_KEYFRAME_EFFECT_READ_ONLY_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web_animations/animatable.h"
#include "cobalt/web_animations/animation_effect_read_only.h"
#include "cobalt/web_animations/keyframe.h"

namespace cobalt {
namespace web_animations {

// Keyframe effects are represented by the KeyframeEffectReadOnly interface.
//   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-keyframeeffect-interfaces
// This class contains a sequence of keyframes which define an effect.  Each
// keyframe specifies an offset and values for a set of CSS properties.  All
// keyframes are normalized to be between 0 and 1, and the responsibility for
// converting from an Animation's local time to a value between 0 and 1 lies
// within AnimationEffectTimingReadOnly, an object owned by
// AnimationEffectReadOnly, this class's superclass.
class KeyframeEffectReadOnly : public AnimationEffectReadOnly {
 public:
  class Data {
   public:
    explicit Data(const std::vector<Keyframe::Data>& keyframes);
    explicit Data(const std::vector<scoped_refptr<Keyframe> >& keyframes);

    typedef std::vector<Keyframe::Data> KeyframeSequence;
    const KeyframeSequence& keyframes() const { return keyframes_; }

    // Returns true if the given property is referenced by any of this effect's
    // keyframes.
    bool IsPropertyAnimated(cssom::PropertyKey property_name) const;

    // Returns true if the given property is the only property referenced by any
    // of this effect's keyframes.
    bool IsOnlyPropertyAnimated(cssom::PropertyKey property_name) const;

    // Applies the underlying effect to the given input CSS style, animating
    // all properties of the style that are referenced by this effect, and
    // updating the provided CSS style to reflect the output animated style,
    // given the current iteration progress and current_iteration.
    void ApplyAnimation(
        const scoped_refptr<cssom::CSSComputedStyleData>& in_out_style,
        double iteration_progress, double current_iteration) const;

    // Applies this effect to a single property value, accepting the underlying
    // value of that property value as input and returning an animated
    // property value as output.
    scoped_refptr<cssom::PropertyValue> ComputeAnimatedPropertyValue(
        cssom::PropertyKey target_property,
        const scoped_refptr<cssom::PropertyValue>& underlying_value,
        double iteration_progress, double current_iteration) const;

   private:
    void CheckKeyframesSorted() const;
    void PopulatePropertiesAffected();

    // A list of keyframes sorted by offset.
    KeyframeSequence keyframes_;

    // The set of properties that are affected by this animation.
    std::set<cssom::PropertyKey> properties_affected_;
  };

  KeyframeEffectReadOnly(
      const scoped_refptr<AnimationEffectTimingReadOnly>& timing,
      const scoped_refptr<Animatable>& target,
      const std::vector<scoped_refptr<Keyframe> >& frames);
  KeyframeEffectReadOnly(
      const scoped_refptr<AnimationEffectTimingReadOnly>& timing,
      const scoped_refptr<Animatable>& target,
      const std::vector<Keyframe::Data>& frames);

  scoped_refptr<Animatable> target() const { return target_; }

  // Custom, not in any spec.
  const Data& data() const { return data_; }

  DEFINE_WRAPPABLE_TYPE(KeyframeEffectReadOnly);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~KeyframeEffectReadOnly() override {}

  scoped_refptr<Animatable> target_;

  Data data_;

  DISALLOW_COPY_AND_ASSIGN(KeyframeEffectReadOnly);
};

}  // namespace web_animations
}  // namespace cobalt

#endif  // COBALT_WEB_ANIMATIONS_KEYFRAME_EFFECT_READ_ONLY_H_
