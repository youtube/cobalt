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

#include "cobalt/cssom/css_transition_set.h"

#include <algorithm>
#include <cmath>

#include "base/lazy_instance.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_key_list_value.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function_list_value.h"

namespace cobalt {
namespace cssom {

namespace {
// Returns the index at which the property should be animated.  If the property
// should not be animated, -1 is returned.
int GetPropertyTransitionIndex(
    PropertyKey property,
    const scoped_refptr<const PropertyValue>& transition_property_value) {
  if (transition_property_value == KeywordValue::GetNone()) {
    return -1;
  } else {
    const PropertyKeyListValue* property_name_list =
        base::polymorphic_downcast<const PropertyKeyListValue*>(
            transition_property_value.get());

    // The LAST reference to the given property in the transition-property list
    // is the one we use to determine the property's index, so start searching
    // for it from the back to the front.
    //   https://www.w3.org/TR/2013/WD-css3-transitions-20131119/#transition-property-property
    for (int i = static_cast<int>(property_name_list->value().size()) - 1;
         i >= 0; --i) {
      PropertyKey current_list_property =
          property_name_list->value()[static_cast<size_t>(i)];
      if (current_list_property == kAllProperty ||
          current_list_property == property) {
        return i;
      }
    }
  }

  return -1;
}
}  // namespace

TransitionSet::TransitionSet(EventHandler* event_handler)
    : transitions_(event_handler) {}

void TransitionSet::UpdateTransitions(
    const base::TimeDelta& current_time,
    const CSSComputedStyleData& source_computed_style,
    const CSSComputedStyleData& destination_computed_style) {
  const TimeListValue* transition_duration =
      base::polymorphic_downcast<const TimeListValue*>(
          destination_computed_style.transition_duration().get());
  if (transition_duration->value().size() == 1 &&
      transition_duration->value()[0] == base::TimeDelta() &&
      transitions_.IsEmpty()) {
    // No need to process transitions if all their durations are set to
    // 0, the initial value, so this will happen often.
    return;
  }

  // For each animatable property, check to see if there are any transitions
  // assigned to it.  If so, check to see if there are any existing transitions
  // that must be updated, otherwise introduce new transitions.
  const PropertyKeyVector& animatable_properties = GetAnimatableProperties();
  for (PropertyKeyVector::const_iterator iter = animatable_properties.begin();
       iter != animatable_properties.end(); ++iter) {
    UpdateTransitionForProperty(
        *iter, current_time, source_computed_style.GetPropertyValue(*iter),
        destination_computed_style.GetPropertyValue(*iter),
        destination_computed_style);
  }
}

namespace {
void SetReversingValues(
    const base::TimeDelta& current_time, const Transition& old_transition,
    const scoped_refptr<PropertyValue>& new_start_value,
    const scoped_refptr<PropertyValue>& new_end_value,
    scoped_refptr<PropertyValue>* new_reversing_adjusted_start_value,
    float* new_reversing_shortening_factor) {
  // This value is calculated as explained here:
  //   https://www.w3.org/TR/css3-transitions/#reversing
  // These calculations make a pleasant experience when reversing a transition
  // half-way through by making the reverse transition occur over half as much
  // time.
  if (old_transition.reversing_adjusted_start_value()->Equals(*new_end_value)) {
    *new_reversing_shortening_factor = std::min<float>(
        1.0f,
        std::max<float>(
            0.0f, std::abs(old_transition.Progress(current_time) *
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
    PropertyKey property, const base::TimeDelta& current_time,
    const Transition& old_transition, const base::TimeDelta& duration,
    const base::TimeDelta& delay,
    const scoped_refptr<TimingFunction>& timing_function,
    const scoped_refptr<PropertyValue>& end_value) {
  // Since we're updating an old transition, we'll need to know the animated
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

  base::TimeDelta with_reversing_delay =
      delay < base::TimeDelta()
          ? ScaleTimeDelta(delay, new_reversing_shortening_factor)
          : delay;

  return Transition(
      property, current_value_within_old_transition, end_value, current_time,
      ScaleTimeDelta(duration, new_reversing_shortening_factor),
      with_reversing_delay, timing_function, new_reversing_adjusted_start_value,
      new_reversing_shortening_factor);
}

}  // namespace

void TransitionSet::UpdateTransitionForProperty(
    PropertyKey property, const base::TimeDelta& current_time,
    const scoped_refptr<PropertyValue>& start_value,
    const scoped_refptr<PropertyValue>& end_value,
    const CSSComputedStyleData& transition_style) {
  // This method essentially implements the logic defined at
  //   https://www.w3.org/TR/css3-transitions/#starting

  // Get the index of this property in the transition-property list, so we
  // can know if the property should be animated (i.e. it is animated if it
  // is in the list) and also how to match up transition-duration and other
  // transition attributes to it.
  int transition_index = GetPropertyTransitionIndex(
      property, transition_style.transition_property());

  // The property is only animated if its transition duration is greater than 0.
  const TimeListValue* transition_duration =
      base::polymorphic_downcast<const TimeListValue*>(
          transition_style.transition_duration().get());

  // Get animation parameters by using the transition_index modulo the
  // specific property sizes.  This is inline with the specification which
  // states that list property values should be repeated to calculate values
  // for indices larger than the list's size.
  base::TimeDelta duration =
      transition_index < 0
          ? base::TimeDelta()
          : transition_duration->get_item_modulo_size(transition_index);

  if (!duration.is_zero()) {
    if (!start_value->Equals(*end_value)) {
      TimeListValue* delay_list = base::polymorphic_downcast<TimeListValue*>(
          transition_style.transition_delay().get());
      const base::TimeDelta& delay =
          delay_list->get_item_modulo_size(transition_index);

      TimingFunctionListValue* timing_function_list =
          base::polymorphic_downcast<TimingFunctionListValue*>(
              transition_style.transition_timing_function().get());
      const scoped_refptr<TimingFunction>& timing_function =
          timing_function_list->get_item_modulo_size(transition_index);

      // The property has been modified and the transition should be animated.
      // We now check if an active transition for this property already exists
      // or not.
      const Transition* existing_transition =
          transitions_.GetTransitionForProperty(property);

      if (existing_transition &&
          current_time < existing_transition->EndTime()) {
        // A transition is already occurring, so we handle this case a bit
        // differently depending on if we're reversing the previous transition
        // or starting a completely different one.
        transitions_.UpdateTransitionForProperty(
            property,
            CreateTransitionOverOldTransition(
                property, current_time, *existing_transition, duration, delay,
                timing_function, end_value),
            this);
      } else {
        // There is no transition on the object currently, so create a new
        // one for it.
        transitions_.UpdateTransitionForProperty(
            property,
            Transition(property, start_value, end_value, current_time, duration,
                       delay, timing_function, start_value, 1.0f),
            this);
      }
    } else {
      // Check if there is an existing transition for this property and see
      // if it has completed yet.  If so, remove it from the list of
      // transformations.
      const Transition* transition =
          transitions_.GetTransitionForProperty(property);
      if (transition != NULL) {
        if (current_time >= transition->EndTime()) {
          transitions_.RemoveTransitionForPropertyIfExists(
              property, Transition::kIsNotCanceled);
        }
      }
    }
  } else {
    // The property is not in the transition-property list or has zero
    // duration, thus it should no longer be animated. Remove the transition if
    // it exists. It does not generate a transitionend event, it does not pass
    // go, it does not collect $200.
    transitions_.RemoveTransitionForPropertyIfExists(property,
                                                     Transition::kIsCanceled);
  }
}

const Transition* TransitionSet::GetTransitionForProperty(
    PropertyKey property) const {
  return transitions_.GetTransitionForProperty(property);
}

void TransitionSet::Clear() { transitions_.Clear(); }

TransitionSet::TransitionMap::TransitionMap(EventHandler* event_handler)
    : event_handler_(event_handler) {}

TransitionSet::TransitionMap::~TransitionMap() {
  if (event_handler_ != NULL) {
    for (InternalTransitionMap::iterator iter = transitions_.begin();
         iter != transitions_.end(); ++iter) {
      event_handler_->OnTransitionRemoved(iter->second,
                                          Transition::kIsCanceled);
    }
  }
}

const Transition* TransitionSet::TransitionMap::GetTransitionForProperty(
    PropertyKey property) const {
  InternalTransitionMap::const_iterator found = transitions_.find(property);
  if (found == transitions_.end()) return NULL;

  return &found->second;
}

void TransitionSet::TransitionMap::UpdateTransitionForProperty(
    PropertyKey property, const Transition& transition,
    cssom::TransitionSet* transition_set) {
  InternalTransitionMap::iterator found = transitions_.find(property);
  if (found != transitions_.end()) {
    if (event_handler_ != NULL) {
      event_handler_->OnTransitionRemoved(found->second,
                                          Transition::kIsCanceled);
    }
    found->second = transition;
  } else {
    transitions_.insert(std::make_pair(property, transition));
  }

  if (event_handler_ != NULL) {
    event_handler_->OnTransitionStarted(transition, transition_set);
  }
}

void TransitionSet::TransitionMap::RemoveTransitionForPropertyIfExists(
    PropertyKey property, Transition::IsCanceled is_canceled) {
  InternalTransitionMap::iterator found = transitions_.find(property);
  if (found == transitions_.end()) {
    return;
  }

  if (event_handler_ != NULL) {
    event_handler_->OnTransitionRemoved(found->second, is_canceled);
  }

  transitions_.erase(found);
}

void TransitionSet::TransitionMap::Clear() {
  if (event_handler_ != NULL) {
    for (auto& transition : transitions_) {
      event_handler_->OnTransitionRemoved(transition.second,
                                          Transition::kIsCanceled);
    }
  }

  transitions_.clear();
}

namespace {
base::LazyInstance<TransitionSet>::DestructorAtExit g_empty_transition_set =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

const TransitionSet* TransitionSet::EmptyTransitionSet() {
  return &g_empty_transition_set.Get();
}

}  // namespace cssom
}  // namespace cobalt
