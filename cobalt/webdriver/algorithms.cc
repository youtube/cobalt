// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/webdriver/algorithms.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_br_element.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/node.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/math/rect.h"

namespace cobalt {
namespace webdriver {
namespace algorithms {
namespace {
// Characters that match \s in ECMAScript regular expressions.
// Note that non-breaking space is at the beginning to simplify definition of
// kWhitespaceCharsExcludingNonBreakingSpace below.
const char kWhitespaceChars[] =
    u8"\u00a0 "
    u8"\f\n\r\t\v\u1680\u180e\u2000\u2001\u2002\u2003\u2004\u2005\u2006"
    u8"\u2007\u2008\u2009\u200a\u2028\u2029\u202f\u205f\u3000\ufeff";
const char* kWhitespaceCharsExcludingNonBreakingSpace = kWhitespaceChars + 1;

// Defined in https://www.w3.org/TR/webdriver/#text.horizontal
const char kHorizontalWhitespaceChars[] = u8" \f\t\v\u2028\u2029";

// Defined in step 2.1 of the getElementText() algorithm in
// https://www.w3.org/TR/webdriver/#getelementtext
const char kZeroWidthSpacesAndFeeds[] = u8"\f\v\u200b\u200e\u200f";

const char kNonBreakingSpace = '\xa0';

bool IsHorizontalWhitespace(char c) {
  DCHECK_NE(c, '\0');
  return strchr(kHorizontalWhitespaceChars, c) != NULL;
}

bool IsZeroWidthSpaceOrFeed(char c) {
  DCHECK_NE(c, '\0');
  return strchr(kZeroWidthSpacesAndFeeds, c) != NULL;
}

bool IsInHeadElement(dom::Element* element) {
  if (element->AsHTMLElement() &&
      element->AsHTMLElement()->AsHTMLHeadElement()) {
    return true;
  }
  scoped_refptr<dom::Element> parent = element->parent_element();
  if (parent == NULL) {
    return false;
  }
  return IsInHeadElement(parent.get());
}

// Helper class that can be used as a predicate to std::remove_if.
// When more than one instance of the character |c| occurs consecutively, the
// functor will return true for each occurrence of |c| after the first one.
class MatchConsecutiveCharactersPredicate {
 public:
  explicit MatchConsecutiveCharactersPredicate(char c)
      : character_to_match_(c), last_('\0') {}
  bool operator()(char c) {
    DCHECK_NE(c, '\0');
    bool same_char = c == last_;
    last_ = c;
    return same_char && c == character_to_match_;
  }

 private:
  char character_to_match_;
  char last_;
};

void CanonicalizeText(const base::optional<std::string>& whitespace_style,
                      const base::optional<std::string>& text_transform,
                      std::string* text) {
  // std::remove_if will not resize the std::string, but will return a new end
  // of the string. Use this iterator instead of text->end() in each step
  // below, and erase the end of the string at the end.
  std::string::iterator end = text->end();

  // https://www.w3.org/TR/webdriver/#getelementtext
  // 2.1 Remove any zero-width spaces (\u200b, \u200e, \u200f), form feeds (\f)
  // or vertical tab feeds (\v) from text.
  end = std::remove_if(text->begin(), end, IsZeroWidthSpaceOrFeed);

  // Consecutive sequences of new lines should be compressed to a single new
  // line. Accomplish this by converting all \r chars to \n chars, and then
  // converting sequences of \n chars to a single \n char.
  std::replace(text->begin(), end, '\r', '\n');
  MatchConsecutiveCharactersPredicate consecutive_newline_predicate('\n');
  end = std::remove_if(text->begin(), end, consecutive_newline_predicate);

  // https://www.w3.org/TR/webdriver/#getelementtext
  // 2.3
  if (whitespace_style) {
    // If the parent's effective CSS whitespace style is 'normal' or 'nowrap'
    // replace each newline (\n) in text with a single space character (\x20).
    if (*whitespace_style == cssom::kNormalKeywordName ||
        *whitespace_style == cssom::kNoWrapKeywordName) {
      std::replace(text->begin(), end, '\n', ' ');
    }

    // If the parent's effective CSS whitespace style is 'pre' or 'pre-wrap'
    // replace each horizontal whitespace character with a non-breaking space
    // character (\xa0).
    // Otherwise replace each sequence of horizontal whitespace characters
    // except non-breaking spaces (\xa0) with a single space character.
    //
    // Cobalt does not have 'pre-wrap' style, so just check for 'pre'.
    if (*whitespace_style == cssom::kPreKeywordName) {
      std::replace_if(text->begin(), end, IsHorizontalWhitespace,
                      kNonBreakingSpace);
    } else {
      // Replace all horizontal whitespace characters with ' '.
      std::replace_if(text->begin(), end, IsHorizontalWhitespace, ' ');
      // Convert consecutive ' ' characters to a single ' '.
      MatchConsecutiveCharactersPredicate consecutive_space_predicate(' ');
      end = std::remove_if(text->begin(), end, consecutive_space_predicate);
    }
  }

  // Trim the original string, since several characters may have been removed.
  text->erase(end, text->end());

  // https://www.w3.org/TR/webdriver/#getelementtext
  // 2.4 Apply the parent's effective CSS text-transform style as per the
  // CSS 2.1 specification ([CSS21])
  if (text_transform) {
    // Cobalt does not support 'capitalize' and 'lowercase' keywords.
    if (*text_transform == cssom::kUppercaseKeywordName) {
      // Convert to UTF16 to do i18n safe upper-case conversion.
      string16 utf16_string;
      UTF8ToUTF16(text->c_str(), text->length(), &utf16_string);
      utf16_string = base::i18n::ToUpper(utf16_string.c_str());
      UTF16ToUTF8(utf16_string.c_str(), utf16_string.length(), text);
    }
  }
}

// Helper template function to get the computed style from a
// cssom::CSSComputedStyleData member function getter.
template <typename style_getter_function>
base::optional<std::string> GetComputedStyle(dom::Element* element,
                                             style_getter_function getter) {
  scoped_refptr<dom::HTMLElement> html_element(element->AsHTMLElement());
  DCHECK(html_element);
  if (html_element->computed_style()) {
    scoped_refptr<cssom::PropertyValue> property_value =
        (html_element->computed_style()->*getter)();
    if (property_value) {
      return property_value->ToString();
    }
  }
  return base::nullopt;
}

// https://www.w3.org/TR/webdriver/#text.blocklevel
bool IsBlockLevelElement(dom::Element* element) {
  base::optional<std::string> display_style =
      GetComputedStyle(element, &cssom::CSSComputedStyleData::display);
  if (display_style) {
    if (*display_style == cssom::kInlineKeywordName ||
        *display_style == cssom::kInlineBlockKeywordName ||
        *display_style == cssom::kNoneKeywordName) {
      // inline-table, table-cell, table-column, table-column-group are not
      // supported by Cobalt.
      return false;
    }
  }
  return true;
}

// Return true if there is at least one string in the vector and the last one
// is non-empty.
bool LastLineIsNonEmpty(const std::vector<std::string>& lines) {
  if (lines.empty()) {
    return false;
  }
  return !lines.back().empty();
}

// Recursive function to build the vector of lines for the text representation
// of an element.
void GetElementTextInternal(dom::Element* element,
                            std::vector<std::string>* lines) {
  // If the element is a: BR element: Push '' to lines and continue.
  if (element->AsHTMLElement() && element->AsHTMLElement()->AsHTMLBRElement()) {
    lines->push_back("");
    return;
  }

  bool is_block = IsBlockLevelElement(element);

  // If the element is a: Block-level element and if last(lines) is not '',
  // push '' to lines.
  if (is_block && LastLineIsNonEmpty(*lines)) {
    lines->push_back("");
  }

  // These styles are needed for the text nodes.
  base::optional<std::string> whitespace_style =
      GetComputedStyle(element, &cssom::CSSComputedStyleData::white_space);
  base::optional<std::string> text_transform_style =
      GetComputedStyle(element, &cssom::CSSComputedStyleData::text_transform);

  bool is_displayed = IsDisplayed(element);

  scoped_refptr<dom::NodeList> children = element->child_nodes();
  // https://www.w3.org/TR/webdriver/#getelementtext
  // 2. For each descendent of node, at time of execution, in order:
  for (uint32 i = 0; i < children->length(); ++i) {
    scoped_refptr<dom::Node> child = children->Item(i);
    if (child->IsText() && is_displayed) {
      // If descendent is a [DOM] text node let text equal the nodeValue
      // property of descendent.
      std::string text = child->node_value().value_or("");
      CanonicalizeText(whitespace_style, text_transform_style, &text);

      // 2.5 If last(lines) ends with a space character and text starts with a
      // space character, trim the first character of text.
      // 2.6 Append text to last(lines) in-place.
      if (lines->empty()) {
        lines->push_back(text);
      } else {
        if (!lines->back().empty() && *(lines->back().rbegin()) == ' ' &&
            !text.empty() && *(text.begin()) == ' ') {
          text.erase(0);
        }
        lines->back().append(text);
      }
    } else if (child->IsElement()) {
      scoped_refptr<dom::Element> child_element = child->AsElement();
      GetElementTextInternal(child_element.get(), lines);
    }
  }

  if (is_block && LastLineIsNonEmpty(*lines)) {
    lines->push_back("");
  }
}

bool DisplayStyleIsNone(dom::Element* element) {
  base::optional<std::string> display_style =
      GetComputedStyle(element, &cssom::CSSComputedStyleData::display);
  return display_style && *display_style == cssom::kNoneKeywordName;
}

// Return true if opacity is set to zero.
bool IsTransparent(dom::Element* element) {
  base::optional<std::string> opacity_style =
      GetComputedStyle(element, &cssom::CSSComputedStyleData::opacity);
  return opacity_style && *opacity_style == "0";
}

// Return true if this element and all its ancestors have display style set to
// none.
bool AreElementAndAncestorsDisplayed(dom::Element* element) {
  DCHECK(element);
  if (DisplayStyleIsNone(element)) {
    return false;
  }
  scoped_refptr<dom::Element> parent = element->parent_element();
  return !parent || AreElementAndAncestorsDisplayed(parent.get());
}

// Returns true if this element has non-zero dimensions, or if any of its
// children have non-zero dimensions and this element's overflow style is not
// set to hidden.
bool HasPositiveSizeDimensions(dom::Element* element) {
  DCHECK(element);
  scoped_refptr<dom::DOMRect> rect = element->GetBoundingClientRect();
  DCHECK(rect);
  if (rect->height() > 0 && rect->width() > 0) {
    return true;
  }
  // Zero-sized elements should still be considered to have positive size
  // if they have a child element or text node with positive size, unless
  // the element has an 'overflow' style of 'hidden'.
  base::optional<std::string> overflow_style =
      GetComputedStyle(element, &cssom::CSSComputedStyleData::overflow);
  if (overflow_style && *overflow_style == cssom::kHiddenKeywordName) {
    return false;
  }
  scoped_refptr<dom::NodeList> child_nodes = element->child_nodes();
  DCHECK(child_nodes);
  for (uint32 i = 0; i < child_nodes->length(); ++i) {
    scoped_refptr<dom::Node> child = child_nodes->Item(i);
    if (child->IsText() ||
        (child->IsElement() && HasPositiveSizeDimensions(child->AsElement()))) {
      return true;
    }
  }
  // Neither this element nor any of its children have positive size dimensions.
  return false;
}

math::Rect GetRect(dom::Element* element) {
  scoped_refptr<dom::DOMRect> element_rect = element->GetBoundingClientRect();
  math::RectF rect(element_rect->x(), element_rect->y(), element_rect->width(),
                   element_rect->height());
  return math::Rect::RoundFromRectF(rect);
}

// Returns true if this element is completely hidden by overflow.
bool IsHiddenByOverflow(dom::Element* element) {
  math::Rect element_rect = GetRect(element);
  // Check each ancestor of this element and if the ancestor's overflow style
  // is set to hidden, check if this element completely overflows or underflows
  // the element and is thus not visible.
  dom::Element* parent = element->parent_element();
  while (parent) {
    // Only block level elements will hide children due to overflow.
    if (IsBlockLevelElement(parent)) {
      // Cobalt doesn't support overflow-x or overflow-y, so just check for
      // overflow.
      base::optional<std::string> overflow_style =
          GetComputedStyle(parent, &cssom::CSSComputedStyleData::overflow);
      if (overflow_style && *overflow_style == cssom::kHiddenKeywordName) {
        // Get the parent's rect. If the element's rect does not intersect the
        // parent's rect, then it is hidden by overflow.
        math::Rect parent_rect = GetRect(parent);
        if (!element_rect.Intersects(parent_rect)) {
          return true;
        }
      }
    }
    parent = parent->parent_element();
  }
  return false;
}

// Returns true if this element and all its children are completely hidden
// due to overflow.
bool AreElementAndChildElementsHiddenByOverflow(dom::Element* element) {
  if (!IsHiddenByOverflow(element)) {
    return false;
  }
  scoped_refptr<dom::NodeList> child_nodes = element->child_nodes();
  DCHECK(child_nodes);
  // Check if all child elements are also hidden by overflow.
  for (uint32 i = 0; i < child_nodes->length(); ++i) {
    scoped_refptr<dom::Node> child = child_nodes->Item(i);
    if (child->IsElement()) {
      scoped_refptr<dom::Element> child_as_element = child->AsElement();
      if (AreElementAndChildElementsHiddenByOverflow(child_as_element.get()) ||
          !HasPositiveSizeDimensions(child_as_element.get())) {
        return false;
      }
    }
  }
  // All child elements are hidden by overflow.
  return true;
}

}  // namespace

std::string GetElementText(dom::Element* element) {
  DCHECK(element);

  // https://www.w3.org/TR/webdriver/#getelementtext
  // 1. If the element is in the head element of the document, return an empty
  // string.
  if (IsInHeadElement(element)) {
    return "";
  }

  // Update the computed styles first to ensure we get up-to-date computed
  // styles.
  DCHECK(element->node_document());
  element->node_document()->UpdateComputedStyles();

  // Recursively visit this element and its children to create a vector of lines
  // of text.
  std::vector<std::string> lines;
  GetElementTextInternal(element, &lines);

  // Trim leading and trailing non-breaking space characters in each line in
  // place.
  for (size_t i = 0; i < lines.size(); ++i) {
    TrimString(lines[0], kWhitespaceCharsExcludingNonBreakingSpace,
               &(lines[0]));
  }
  // Join the lines, and trim any leading/trailing newlines.
  std::string joined = JoinString(lines, '\n');
  TrimString(joined, "\n", &joined);
  // Convert non-breaking spaces to regular spaces.
  std::replace(joined.begin(), joined.end(), kNonBreakingSpace, ' ');
  return joined;
}

// There is a spec for "displayedness" available:
//   https://w3c.github.io/webdriver/webdriver-spec.html#element-displayedness
// However, the algorithm described in the spec does not match existing
// implementations of WebDriver.
// IsDisplayed will match the existing implementations, using the implementation
// in selenium's github repository as a reference.
// https://github.com/SeleniumHQ/selenium/blob/master/javascript/atoms/dom.js#L577
bool IsDisplayed(dom::Element* element) {
  DCHECK(element);

  // By convention, BODY element is always shown: BODY represents the document
  // and even if there's nothing rendered in there, user can always see there's
  // the document.
  if (element->AsHTMLElement() &&
      element->AsHTMLElement()->AsHTMLBodyElement()) {
    return true;
  }

  // Update the computed styles first to ensure we get up-to-date computed
  // styles.
  if (element->AsHTMLElement()) {
    DCHECK(element->node_document());
    // If the element is an HTML element, we can update for only its subtree.
    element->node_document()->UpdateComputedStyleOnElementAndAncestor(
        element->AsHTMLElement());
  } else {
    DCHECK(element->node_document());
    element->node_document()->UpdateComputedStyles();
  }

  // Any element with hidden/collapsed visibility is not shown.
  base::optional<std::string> visiblity_style =
      GetComputedStyle(element, &cssom::CSSComputedStyleData::visibility);
  if (visiblity_style && *visiblity_style == cssom::kHiddenKeywordName) {
    return false;
  }

  // If element and its ancestors are not displayed, return false.
  if (!AreElementAndAncestorsDisplayed(element)) {
    return false;
  }

  if (IsTransparent(element)) {
    return false;
  }

  // Any element without positive size dimensions is not shown.
  if (!HasPositiveSizeDimensions(element)) {
    return false;
  }

  if (IsHiddenByOverflow(element)) {
    scoped_refptr<dom::NodeList> child_nodes = element->child_nodes();
    DCHECK(child_nodes);
    // If all children are hidden, then this one is hidden.
    for (uint32 i = 0; i < child_nodes->length(); ++i) {
      scoped_refptr<dom::Node> child = child_nodes->Item(i);
      scoped_refptr<dom::Element> child_as_element = child->AsElement();
      bool child_is_hidden = !child_as_element ||
                             IsHiddenByOverflow(child_as_element.get()) ||
                             !HasPositiveSizeDimensions(child_as_element.get());
      if (!child_is_hidden) {
        return true;
      }
    }
    return false;
  }
  return true;
}

}  // namespace algorithms
}  // namespace webdriver
}  // namespace cobalt
