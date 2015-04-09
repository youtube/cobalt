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
  // For each animatable property, check to see if there are any transitions
  // assigned to it.  If so, check to see if there are any existing transitions
  // that must be updated, otherwise introduce new transitions.
  UpdateTransitionForProperty(kBackgroundColorPropertyName, current_time,
                              source_computed_style.background_color(),
                              destination_computed_style.background_color(),
                              destination_computed_style);
}

void TransitionSet::UpdateTransitionForProperty(
    const char* property_name, const base::Time& current_time,
    const scoped_refptr<PropertyValue>& source_value,
    const scoped_refptr<PropertyValue>& destination_value,
    const cssom::CSSStyleDeclarationData& transition_style) {
  TimeListValue* transition_duration =
      base::polymorphic_downcast<TimeListValue*>(
          transition_style.transition_duration().get());

  // This is not implemented according to the specifications yet, this will
  // be done later.  Here, we check if the new CSS value is different from
  // the original one.  If it is, we start a transition from the old one to
  // the new one.
  if (!source_value->IsEqual(destination_value)) {
    int transition_index = GetPropertyTransitionIndex(
        property_name, transition_style.transition_property());
    // Is this property marked to transition?
    if (transition_index != -1) {
      base::TimeDelta duration =
          transition_duration->time_at_index(transition_index).ToTimeDelta();

      // Only transition if the duration is not set to 0.
      if (duration.InMilliseconds() != 0) {
        transitions_[property_name] = Transition(
            property_name, source_value, destination_value, current_time,
            duration, base::TimeDelta(), TimingFunction(), source_value, 1.0f);
      }
    }
  }
}

const Transition* TransitionSet::GetTransitionForProperty(
    const char* property) const {
  InternalTransitionMap::const_iterator found = transitions_.find(property);
  if (found == transitions_.end()) return NULL;

  return &found->second.value();
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
