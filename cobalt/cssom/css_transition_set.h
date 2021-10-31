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

#ifndef COBALT_CSSOM_CSS_TRANSITION_SET_H_
#define COBALT_CSSOM_CSS_TRANSITION_SET_H_

#include <map>

#include "base/basictypes.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "cobalt/cssom/css_transition.h"
#include "cobalt/cssom/property_definitions.h"

namespace cobalt {
namespace cssom {

class CSSComputedStyleData;
class PropertyValue;

// Maintains a mapping from the set of all unique CSS style properties to
// active transitions.  Not all style properties may have active transitions.
// This class is used to maintain persistence of active transitions and is
// expected to be modified over time as events such as CSS style modifications
// occur.
class TransitionSet {
 public:
  // An EventHandler object can be supplied to the TransitionSet upon
  // construction to have TransitionSet update a caller as it internally updates
  // its set of active transitions on each call to UpdateTransitions().  This
  // is useful as it can provide an external observer (e.g. such as an adapter
  // from CSS Transitions to the Web Animations API) the opportunity to take
  // action based on transition start/end events.
  class EventHandler {
   public:
    virtual ~EventHandler() {}
    // Called when a transition is starting, either freshly introduced or
    // replacing an existing transition.
    virtual void OnTransitionStarted(const Transition& transition,
                                     TransitionSet* transition_set) = 0;

    // Called when a transition is removed, either because it has finished, or
    // because it was replaced (in which case OnTransitionStarted() will be
    // called with the replacement immediately after this).
    virtual void OnTransitionRemoved(const Transition& transition,
                                     Transition::IsCanceled is_canceled) = 0;
  };

  explicit TransitionSet(EventHandler* event_handler = NULL);

  // Helper function to get a const reference to a global empty transition set.
  static const TransitionSet* EmptyTransitionSet();

  // Given a source and destination CSS Style Declarations (presumably applied
  // to an HTML element), updates all current transitions to reflect the new
  // state.  This may mean removing transitions that were canceled or
  // completed, updating existing transitions with new values, or creating new
  // transitions.  The implementation of this method is defined here:
  //   https://www.w3.org/TR/2013/WD-css3-transitions-20131119/#starting
  void UpdateTransitions(const base::TimeDelta& current_time,
                         const CSSComputedStyleData& source,
                         const CSSComputedStyleData& destination);

  // Given the name of a property, returns the active transition corresponding
  // to it.  If no transition currently exists for this property, this method
  // returns NULL.
  const Transition* GetTransitionForProperty(PropertyKey property) const;

  // Returns true if there are no animations in this animation set.
  bool empty() const { return transitions_.IsEmpty(); }

  // Clears out all currently running transitions.
  void Clear();

 private:
  void UpdateTransitionForProperty(
      PropertyKey property, const base::TimeDelta& current_time,
      const scoped_refptr<PropertyValue>& source_value,
      const scoped_refptr<PropertyValue>& destination_value,
      const CSSComputedStyleData& transition_style);

  // We define a TransitionMap (from CSS property to Transition object) here to
  // host our set of active transitions, and additionally manage the firing
  // of transition begin/end events.
  class TransitionMap {
   public:
    explicit TransitionMap(EventHandler* event_handler);
    ~TransitionMap();

    const Transition* GetTransitionForProperty(PropertyKey property) const;
    bool IsEmpty() const { return transitions_.empty(); }

    // Adds or replaces an existing transition in the map.
    void UpdateTransitionForProperty(PropertyKey property,
                                     const Transition& transition,
                                     cssom::TransitionSet* transition_set);

    // Removes an existing transition from the map if it exists.
    void RemoveTransitionForPropertyIfExists(
        PropertyKey property, Transition::IsCanceled is_canceled);

    // Clears all entries from the map, as if RemoveTransitionForProperty()
    // had been called on each property.
    void Clear();

    typedef std::map<PropertyKey, Transition> InternalTransitionMap;
    const InternalTransitionMap& GetInternalMap() const { return transitions_; }

   private:
    EventHandler* event_handler_;
    InternalTransitionMap transitions_;
  };

  TransitionMap transitions_;

  DISALLOW_COPY_AND_ASSIGN(TransitionSet);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_TRANSITION_SET_H_
