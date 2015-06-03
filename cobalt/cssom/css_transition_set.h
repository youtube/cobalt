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

#ifndef CSSOM_CSS_TRANSITION_SET_H_
#define CSSOM_CSS_TRANSITION_SET_H_

#include "base/hash_tables.h"
#include "base/optional.h"
#include "base/time.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/css_transition.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// Maintains a mapping from the set of all unique CSS style properties to
// active transitions.  Not all style properties may have active transitions.
// This class is used to maintain persistance of active transitions and is
// expected to be modified over time as events such as CSS style modifications
// occur.
class TransitionSet {
 public:
  TransitionSet();

  // Helper function to get a const reference to a global empty transition set.
  static const TransitionSet* EmptyTransitionSet();

  // Given a source and destination CSS Style Declarations (presumably applied
  // to an HTML element), updates all current transitions to reflect the new
  // state.  This may mean removing transitions that were canceled or
  // completed, updating existing transitions with new values, or creating new
  // transitions.  The implementation of this method is defined here:
  //   http://www.w3.org/TR/2013/WD-css3-transitions-20131119/#starting
  void UpdateTransitions(const base::Time& current_time,
                         const cssom::CSSStyleDeclarationData& source,
                         const cssom::CSSStyleDeclarationData& destination);

  // Given the name of a property, returns the active transition corresponding
  // to it.  If no transition currently exists for this property, this method
  // returns NULL.
  const Transition* GetTransitionForProperty(const char* property) const;

  // Returns true if there are no animations in this animation set.
  bool empty() const { return transitions_.empty(); }

  // Given a time and a target style, animates all properties found to be
  // transitioning by this TransitionSet.  Non-animated properties are not
  // adjusted on the target style.
  void ApplyTransitions(const base::Time& current_time,
                        cssom::CSSStyleDeclarationData* target_style) const;

 private:
  void UpdateTransitionForProperty(
      const char* property_name, const base::Time& current_time,
      const scoped_refptr<PropertyValue>& source_value,
      const scoped_refptr<PropertyValue>& destination_value,
      const cssom::CSSStyleDeclarationData& transition_style);

  // Inserts or replaces an existing value in the internal map.  Essentially
  // this method behaves like InternalTransitionMap::operator[], but does not
  // require that the map value have a default constructor defined.
  void InsertOrReplaceInInternalMap(
      const char* property_name, const Transition& transition);

  // The value of our internal hash map is a base::optional type so that we
  // do not need to specify a default constructor on it.
  typedef base::hash_map<const char*, Transition> InternalTransitionMap;
  InternalTransitionMap transitions_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_TRANSITION_SET_H_
