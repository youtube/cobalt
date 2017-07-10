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

#include "cobalt/accessibility/internal/text_alternative_helper.h"

#include <utility>

#include "base/string_split.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_image_element.h"
#include "cobalt/dom/node_list.h"

namespace cobalt {
namespace accessibility {
namespace internal {
namespace {
// Helper class that inserts a base::Token into a base::hash_set during the
// class's lifetime.
class ScopedIdSetMember {
 public:
  // Adds |token| into |hash_set| for the duration of this class instance's
  // lifetime. If |token| is an empty string, it doesn't get added to the set.
  //
  // TextAlternativeHelper::visited_element_ids_ is used to keep track of a set
  // of element ids that have been visited during the current computation to
  // ensure that we do not follow any ID reference loops. When
  // AppendTextAlternative is called, the ID of that element is pushed into the
  // set using this helper class, so that the ID will be popped back out of the
  // set which allows us to re-visit that element in a non-recursive manner, if
  // necessary.
  ScopedIdSetMember(base::hash_set<base::Token>* hash_set,
                    const base::Token& token)
      : hash_set_(hash_set) {
    // Ignore empty tokens.
    if (SbStringGetLength(token.c_str()) > 0) {
      DCHECK(hash_set_->find(token) == hash_set_->end());
      iterator_pair_ = hash_set_->insert(token);
      DCHECK(iterator_pair_.second);
    } else {
      // Ensure iterator_pair_ is initialized.
      iterator_pair_ = std::make_pair(hash_set_->end(), false);
    }
  }
  ~ScopedIdSetMember() {
    if (iterator_pair_.second) {
      hash_set_->erase(iterator_pair_.first);
    }
  }

 private:
  base::hash_set<base::Token>* hash_set_;
  std::pair<base::hash_set<base::Token>::iterator, bool> iterator_pair_;
};

// Look up an element whose id is |id| in the document that |element| is in.
scoped_refptr<dom::Element> GetElementById(
    const scoped_refptr<dom::Element>& element, const std::string& id) {
  // TODO: If referenced elements tend to be close to the referencing element,
  // it may be more efficient to do a breadth-first search starting from
  // |element|.
  dom::Document* document = element->node_document();
  scoped_refptr<dom::Element> target_element = document->GetElementById(id);
  DLOG_IF(WARNING, !target_element) << "Could not find aria-labelledby target: "
                                    << id;
  return target_element;
}
}  // namespace

void TextAlternativeHelper::AppendTextAlternative(
    const scoped_refptr<dom::Node>& node) {
  DCHECK(node);
  // https://www.w3.org/TR/2014/REC-wai-aria-implementation-20140320/#mapping_additional_nd_te
  // 5.6.1.3. Text Alternative Computation

  if (node->IsText()) {
    // Rule 3D
    // Get the contents of the text node.
    if (node->text_content()) {
      AppendTextIfNonEmpty(*node->text_content());
    }
    return;
  }

  scoped_refptr<dom::Element> element = node->AsElement();
  if (!element) {
    // Since this is neither a Text or Element, ignore it.
    return;
  }

  if (visited_element_ids_.find(element->id()) != visited_element_ids_.end()) {
    DLOG(INFO) << "Skipping element to prevent reference loop: "
               << element->id();
    return;
  }

  ScopedIdSetMember scoped_id_set_member(&visited_element_ids_, element->id());

  // Rule 1
  // Skip hidden elements unless the author specifies to use them via an
  // aria-labelledby or aria-describedby being used in the current computation.
  if (!in_labelled_by_or_described_by_ && IsAriaHidden(element)) {
    return;
  }

  // Rule 2A (first bullet point)
  // The aria-labelledby attribute takes precedence as the element's text
  // alternative unless this computation is already occurring as the result of a
  // recursive aria-labelledby declaration.
  if (!in_labelled_by_or_described_by_) {
    if (TryAppendFromLabelledByOrDescribedBy(element,
                                             base::Tokens::aria_labelledby())) {
      return;
    }
  }

  // Rule 2A (second bullet point)
  // If aria-labelledby is empty or undefined, the aria-label attribute, which
  // defines an explicit text string, is used. However, if this computation is
  // already occurring as the result of a recursive text alternative computation
  // and the current element is an embedded control as defined in rule 2B,
  // ignore the aria-label attribute and skip directly to rule 2B.
  //
  // Note: Cobalt doesn't support the embedded controls defined in rule 2B, so
  // just get the alternative text from the label, if it exists.
  if (TryAppendFromLabel(element)) {
    return;
  }

  // Rule 2A (third bullet point)
  // If aria-labelledby and aria-label are both empty or undefined, and if the
  // element is not marked as presentational (role="presentation", check for the
  // presence of an equivalent host language attribute or element for
  // associating a label, and use those mechanisms to determine a text
  // alternative. For example, in HTML, the img element's alt attribute defines
  // a label string and the label element references the form element it labels.
  //
  // This implementation does not support the "role" attribute, so just get the
  // "alt" property, if this is an element that supports it
  if (TryAppendFromAltProperty(element)) {
    return;
  }

  // Rule 2B
  // Cobalt does not support the controls described in this step, so skip it.

  // Rule 2C
  // Otherwise, if the attributes checked in rules A and B didn't provide
  // results, text is collected from descendant content if the current element's
  // role allows "Name From: contents." The text alternatives for child nodes
  // will be concatenated, using this same set of rules. This same rule may
  // apply to a child, which means the computation becomes recursive and can
  // result in text being collected in all the nodes in this subtree, no matter
  // how deep they are. However, any given descendant subtree may instead
  // collect their part of the text alternative from the preferred markup
  // described in A and B above. These author-specified attributes are assumed
  // to provide the correct text alternative for the entire subtree. All in all,
  // the node rules are applied consistently as text alternatives are collected
  // from descendants, and each containing element in those descendants may or
  // may not allow their contents to be used. Each node in the subtree is
  // consulted only once. If text has been collected from a child node, and is
  // referenced by another IDREF in some descendant node, then that second, or
  // subsequent, reference is not followed. This is done to avoid infinite
  // loops.
  //
  // The "role" attribute is not supported, so ignore it. Recurse through
  // children and concatenate text alternatives.
  scoped_refptr<dom::NodeList> children = element->child_nodes();
  for (int i = 0; i < children->length(); ++i) {
    scoped_refptr<dom::Node> child = children->Item(i);
    AppendTextAlternative(child);
  }

  // 5.6.1.2. Description Compution
  // If aria-describedby is present, user agents MUST compute the accessible
  // description by concatenating the text alternatives for nodes referenced by
  // an aria-describedby attribute on the current node. The text alternatives
  // for the referenced nodes are computed using a number of methods.
  if (!in_labelled_by_or_described_by_) {
    TryAppendFromLabelledByOrDescribedBy(element,
                                         base::Tokens::aria_describedby());
  }
}

std::string TextAlternativeHelper::GetTextAlternative() {
  return JoinString(alternatives_, ' ');
}

bool TextAlternativeHelper::IsAriaHidden(
    const scoped_refptr<dom::Element>& element) {
  if (!element) {
    return false;
  }
  base::optional<std::string> aria_hidden_attribute =
      element->GetAttribute(base::Tokens::aria_hidden().c_str());
  if (aria_hidden_attribute.value_or("") == base::Tokens::true_token()) {
    return true;
  }
  return IsAriaHidden(element->parent_element());
}

bool TextAlternativeHelper::TryAppendFromLabelledByOrDescribedBy(
    const scoped_refptr<dom::Element>& element, const base::Token& token) {
  DCHECK(!in_labelled_by_or_described_by_);
  base::optional<std::string> attributes = element->GetAttribute(token.c_str());
  std::vector<std::string> ids;
  // If aria-labelledby is empty or undefined, the aria-label attribute ... is
  // used (defined below).
  base::SplitStringAlongWhitespace(attributes.value_or(""), &ids);
  const size_t current_num_alternatives = alternatives_.size();
  if (!ids.empty()) {
    in_labelled_by_or_described_by_ = true;
    for (int i = 0; i < ids.size(); ++i) {
      if (visited_element_ids_.find(base::Token(ids[i])) !=
          visited_element_ids_.end()) {
        DLOG(WARNING) << "Skipping reference to ID: " << ids[i]
                      << " to prevent reference loop.";
        continue;
      }
      scoped_refptr<dom::Element> target_element =
          GetElementById(element, ids[i]);
      if (!target_element) {
        DLOG(WARNING) << "Could not find " << token.c_str()
                      << " target: " << ids[i];
        continue;
      }
      AppendTextAlternative(target_element);
    }
    in_labelled_by_or_described_by_ = false;
  }
  // Check if any of these recursive calls to AppendTextAlternative actually
  // ended up appending something.
  return current_num_alternatives != alternatives_.size();
}

bool TextAlternativeHelper::TryAppendFromLabel(
    const scoped_refptr<dom::Element>& element) {
  base::optional<std::string> label_attribute =
      element->GetAttribute(base::Tokens::aria_label().c_str());
  return AppendTextIfNonEmpty(label_attribute.value_or(""));
}

bool TextAlternativeHelper::TryAppendFromAltProperty(
    const scoped_refptr<dom::Element>& element) {
  // https://www.w3.org/TR/REC-html40/struct/objects.html#h-13.8
  // The only element type that supports the "alt" attribute that Cobalt
  // implements is the <img> element.
  if (element->AsHTMLElement() &&
      element->AsHTMLElement()->AsHTMLImageElement()) {
    base::optional<std::string> alt_attribute =
        element->GetAttribute(base::Tokens::alt().c_str());
    if (alt_attribute) {
      return AppendTextIfNonEmpty(*alt_attribute);
    }
  }
  return false;
}

bool TextAlternativeHelper::AppendTextIfNonEmpty(const std::string& text) {
  std::string trimmed;
  TrimWhitespaceASCII(text, TRIM_ALL, &trimmed);
  if (!trimmed.empty()) {
    alternatives_.push_back(trimmed);
  }
  return !trimmed.empty();
}

}  // namespace internal
}  // namespace accessibility
}  // namespace cobalt
