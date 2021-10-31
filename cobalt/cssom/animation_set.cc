// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/animation_set.h"

#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function_list_value.h"

namespace cobalt {
namespace cssom {

AnimationSet::AnimationSet(EventHandler* event_handler)
    : event_handler_(event_handler) {}

namespace {
base::TimeDelta GetTimeValue(size_t index, PropertyValue* list_value) {
  return base::polymorphic_downcast<TimeListValue*>(list_value)
      ->get_item_modulo_size(static_cast<int>(index));
}

Animation::FillMode GetFillMode(size_t index, PropertyValue* list_value) {
  PropertyValue* value =
      base::polymorphic_downcast<PropertyListValue*>(list_value)
          ->get_item_modulo_size(static_cast<int>(index))
          .get();

  if (value == KeywordValue::GetNone()) {
    return Animation::kNone;
  } else if (value == KeywordValue::GetForwards()) {
    return Animation::kForwards;
  } else if (value == KeywordValue::GetBackwards()) {
    return Animation::kBackwards;
  } else if (value == KeywordValue::GetBoth()) {
    return Animation::kBoth;
  } else {
    NOTREACHED();
    return Animation::kNone;
  }
}

Animation::PlaybackDirection GetDirection(size_t index,
                                          PropertyValue* list_value) {
  PropertyValue* value =
      base::polymorphic_downcast<PropertyListValue*>(list_value)
          ->get_item_modulo_size(static_cast<int>(index))
          .get();

  if (value == KeywordValue::GetNormal()) {
    return Animation::kNormal;
  } else if (value == KeywordValue::GetReverse()) {
    return Animation::kReverse;
  } else if (value == KeywordValue::GetAlternate()) {
    return Animation::kAlternate;
  } else if (value == KeywordValue::GetAlternateReverse()) {
    return Animation::kAlternateReverse;
  } else {
    NOTREACHED();
    return Animation::kNormal;
  }
}

float GetIterationCount(size_t index, PropertyValue* list_value) {
  PropertyValue* value =
      base::polymorphic_downcast<PropertyListValue*>(list_value)
          ->get_item_modulo_size(static_cast<int>(index))
          .get();

  if (value == KeywordValue::GetInfinite()) {
    return std::numeric_limits<float>::infinity();
  } else {
    return base::polymorphic_downcast<NumberValue*>(value)->value();
  }
}

scoped_refptr<TimingFunction> GetTimingFunction(size_t index,
                                                PropertyValue* list_value) {
  return base::polymorphic_downcast<TimingFunctionListValue*>(list_value)
      ->get_item_modulo_size(static_cast<int>(index));
}
}  // namespace

bool AnimationSet::Update(const base::TimeDelta& current_time,
                          const CSSComputedStyleData& style,
                          const CSSKeyframesRule::NameMap& keyframes_map) {
  const std::vector<scoped_refptr<PropertyValue> >& names =
      base::polymorphic_downcast<PropertyListValue*>(
          style.animation_name().get())
          ->value();

  // There should always be at least one element in the list (e.g. the
  // 'none' keyword).
  DCHECK_LE(static_cast<size_t>(1), names.size());

  if (animations_.empty() && names.size() == 1 &&
      names[0] == KeywordValue::GetNone()) {
    // If we have no current animations playing and no animations were
    // specified, there is nothing to do so we can return immediately.
    return false;
  }

  // Whether or not the animations have been modified by this update.
  bool animations_modified = false;

  // Build a set of all animations in the new declared animation set, so that
  // we can later use this to decide which old animations are no longer active.
  std::set<std::string> declared_animation_set;

  for (size_t i = 0; i < names.size(); ++i) {
    // If 'none' is specified as the animation name, this animation list element
    // is a no-op, skip it.
    if (names[i] == KeywordValue::GetNone()) {
      continue;
    }

    // Take note of the appearance of this string in 'animation-name' so that
    // we can check later which 'animation-names' used to be in the list but
    // no longer are.
    const std::string& name_string =
        base::polymorphic_downcast<StringValue*>(names[i].get())->value();
    declared_animation_set.insert(name_string);

    if (animations_.find(name_string) != animations_.end()) {
      // If the animation is already playing, we shouldn't interfere with it.
      continue;
    }

    // A new animation not previously started is being introduced, start by
    // looking up its keyframes rule (which may not exist).
    scoped_refptr<CSSKeyframesRule> keyframes;
    CSSKeyframesRule::NameMap::const_iterator found =
        keyframes_map.find(name_string);
    if (found != keyframes_map.end()) {
      keyframes = found->second;
    } else {
      LOG(WARNING) << "animation-name referenced undefined @keyframes rule, '"
                   << name_string << "'";
      continue;
    }

    // Create the animation and insert it into our map of currently active
    // animations.
    InternalAnimationMap::iterator inserted = animations_.insert(
        std::make_pair(
            name_string,
            AnimationEntry(Animation(name_string, keyframes, current_time,
                GetTimeValue(i, style.animation_delay().get()),
                GetTimeValue(i, style.animation_duration().get()),
                GetFillMode(i, style.animation_fill_mode().get()),
                GetIterationCount(
                    i, style.animation_iteration_count().get()),
                GetDirection(i, style.animation_direction().get()),
                GetTimingFunction(
                    i, style.animation_timing_function().get())))))
        .first;
    if (event_handler_) {
      event_handler_->OnAnimationStarted(inserted->second.animation, this);
    }

    animations_modified = true;
  }

  // Finally check for any animations that are now ended.
  std::vector<std::string> animations_to_end;
  for (InternalAnimationMap::iterator iter = animations_.begin();
       iter != animations_.end(); ++iter) {
    // If the animation is playing, but the current time is past the end time,
    // then we should signal to the event handler that it has ended.
    bool animation_has_ended =
        current_time >= iter->second.animation.start_time() +
                        iter->second.animation.duration();
    if (animation_has_ended && !iter->second.ended) {
      iter->second.ended = true;
      if (event_handler_) {
        event_handler_->OnAnimationEnded(iter->second.animation);
      }
    }

    // If the animation used to be playing, but it no longer appears in the
    // list of declared animations, then it has ended and we should mark it
    // as such.
    bool animation_is_removed = declared_animation_set.find(iter->first) ==
                                declared_animation_set.end();
    if (animation_is_removed) {
      if (event_handler_) {
        event_handler_->OnAnimationRemoved(iter->second.animation);
      }
      animations_to_end.push_back(iter->first);
    }
  }

  if (!animations_to_end.empty()) {
    for (std::vector<std::string>::iterator iter = animations_to_end.begin();
         iter != animations_to_end.end(); ++iter) {
      animations_.erase(*iter);
    }

    animations_modified = true;
  }

  return animations_modified;
}

void AnimationSet::Clear() {
  for (auto& iter : animations_) {
    if (event_handler_) {
      event_handler_->OnAnimationRemoved(iter.second.animation);
    }
  }
  animations_.clear();
}

}  // namespace cssom
}  // namespace cobalt
