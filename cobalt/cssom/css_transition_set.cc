/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/cssom/css_transition_set.h"

#include <cmath>

#include "base/lazy_instance.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_name_list_value.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/time_list_value.h"

namespace cobalt {
namespace cssom {

namespace {
// Returns the index at which the property should be animated.  If the property
// should not be animated, -1 is returned.
int GetPropertyTransitionIndex(
    const char* property,
    const scoped_refptr<PropertyValue>& transition_property_value) {
  if (transition_property_value == KeywordValue::GetNone()) {
    return -1;
  } else {
    PropertyNameListValue* property_name_list =
        base::polymorphic_downcast<PropertyNameListValue*>(
            transition_property_value.get());

    // The LAST reference to the given property in the transition-property list
    // is the one we use to determine the property's index, so start searching
    // for it from the back to the front.
    //   http://www.w3.org/TR/2013/WD-css3-transitions-20131119/#transition-property-property
    for (int i = static_cast<int>(property_name_list->value().size()) - 1;
         i >= 0; --i) {
      const char* current_list_property =
          property_name_list->value()[static_cast<size_t>(i)];
      if (current_list_property == kAllPropertyName ||
          current_list_property == property) {
        return i;
      }
    }
  }

  return -1;
}
}  // namespace

TransitionSet::TransitionSet() {}

void TransitionSet::UpdateTransitions(
    const base::Time& current_time,
    const cssom::CSSStyleDeclarationData& source_computed_style,
    const cssom::CSSStyleDeclarationData& destination_computed_style) {
  TimeListValue* transition_duration =
      base::polymorphic_downcast<TimeListValue*>(
          destination_computed_style.transition_duration().get());
  if (transition_duration->value().size() == 1 &&
      transition_duration->value()[0].value == 0.0f) {
    // No need to process transitions if all their durations are set to
    // 0, the initial value, so this will happen often.
    return;
  }

  // For each animatable property, check to see if there are any transitions
  // assigned to it.  If so, check to see if there are any existing transitions
  // that must be updated, otherwise introduce new transitions.
  UpdateTransitionForProperty(kBackgroundColorPropertyName, current_time,
                              source_computed_style.background_color(),
                              destination_computed_style.background_color(),
                              destination_computed_style);
  UpdateTransitionForProperty(
      kTransformPropertyName, current_time, source_computed_style.transform(),
      destination_computed_style.transform(), destination_computed_style);
}

void TransitionSet::InsertOrReplaceInInternalMap(
    const char* property_name, const Transition& transition) {
  InternalTransitionMap::iterator found = transitions_.find(property_name);
  if (found == transitions_.end()) {
    transitions_.insert(std::make_pair(property_name, transition));
  } else {
    found->second = transition;
  }
}

namespace {
void SetReversingValues(
    const base::Time& current_time, const Transition& old_transition,
    const scoped_refptr<PropertyValue>& new_start_value,
    const scoped_refptr<PropertyValue>& new_end_value,
    scoped_refptr<PropertyValue>* new_reversing_adjusted_start_value,
    float* new_reversing_shortening_factor) {
  // This value is calculated as explained here:
  //   http://www.w3.org/TR/css3-transitions/#reversing
  // These calculations make a pleasant experience when reversing a transition
  // half-way through by making the reverse transition occur over half as much
  // time.
  if (old_transition.reversing_adjusted_start_value()->Equals(*new_end_value)) {
    *new_reversing_shortening_factor =
        std::min<float>(1.0f, std::max<float>(0.0f,
            std::abs(old_transition.Progress(current_time) *
                         old_transition.reversing_shortening_factor() +
                     1 - old_transition.reversing_shortening_factor())));

    *new_reversing_adjusted_start_value = old_transition.end_value();
  } else {
    *new_reversing_adjusted_start_value = new_start_value;
    *new_reversing_shortening_factor = 1.0f;
  }
}

base::TimeDelta ScaleTimeDelta(const base::TimeDelta& time_delta, float scale) {
  return base::TimeDelta::FromMicroseconds(static_cast<int64>(
      time_delta.InSecondsF() * scale * base::Time::kMicrosecondsPerSecond));
}

// Given that a new transition has started on a CSS value that had an active old
// transition occurring on it, this function creates a new transition based on
// the old transition's values (so that we can smoothly transition out of the
// middle of an old transition).
Transition CreateTransitionOverOldTransition(
    const char* property_name, const base::Time& current_time,
    const Transition& old_transition, const base::TimeDelta& duration,
    const scoped_refptr<PropertyValue>& end_value) {
  // Since we're updating an old transtion, we'll need to know the animated
  // CSS style value from the old transition at this point in time.
  scoped_refptr<PropertyValue> current_value_within_old_transition =
      old_transition.Evaluate(current_time);

  // If the new transition is a reversal fo the old transition, we need to
  // setup reversing_adjusted_start_value and reversing_shortening_factor so
  // that they can be used to reduce the new transition's duration.
  scoped_refptr<PropertyValue> new_reversing_adjusted_start_value;
  float new_reversing_shortening_factor;
  SetReversingValues(current_time, old_transition,
                     current_value_within_old_transition, end_value,
                     &new_reversing_adjusted_start_value,
                     &new_reversing_shortening_factor);

  // TODO(***REMOVED***): Implement transition-delay property.
  base::TimeDelta delay = base::TimeDelta();
  if (delay < base::TimeDelta()) {
    delay = ScaleTimeDelta(delay, new_reversing_shortening_factor);
  }

  return Transition(property_name, current_value_within_old_transition,
                    end_value, current_time,
                    ScaleTimeDelta(duration, new_reversing_shortening_factor),
                    delay, TimingFunction(), new_reversing_adjusted_start_value,
                    new_reversing_shortening_factor);
}

}  // namespace

void TransitionSet::UpdateTransitionForProperty(
    const char* property_name, const base::Time& current_time,
    const scoped_refptr<PropertyValue>& start_value,
    const scoped_refptr<PropertyValue>& end_value,
    const cssom::CSSStyleDeclarationData& transition_style) {
  // This method essentially implements the logic defined at
  //   http://www.w3.org/TR/css3-transitions/#starting

  // Get the index of this property in the transition-property list, so we
  // can know if the property should be animated (i.e. it is animated if it
  // is in the list) and also how to match up transition-duration and other
  // transition attributes to it.
  int transition_index = GetPropertyTransitionIndex(
      property_name, transition_style.transition_property());

  if (transition_index != -1) {
    // The property should be animated, though we check now to see if the
    // corresponding transition-duration is set to 0 or not, since 0 implies
    // that it would not be animated.
    TimeListValue* transition_duration =
        base::polymorphic_downcast<TimeListValue*>(
            transition_style.transition_duration().get());
    base::TimeDelta duration =
        transition_duration->time_at_index(transition_index).ToTimeDelta();

    if (duration.InMilliseconds() != 0 && !start_value->Equals(*end_value)) {
      // The property has been modified and the transition should be animated.
      // We now check if an active transition for this property already exists
      // or not.
      InternalTransitionMap::iterator found = transitions_.find(property_name);
      if (found != transitions_.end() &&
          current_time < found->second.EndTime()) {
        // A transition is already ocurring, so we handle this case a bit
        // differently depending on if we're reversing the previous transition
        // or starting a completely different one.
        found->second = CreateTransitionOverOldTransition(
            property_name, current_time, found->second, duration, end_value);
      } else {
        // There is no transition on the object currently, so create a new
        // one for it.
        InsertOrReplaceInInternalMap(property_name,
            Transition(
                property_name, start_value, end_value, current_time, duration,
                base::TimeDelta(), TimingFunction(), start_value, 1.0f));
      }
    } else {
      // Check if there is an existing transition for this property and see
      // if it has completed yet.  If so, remove it from the list of
      // transformations.
      // TODO(***REMOVED***): Fire off a transitionend event.
      //   http://www.w3.org/TR/css3-transitions/#transitionend
      const Transition* transition = GetTransitionForProperty(property_name);
      if (transition != NULL) {
        if (current_time >= transition->EndTime()) {
          transitions_.erase(property_name);
        }
      }
    }
  } else {
    // The property is not in the transition-property list, thus it should no
    // longer be animated.  Remove the transition if it exists.  It does
    // not generate a transitionend event, it does not pass go, it does not
    // collect $200.
    transitions_.erase(property_name);
  }
}

const Transition* TransitionSet::GetTransitionForProperty(
    const char* property) const {
  InternalTransitionMap::const_iterator found = transitions_.find(property);
  if (found == transitions_.end()) return NULL;

  return &found->second;
}

void TransitionSet::ApplyTransitions(
    const base::Time& current_time,
    cssom::CSSStyleDeclarationData* target_style) const {
  // For each animatable property, check if it's transitioning, and if so,
  // evaluate its new animated value given the current time and update the
  // target_style.
  const Transition* background_color_transition =
      GetTransitionForProperty(kBackgroundColorPropertyName);
  if (background_color_transition) {
    target_style->set_background_color(
        background_color_transition->Evaluate(current_time));
  }

  const Transition* transform_transition =
      GetTransitionForProperty(kTransformPropertyName);
  if (transform_transition) {
    target_style->set_transform(transform_transition->Evaluate(current_time));
  }
}

namespace {
base::LazyInstance<TransitionSet> g_empty_transition_set =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

const TransitionSet& TransitionSet::EmptyTransitionSet() {
  return g_empty_transition_set.Get();
}

}  // namespace cssom
}  // namespace cobalt
