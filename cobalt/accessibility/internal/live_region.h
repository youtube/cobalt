// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_ACCESSIBILITY_INTERNAL_LIVE_REGION_H_
#define COBALT_ACCESSIBILITY_INTERNAL_LIVE_REGION_H_

#include <memory>
#include <string>

#include "cobalt/dom/node.h"

namespace cobalt {
namespace accessibility {
namespace internal {
// Collection of functions to support the implementation of wai-aria Live
// Regions:
// https://www.w3.org/TR/2010/WD-wai-aria-20100916/terms#def_liveregion
class LiveRegion {
 public:
  // Types of mutation to the DOM subtree that might cause a live region to be
  // updated, corresponding to the possible values of aria-relevant as
  // described in:
  // https://www.w3.org/TR/2011/CR-wai-aria-20110118/states_and_properties#aria-relevant
  enum MutationType {
    kMutationTypeAddition = 1,  // corresponds to "additions"
    kMutationTypeRemoval,       // corresponds to "removals"
    kMutationTypeText,          // corresponds to "text"
  };

  // Returns a LiveRegion instance describing the live region that this element
  // is a member of, or NULL if this element is not part of a live region.
  //
  // Searches the element's ancestors looking for the first element with a
  // valid aria-live attribute.
  static std::unique_ptr<LiveRegion> GetLiveRegionForNode(
      const scoped_refptr<dom::Node>& node);

  // Returns true if the value of aria-live is "assertive".
  // https://www.w3.org/TR/2011/CR-wai-aria-20110118/states_and_properties#aria-live
  bool IsAssertive() const;

  // Returns true if this live region should be updated in response to the
  // specified type of change, based on the value of this element's
  // aria-relevant property.
  // https://www.w3.org/TR/2011/CR-wai-aria-20110118/states_and_properties#aria-relevant
  bool IsMutationRelevant(MutationType mutation_type) const;

  // Returns true if changes to this node should result in an atomic update
  // to its live region, according to the value of the aria-atomic property set
  // on this node or its ancestors, up to and including the live region root.
  //
  // https://www.w3.org/TR/2011/CR-wai-aria-20110118/states_and_properties#aria-atomic
  bool IsAtomic(const scoped_refptr<dom::Node>& node) const;

  // Returns the root of the live region (i.e.) the element that has the
  // aria-live property set on it.
  const scoped_refptr<dom::Element>& root() { return root_; }

 private:
  explicit LiveRegion(const scoped_refptr<dom::Element>& root) : root_(root) {}

  // The root element of the live region. That is, the element with a valid
  // aria-live attribute.
  scoped_refptr<dom::Element> root_;
};

}  // namespace internal
}  // namespace accessibility
}  // namespace cobalt

#endif  // COBALT_ACCESSIBILITY_INTERNAL_LIVE_REGION_H_
