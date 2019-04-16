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

#include "cobalt/accessibility/internal/live_region.h"

#include <bitset>
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/element.h"

namespace cobalt {
namespace accessibility {
namespace internal {
namespace {
typedef std::bitset<8> RelevantMutationsBitset;

// Default value for the aria-relevant property per the spec.
// https://www.w3.org/TR/wai-aria/states_and_properties#aria-relevant
const char kDefaultAriaRelevantValue[] = "additions text";

bool HasValidLiveRegionProperty(const scoped_refptr<dom::Element>& element) {
  std::string aria_live_attribute =
      element->GetAttribute(base::Tokens::aria_live().c_str()).value_or("");

  return aria_live_attribute == base::Tokens::assertive() ||
         aria_live_attribute == base::Tokens::polite();
}

RelevantMutationsBitset GetRelevantMutations(
    const scoped_refptr<dom::Element>& element) {
  RelevantMutationsBitset bitset;

  std::string aria_relevant_attribute =
      element->GetAttribute(base::Tokens::aria_relevant().c_str())
          .value_or(kDefaultAriaRelevantValue);
  std::vector<std::string> tokens =
      base::SplitString(aria_relevant_attribute, base::kWhitespaceASCII,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i] == base::Tokens::additions()) {
      bitset.set(LiveRegion::kMutationTypeAddition);
    } else if (tokens[i] == base::Tokens::removals()) {
      bitset.set(LiveRegion::kMutationTypeRemoval);
    } else if (tokens[i] == base::Tokens::text()) {
      bitset.set(LiveRegion::kMutationTypeText);
    } else if (tokens[i] == base::Tokens::all()) {
      bitset.set();
    } else {
      DLOG(WARNING) << "Unexpected value for aria-relevant attribute: "
                    << tokens[i];
    }
  }
  return bitset;
}
}  // namespace

// static
std::unique_ptr<LiveRegion> LiveRegion::GetLiveRegionForNode(
    const scoped_refptr<dom::Node>& node) {
  if (!node) {
    return std::unique_ptr<LiveRegion>(nullptr);
  }
  scoped_refptr<dom::Element> element = node->AsElement();
  if (element && HasValidLiveRegionProperty(element)) {
    return base::WrapUnique(new LiveRegion(element));
  }
  return GetLiveRegionForNode(node->parent_node());
}

bool LiveRegion::IsAssertive() const {
  base::Optional<std::string> aria_live_attribute =
      root_->GetAttribute(base::Tokens::aria_live().c_str());
  if (!aria_live_attribute) {
    NOTREACHED();
    return false;
  }
  return *aria_live_attribute == base::Tokens::assertive();
}

bool LiveRegion::IsMutationRelevant(MutationType mutation_type) const {
  RelevantMutationsBitset bitset = GetRelevantMutations(root_);
  return bitset.test(mutation_type);
}

bool LiveRegion::IsAtomic(const scoped_refptr<dom::Node>& node) const {
  // Stop searching if we go past the live region's root node. The default is
  // non-atomic.
  if (!node || node == root_->parent_node()) {
    return false;
  }
  if (node->IsElement()) {
    scoped_refptr<dom::Element> element = node->AsElement();
    // Search ancestors of the changed element to determine if this change is
    // atomic or not, per the algorithm described in the spec.
    // https://www.w3.org/TR/wai-aria/states_and_properties#aria-atomic
    base::Optional<std::string> aria_atomic_attribute =
        element->GetAttribute(base::Tokens::aria_atomic().c_str());
    if (aria_atomic_attribute) {
      if (*aria_atomic_attribute == base::Tokens::true_token()) {
        return true;
      } else if (*aria_atomic_attribute == base::Tokens::false_token()) {
        return false;
      } else {
        DLOG(WARNING) << "Unexpected token for aria-atomic: "
                      << *aria_atomic_attribute;
      }
    }
  }
  return IsAtomic(node->parent_node());
}

}  // namespace internal
}  // namespace accessibility
}  // namespace cobalt
