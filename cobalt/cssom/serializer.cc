// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/serializer.h"
#include "cobalt/cssom/active_pseudo_class.h"
#include "cobalt/cssom/after_pseudo_element.h"
#include "cobalt/cssom/attribute_selector.h"
#include "cobalt/cssom/before_pseudo_element.h"
#include "cobalt/cssom/class_selector.h"
#include "cobalt/cssom/complex_selector.h"
#include "cobalt/cssom/compound_selector.h"
#include "cobalt/cssom/empty_pseudo_class.h"
#include "cobalt/cssom/focus_pseudo_class.h"
#include "cobalt/cssom/hover_pseudo_class.h"
#include "cobalt/cssom/id_selector.h"
#include "cobalt/cssom/not_pseudo_class.h"
#include "cobalt/cssom/simple_selector.h"
#include "cobalt/cssom/type_selector.h"

namespace cobalt {
namespace cssom {

namespace {
// Used to replace an unknown, unrecognized or unrepresentable character.
constexpr int kUnicodeReplacementCharacter = 0xFFFD;
constexpr char kUnicodeReplacementCharacterUtf8[] = u8"\uFFFD";
}  // namespace

Serializer::Serializer(std::string* output) : output_(output) {}

void Serializer::SerializeIdentifier(base::Token identifier) {
  // https://www.w3.org/TR/cssom/#serialize-an-identifier
  //
  // To serialize an identifier means to create a string represented by the
  // concatenation of, for each character of the identifier: For each character
  // of the identifier:
  int char_num = 0;
  uint32_t first_char = 0;
  const uint8_t* next_p = reinterpret_cast<const uint8_t*>(identifier.c_str());
  while (*next_p) {
    uint32_t c;  // code point
    const uint8_t* curr_p = next_p;
    next_p += DecodeUTF8(curr_p, &c);

    char_num++;
    if (char_num == 1) first_char = c;

    // If the character is NULL (U+0000), then the REPLACEMENT CHARACTER
    // (U+FFFD).
    if (c == 0x00) {
      output_->append(kUnicodeReplacementCharacterUtf8);
      continue;
    }

    // If the character is in the range [\1-\1f] (U+0001 to U+001F) or is
    // U+007F, then the character escaped as code point.
    if ((0x01 <= c && c <= 0x1F) || c == 0x7f) {
      EscapeCodePoint(c);
      continue;
    }

    // If the character is the first character and is in the range [0-9] (U+0030
    // to U+0039), then the character escaped as code point.
    bool is_numeric = ('0' <= c && c <= '9');
    if (char_num == 1 && is_numeric) {
      EscapeCodePoint(c);
      continue;
    }

    // If the character is the second character and is in the range [0-9]
    // (U+0030 to U+0039) and the first character is a "-" (U+002D), then the
    // character escaped as code point.
    if (char_num == 2 && is_numeric && first_char == '-') {
      EscapeCodePoint(c);
      continue;
    }

    // If the character is the first character and is a "-" (U+002D), and there
    // is no second character, then the escaped character.
    if (char_num == 1 && c == '-' && *next_p == '\0') {
      EscapeCharacter(c);
      continue;
    }

    // If the character is not handled by one of the above rules and is greater
    // than or equal to U+0080, is "-" (U+002D) or "_" (U+005F), or is in one of
    // the ranges [0-9] (U+0030 to U+0039), [A-Z] (U+0041 to U+005A), or \[a-z]
    // (U+0061 to U+007A), then the character itself.
    if (c >= 0x80 || c == '-' || c == '_' || is_numeric ||
        ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')) {
      do {
        output_->push_back(*curr_p);
      } while (++curr_p < next_p);
      continue;
    }

    // Otherwise, the escaped character.
    EscapeCharacter(c);
  }
}

void Serializer::SerializeString(const std::string& string) {
  // https://www.w3.org/TR/cssom/#serialize-a-string
  //
  // Create a string represented by '"' (U+0022), followed by the result of
  // applying the rules below to each character of the given string, followed by
  // '"' (U+0022):
  output_->push_back('"');

  const uint8_t* p = reinterpret_cast<const uint8_t*>(string.c_str());
  for (size_t n = 0, length = string.length(); n < length; ++n) {
    uint8_t c = p[n];

    // If the character is NULL (U+0000), then the REPLACEMENT CHARACTER
    // (U+FFFD) escaped as code point.
    if (c == 0x00) {
      EscapeCodePoint(kUnicodeReplacementCharacter);
      continue;
    }

    // If the character is in the range [\1-\1f] (U+0001 to U+001F) or is
    // U+007F, the character escaped as code point.
    if ((0x01 <= c && c <= 0x1F) || c == 0x7f) {
      EscapeCodePoint(c);
      continue;
    }

    // If the character is '"' (U+0022) or "\" (U+005C), the escaped character.
    if (c == 0x22 || c == 0x5c) {
      EscapeCharacter(c);
      continue;
    }

    // Otherwise, the character itself.
    output_->push_back(c);
  }

  // ...followed by '"' (U+0022)
  output_->push_back('"');
}

void Serializer::SerializeSelectors(const Selectors& selectors) {
  // https://www.w3.org/TR/cssom/#serializing-selectors
  //
  // To serialize a group of selectors serialize each selector in the group of
  // selectors and then serialize the group.
  for (auto it = selectors.begin(); it != selectors.end(); it++) {
    // https://www.w3.org/TR/cssom/#serialize-a-comma-separated-list
    //
    // To serialize a comma-separated list concatenate all items of the list in
    // list order while separating them by ", ", i.e., COMMA (U+002C) followed
    // by a single SPACE (U+0020).
    if (it != selectors.begin()) {
      output_->append(", ");
    }
    SerializeSelector(**it);
  }
}

void Serializer::SerializeSelector(const Selector& selector) {
  const_cast<Selector*>(&selector)->Accept(this);
}

int Serializer::DecodeUTF8(const uint8_t* p, uint32_t* out_c) {
  DCHECK(p && *p);
  uint32_t c;
  int len;
  if (p[0] < 0x80) {
    *out_c = p[0];
    return 1;
  } else if (p[0] < 0xC0) {
    DLOG(ERROR) << "Bad UTF-8 first byte";
    *out_c = kUnicodeReplacementCharacter;
    return 1;
  } else if (p[0] < 0xE0) {
    c = p[0] & 0x1F;
    len = 2;
  } else if (p[0] < 0xF0) {
    c = p[0] & 0x0F;
    len = 3;
  } else if (p[0] < 0xF8) {
    c = p[0] & 0x07;
    len = 4;
  } else {
    DLOG(ERROR) << "Bad UTF-8 first byte";
    *out_c = kUnicodeReplacementCharacter;
    return 1;
  }
  for (int n = 1; n < len; ++n) {
    if ((p[n] & 0xC0) != 0x80) {
      DLOG(ERROR) << "Bad UTF-8 byte " << n;
      *out_c = kUnicodeReplacementCharacter;
      return n;
    }
    c <<= 6;
    c |= p[n] & 0x3F;
  }
  *out_c = c;
  return len;
}

void Serializer::EscapeCodePoint(uint32_t c) {
  // Highest valid code point in Unicode.
  DCHECK_LE(c, 0x10FFFFu);

  // To escape a character as code point means to create a string of "\"
  // (U+005C), followed by the Unicode code point as the smallest possible
  // number of hexadecimal digits in the range 0-9 a-f (U+0030 to U+0039 and
  // U+0061 to U+0066) to represent the code point in base 16, followed by a
  // single SPACE (U+0020).
  output_->push_back('\\');

  constexpr char kHexDigits[] = "0123456789abcdef";
  char buffer[9];
  int pos = sizeof(buffer);
  buffer[--pos] = '\0';
  while (c != 0) {
    buffer[--pos] = kHexDigits[c & 0x0F];
    c >>= 4;
  }
  if (pos + 1 == sizeof(buffer)) buffer[--pos] = '0';
  output_->append(buffer + pos);

  // ...followed by a single SPACE (U+0020).
  output_->push_back(' ');
}

void Serializer::EscapeCharacter(uint32_t c) {
  // To escape a character means to create a string of "\" (U+005C), followed by
  // the character.
  DCHECK_GE(c, 0x20u);
  DCHECK_LE(c, 0x7Fu);
  output_->push_back('\\');
  output_->push_back(static_cast<char>(c));
}

void Serializer::VisitUniversalSelector(UniversalSelector* universal_selector) {
  // https://www.w3.org/TR/cssom/#serialize-a-selector
  // If this is a universal selector append "*" (U+002A) to s.
  output_->push_back('*');
}

void Serializer::VisitTypeSelector(TypeSelector* type_selector) {
  // https://www.w3.org/TR/cssom/#serialize-a-selector
  // If this is a type selector append the escaped element name to s.
  SerializeIdentifier(type_selector->element_name());
}

void Serializer::VisitAttributeSelector(AttributeSelector* attribute_selector) {
  // https://www.w3.org/TR/cssom/#serialize-a-selector

  // 1. Append "[" (U+005B) to s.
  output_->push_back('[');

  // 2. If the namespace prefix maps to a namespace that is not the null
  // namespace (not in a namespace) append the escaped namespace prefix,
  // followed by a "|" (U+007C) to s.
  // [Cobalt doesn't support @namespace]

  // 3. Append the escaped attribute name to s.
  SerializeIdentifier(attribute_selector->attribute_name());

  // 4. If there is an attribute value specified, append "=", "~=", "|=", "^=",
  // "$=", or "*=" as appropriate (depending on the type of attribute selector),
  // followed by the string escaped attribute value, to s.
  const char* match_op = nullptr;
  switch (attribute_selector->value_match_type()) {
    case AttributeSelector::kNoMatch:
      match_op = nullptr;
      break;
    case AttributeSelector::kEquals:
      match_op = "=";
      break;
    case AttributeSelector::kIncludes:
      match_op = "~=";
      break;
    case AttributeSelector::kDashMatch:
      match_op = "|=";
      break;
    case AttributeSelector::kBeginsWith:
      match_op = "^=";
      break;
    case AttributeSelector::kEndsWith:
      match_op = "$=";
      break;
    case AttributeSelector::kContains:
      match_op = "*=";
      break;
  }
  if (match_op != nullptr) {
    output_->append(match_op);
    SerializeString(attribute_selector->attribute_value());
  }

  // 5. If the attribute selector has the case-sensitivity flag present, append
  // " i" (U+0020 U+0069) to s.
  // [Cobalt doesn't support the CSS4 case-sensitivity attributes.]

  // 6. Append "]" (U+005D) to s.
  output_->push_back(']');
}

void Serializer::VisitClassSelector(ClassSelector* class_selector) {
  // https://www.w3.org/TR/cssom/#serialize-a-selector
  // Append a "." (U+002E), followed by the escaped class name to s.
  output_->push_back('.');
  SerializeIdentifier(class_selector->class_name());
}

void Serializer::VisitIdSelector(IdSelector* id_selector) {
  // https://www.w3.org/TR/cssom/#serialize-a-selector
  // Append a "#" (U+0023), followed by the escaped ID to s.
  output_->push_back('#');
  SerializeIdentifier(id_selector->id());
}

void Serializer::VisitPseudoClass(PseudoClass* pseudo_class) {
  // https://www.w3.org/TR/cssom/#serialize-a-selector
  // If the pseudo-class does not accept arguments append ":" (U+003A),
  // followed by the name of the pseudo-class, to s.
  output_->push_back(':');
  output_->append(pseudo_class->text().c_str());
}

void Serializer::VisitActivePseudoClass(
    ActivePseudoClass* active_pseudo_class) {
  VisitPseudoClass(active_pseudo_class);
}

void Serializer::VisitEmptyPseudoClass(EmptyPseudoClass* empty_pseudo_class) {
  VisitPseudoClass(empty_pseudo_class);
}

void Serializer::VisitFocusPseudoClass(FocusPseudoClass* focus_pseudo_class) {
  VisitPseudoClass(focus_pseudo_class);
}

void Serializer::VisitHoverPseudoClass(HoverPseudoClass* hover_pseudo_class) {
  VisitPseudoClass(hover_pseudo_class);
}

void Serializer::VisitNotPseudoClass(NotPseudoClass* not_pseudo_class) {
  // https://www.w3.org/TR/cssom/#serialize-a-simple-selector
  //
  // Append ":" (U+003A), followed by the name of the pseudo-class, followed by
  // "(" (U+0028), followed by the value of the pseudo-class argument(s)
  // determined as per below, followed by ")" (U+0029), to s.
  // ...
  // :not() - The result of serializing the value using the rules for
  // serializing a group of selectors.
  VisitPseudoClass(not_pseudo_class);
  output_->push_back('(');
  not_pseudo_class->selector()->Accept(this);
  output_->push_back(')');
}

void Serializer::VisitPseudoElement(PseudoElement* pseudo_element) {
  // https://www.w3.org/TR/cssom/#serialize-a-selector
  // If this is the last part of the chain of the selector and there is a
  // pseudo-element, append "::" followed by the name of the pseudo-element,
  // to s.
  output_->append("::");
  output_->append(pseudo_element->text().c_str());
}

void Serializer::VisitAfterPseudoElement(
    AfterPseudoElement* after_pseudo_element) {
  VisitPseudoElement(after_pseudo_element);
}

void Serializer::VisitBeforePseudoElement(
    BeforePseudoElement* before_pseudo_element) {
  VisitPseudoElement(before_pseudo_element);
}

void Serializer::VisitCompoundSelector(CompoundSelector* compound_selector) {
  // https://www.w3.org/TR/cssom/#serialize-a-selector
  //
  // 1. If there is only one simple selector in the compound selectors which is
  // a universal selector, append the result of serializing the universal
  // selector to s.
  if (compound_selector->simple_selectors().size() == 1 &&
      compound_selector->simple_selectors().front()->AsUniversalSelector()) {
    compound_selector->simple_selectors().front()->Accept(this);
    return;
  }

  // 2. Otherwise, for each simple selector in the compound selectors that is
  // not a universal selector of which the namespace prefix maps to a namespace
  // that is not the default namespace serialize the simple selector and append
  // the result to s.
  // [Cobalt doesn't support @namespace]
  for (CompoundSelector::SimpleSelectors::const_iterator iter =
           compound_selector->simple_selectors().begin();
       iter != compound_selector->simple_selectors().end(); ++iter) {
    if ((*iter)->AsUniversalSelector() == NULL) {
      (*iter)->Accept(this);
    }
  }
}

void Serializer::VisitComplexSelector(ComplexSelector* complex_selector) {
  // https://www.w3.org/TR/cssom/#serialize-a-selector
  //
  // If this is not the last part of the chain of the selector append a single
  // SPACE (U+0020), followed by the combinator ">", "+", "~", ">>", "||", as
  // appropriate, followed by another single SPACE (U+0020) if the combinator
  // was not whitespace, to s.
  CompoundSelector* selector = complex_selector->first_selector();
  if (!selector) return;
  selector->Accept(this);
  Combinator* combinator = selector->right_combinator();
  while (combinator) {
    // The |VisitFooCombinator| methods below add the spaces before & after.
    combinator->Accept(this);
    selector = combinator->right_selector();
    selector->Accept(this);
    combinator = selector->right_combinator();
  }
}

void Serializer::VisitChildCombinator(ChildCombinator* child_combinator) {
  output_->append(" > ");
}

void Serializer::VisitNextSiblingCombinator(
    NextSiblingCombinator* next_sibling_combinator) {
  output_->append(" + ");
}

void Serializer::VisitDescendantCombinator(
    DescendantCombinator* descendant_combinator) {
  output_->push_back(' ');
}

void Serializer::VisitFollowingSiblingCombinator(
    FollowingSiblingCombinator* following_sibling_combinator) {
  output_->append(" ~ ");
}

}  // namespace cssom
}  // namespace cobalt
