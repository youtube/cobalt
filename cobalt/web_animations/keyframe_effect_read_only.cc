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

#include "cobalt/web_animations/keyframe_effect_read_only.h"

#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/cssom/interpolate_property_value.h"
#include "cobalt/cssom/property_definitions.h"

namespace cobalt {
namespace web_animations {

KeyframeEffectReadOnly::KeyframeEffectReadOnly(
    const scoped_refptr<AnimationEffectTimingReadOnly>& timing,
    const scoped_refptr<Animatable>& target,
    const std::vector<scoped_refptr<Keyframe> >& frames)
    : AnimationEffectReadOnly(timing), target_(target), data_(frames) {}

KeyframeEffectReadOnly::KeyframeEffectReadOnly(
    const scoped_refptr<AnimationEffectTimingReadOnly>& timing,
    const scoped_refptr<Animatable>& target,
    const Data::KeyframeSequence& frames)
    : AnimationEffectReadOnly(timing), target_(target), data_(frames) {}

KeyframeEffectReadOnly::Data::Data(const KeyframeSequence& keyframes)
    : keyframes_(keyframes) {
  CheckKeyframesSorted();
  PopulatePropertiesAffected();
}

KeyframeEffectReadOnly::Data::Data(
    const std::vector<scoped_refptr<Keyframe> >& keyframes) {
  // For each Keyframe object, we must extract and store its associated
  // Keyframe::Data object into a separate but parallel vector that we
  // internally store as our list of keyframes.
  for (std::vector<scoped_refptr<Keyframe> >::const_iterator iter =
           keyframes.begin();
       iter != keyframes.end(); ++iter) {
    keyframes_.push_back((*iter)->data());
  }
  CheckKeyframesSorted();
  PopulatePropertiesAffected();
}

void KeyframeEffectReadOnly::Data::CheckKeyframesSorted() const {
  base::optional<double> last_offset;
  for (KeyframeSequence::const_iterator iter = keyframes_.begin();
       iter != keyframes_.end(); ++iter) {
    DCHECK(iter->offset())
        << "We currently do not support automatic spacing of keyframes.";
    if (last_offset) {
      DCHECK_GE(*iter->offset(), *last_offset);
    }
    last_offset = iter->offset();
  }
}

void KeyframeEffectReadOnly::Data::PopulatePropertiesAffected() {
  // Compute a set of all properties affected by this effect by iterating
  // through our list of keyframes and querying them for the properties that
  // they affect.
  for (KeyframeSequence::const_iterator iter = keyframes_.begin();
       iter != keyframes_.end(); ++iter) {
    const Keyframe::Data& keyframe = *iter;
    for (Keyframe::Data::PropertyValueMap::const_iterator prop_iter =
             keyframe.property_values().begin();
         prop_iter != keyframe.property_values().end(); ++prop_iter) {
      properties_affected_.insert(prop_iter->first);
    }
  }
}

bool KeyframeEffectReadOnly::Data::IsPropertyAnimated(
    cssom::PropertyKey property) const {
  return properties_affected_.find(property) != properties_affected_.end();
}

bool KeyframeEffectReadOnly::Data::IsOnlyPropertyAnimated(
    cssom::PropertyKey property) const {
  if (properties_affected_.size() != 1) {
    return false;
  }

  return *properties_affected_.begin() == property;
}

void KeyframeEffectReadOnly::Data::ApplyAnimation(
    const scoped_refptr<cssom::CSSComputedStyleData>& in_out_style,
    double iteration_progress, double current_iteration) const {
  for (std::set<cssom::PropertyKey>::const_iterator iter =
           properties_affected_.begin();
       iter != properties_affected_.end(); ++iter) {
    if (GetPropertyAnimatable(*iter)) {
      in_out_style->SetPropertyValue(
          *iter, ComputeAnimatedPropertyValue(
                     *iter, in_out_style->GetPropertyValue(*iter),
                     iteration_progress, current_iteration));
    } else {
      NOTIMPLEMENTED() << GetPropertyName(*iter) << " is not animatable.";
    }
  }
}

namespace {
struct PropertySpecificKeyframe {
  static PropertySpecificKeyframe DefaultBeginFrame(
      const scoped_refptr<cssom::PropertyValue>& underlying_value) {
    return PropertySpecificKeyframe(0.0, cssom::TimingFunction::GetLinear(),
                                    underlying_value);
  }
  static PropertySpecificKeyframe DefaultEndFrame(
      const scoped_refptr<cssom::PropertyValue>& underlying_value) {
    return PropertySpecificKeyframe(1.0, cssom::TimingFunction::GetLinear(),
                                    underlying_value);
  }
  static PropertySpecificKeyframe FromKeyframe(
      const Keyframe::Data& keyframe, cssom::PropertyKey target_property) {
    Keyframe::Data::PropertyValueMap::const_iterator found =
        keyframe.property_values().find(target_property);
    DCHECK(found != keyframe.property_values().end());

    return PropertySpecificKeyframe(*keyframe.offset(), keyframe.easing(),
                                    found->second);
  }

  PropertySpecificKeyframe(double offset,
                           const scoped_refptr<cssom::TimingFunction>& easing,
                           const scoped_refptr<cssom::PropertyValue>& value)
      : offset(offset), easing(easing), value(value) {}

  double offset;
  scoped_refptr<cssom::TimingFunction> easing;
  scoped_refptr<cssom::PropertyValue> value;
};

int NumberOfKeyframesWithOffsetOfZero(
    const KeyframeEffectReadOnly::Data::KeyframeSequence& keyframes,
    cssom::PropertyKey target_property) {
  int number_of_keyframes_with_offset_of_zero = 0;

  // Since the keyframes are sorted we simply iterate through them in sequence
  // until we find one with an offset greater than zero.
  for (KeyframeEffectReadOnly::Data::KeyframeSequence::const_iterator iter =
           keyframes.begin();
       iter != keyframes.end(); ++iter) {
    if (iter->AffectsProperty(target_property)) {
      DCHECK(iter->offset());
      if (*iter->offset() == 0.0) {
        ++number_of_keyframes_with_offset_of_zero;
      } else {
        break;
      }
    }
  }

  return number_of_keyframes_with_offset_of_zero;
}

int NumberOfKeyframesWithOffsetOfOne(
    const KeyframeEffectReadOnly::Data::KeyframeSequence& keyframes,
    cssom::PropertyKey target_property) {
  int number_of_keyframes_with_offset_of_one = 0;

  // Since the keyframes are sorted we simply iterate through them in reverse
  // order until we find one with an offset less than one.
  for (KeyframeEffectReadOnly::Data::KeyframeSequence::const_reverse_iterator
           iter = keyframes.rbegin();
       iter != keyframes.rend(); ++iter) {
    if (iter->AffectsProperty(target_property)) {
      DCHECK(iter->offset());
      if (*iter->offset() == 1.0) {
        ++number_of_keyframes_with_offset_of_one;
      } else {
        break;
      }
    }
  }

  return number_of_keyframes_with_offset_of_one;
}

template <typename T>
T FirstWithProperty(const T& start, const T& end,
                    cssom::PropertyKey target_property) {
  T iter = start;
  for (; iter != end; ++iter) {
    if (iter->AffectsProperty(target_property)) {
      return iter;
    }
  }
  return iter;
}

}  // namespace

// Described within step 10 from:
//   https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-effect-value-of-a-keyframe-animation-effect
std::pair<base::optional<PropertySpecificKeyframe>,
          base::optional<PropertySpecificKeyframe> >
ComputeIntervalEndpoints(
    const KeyframeEffectReadOnly::Data::KeyframeSequence& keyframes,
    cssom::PropertyKey target_property,
    const scoped_refptr<cssom::PropertyValue>& underlying_value,
    double iteration_progress) {
  // We create a default being/end frame only if we find that we need them.
  std::pair<base::optional<PropertySpecificKeyframe>,
            base::optional<PropertySpecificKeyframe> > interval_endpoints;

  if (iteration_progress < 0.0 &&
      NumberOfKeyframesWithOffsetOfZero(keyframes, target_property) > 1) {
    interval_endpoints.first = PropertySpecificKeyframe::FromKeyframe(
        *FirstWithProperty(keyframes.begin(), keyframes.end(), target_property),
        target_property);
  } else if (iteration_progress >= 1.0 &&
             NumberOfKeyframesWithOffsetOfOne(keyframes, target_property) > 1) {
    interval_endpoints.first = PropertySpecificKeyframe::FromKeyframe(
        *FirstWithProperty(keyframes.rbegin(), keyframes.rend(),
                           target_property),
        target_property);
  } else {
    // Find the keyframe immediately preceeding the iteration_progress and set
    // that to the first endpoint, and set the next keyframe as the second
    // endpoint.
    KeyframeEffectReadOnly::Data::KeyframeSequence::const_iterator prev_iter =
        keyframes.end();
    KeyframeEffectReadOnly::Data::KeyframeSequence::const_iterator iter =
        keyframes.begin();
    for (; iter != keyframes.end(); ++iter) {
      if (iter->AffectsProperty(target_property)) {
        if (*iter->offset() > iteration_progress || *iter->offset() == 1.0) {
          break;
        }
        prev_iter = iter;
      }
    }

    if (prev_iter == keyframes.end() && *iter->offset() == 0.0) {
      DCHECK_LT(iteration_progress, 0.0);
      // In the case that iteration progress is negative, the first keyframe
      // should be set to the last (the only one, if it exists) keyframe with
      // an offset of 0.
      prev_iter = iter;
      iter = FirstWithProperty(iter + 1, keyframes.end(), target_property);
    }

    interval_endpoints.first =
        prev_iter == keyframes.end()
            ? PropertySpecificKeyframe::DefaultBeginFrame(underlying_value)
            : PropertySpecificKeyframe::FromKeyframe(*prev_iter,
                                                     target_property);
    interval_endpoints.second =
        iter == keyframes.end()
            ? PropertySpecificKeyframe::DefaultEndFrame(underlying_value)
            : PropertySpecificKeyframe::FromKeyframe(*iter, target_property);
  }

  return interval_endpoints;
}

// https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#the-effect-value-of-a-keyframe-animation-effect
scoped_refptr<cssom::PropertyValue>
KeyframeEffectReadOnly::Data::ComputeAnimatedPropertyValue(
    cssom::PropertyKey target_property,
    const scoped_refptr<cssom::PropertyValue>& underlying_value,
    double iteration_progress, double current_iteration) const {
  // Since not all steps are implemented here, this parameter is not yet
  // referenced in our implementation.
  UNREFERENCED_PARAMETER(current_iteration);

  // 6. If property-specific keyframes is empty, return underlying value.
  if (!IsPropertyAnimated(target_property)) {
    return underlying_value;
  }

  // 10. (see URL above for description).
  std::pair<base::optional<PropertySpecificKeyframe>,
            base::optional<PropertySpecificKeyframe> > interval_endpoints =
      ComputeIntervalEndpoints(keyframes_, target_property, underlying_value,
                               iteration_progress);

  // 12. If there is only one keyframe in interval endpoints return the property
  //     value of target property on that keyframe.
  if (!interval_endpoints.second) {
    return interval_endpoints.first->value;
  }

  // 13. Let start offset be the computed keyframe offset of the first keyframe
  //     in interval endpoints.
  double start_offset = interval_endpoints.first->offset;

  // 14. Let end offset be the computed keyframe offset of last keyframe in
  //     interval endpoints.
  double end_offset = interval_endpoints.second->offset;

  // 15. Let interval distance be the result of evaluating
  //     (iteration progress - start offset) / (end offset - start offset)
  double interval_distance =
      (iteration_progress - start_offset) / (end_offset - start_offset);

  // NOT IN SPEC. This seems missing from the specification, but this is the
  //              place where per-keyframe timing functions should be applied.
  float scaled_interval_distance =
      interval_endpoints.first->easing != cssom::TimingFunction::GetLinear()
          ? interval_endpoints.first->easing->Evaluate(
                static_cast<float>(interval_distance))
          : static_cast<float>(interval_distance);

  // 16. Return the result of applying the interpolation procedure defined by
  //     the animation behavior of the target property, to the values of the
  //     target property specified on the two keyframes in interval endpoints
  //     taking the first such value as V_start and the second as V_end and
  //     using interval distance as the interpolation parameter p.
  return InterpolatePropertyValue(scaled_interval_distance,
                                  interval_endpoints.first->value,
                                  interval_endpoints.second->value);
}

void KeyframeEffectReadOnly::TraceMembers(script::Tracer* tracer) {
  AnimationEffectReadOnly::TraceMembers(tracer);

  tracer->Trace(target_);
}

}  // namespace web_animations
}  // namespace cobalt
