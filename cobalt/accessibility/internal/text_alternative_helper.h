// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_ACCESSIBILITY_INTERNAL_TEXT_ALTERNATIVE_HELPER_H_
#define COBALT_ACCESSIBILITY_INTERNAL_TEXT_ALTERNATIVE_HELPER_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "cobalt/base/token.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/node.h"

namespace cobalt {
namespace accessibility {
namespace internal {

// Helper class for implementing the WAI-ARIA Text Alternative computation
// algorithm, made public for testing. This class should not be used directly.
class TextAlternativeHelper {
 public:
  TextAlternativeHelper() : in_labelled_by_or_described_by_(false) {}
  void AppendTextAlternative(const scoped_refptr<dom::Node>& node);

  // Return the accumulated alternatives joined by a single space character.
  std::string GetTextAlternative();

  // Append the text alternative from a aria-labelledby or aria-describedby
  // property. Returns true if text alternative(s) were successfully appended.
  bool TryAppendFromLabelledByOrDescribedBy(
      const scoped_refptr<dom::Element>& element, const base::Token& token);

  // Append the text alternative from a aria-label property. Returns true
  // if the aria-label property exists and has a non-empty value.
  bool TryAppendFromLabel(const scoped_refptr<dom::Element>& element);

  // Append the text alternative from an element's "alt" property, for element
  // types that support the semantics of the alt property. The only such element
  // in Cobalt is <img> elements.
  // Returns true if |element| is an element that supports the semantics of the
  // "alt" property, and has a non-empty value for the "alt" property.
  bool TryAppendFromAltProperty(const scoped_refptr<dom::Element>& element);

  // Append |text| to the accumulation of text alternative strings if it is not
  // empty. Returns true if text is non-empty.
  bool AppendTextIfNonEmpty(const std::string& text);

  // Returns true iff this element or one of its ancestors has the "aria-hidden"
  // attribute set to "true".
  static bool IsAriaHidden(const scoped_refptr<dom::Element>& element);

 private:
  typedef base::hash_set<base::Token> TokenSet;

  bool in_labelled_by_or_described_by_;
  std::vector<std::string> alternatives_;
  TokenSet visited_element_ids_;
};

}  // namespace internal
}  // namespace accessibility
}  // namespace cobalt

#endif  // COBALT_ACCESSIBILITY_INTERNAL_TEXT_ALTERNATIVE_HELPER_H_
