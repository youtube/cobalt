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

#include "cobalt/dom/dom_string_map.h"

#include "base/strings/string_util.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"

namespace cobalt {
namespace dom {

namespace {

const char kDataPrefix[] = "data-";
// Subtract one for nul terminator.
const size_t kDataPrefixLength = sizeof(kDataPrefix) - 1;

// See "The algorithm for getting the list of name-value pairs" at
// https://www.w3.org/TR/html50/dom.html#dom-dataset.
base::Optional<std::string> TryConvertAttributeNameToPropertyName(
    const std::string& attribute_name) {
  // First five characters of attribute name should be "data-".
  if (attribute_name.compare(0, kDataPrefixLength, kDataPrefix) != 0) {
    return base::nullopt;
  }

  // For each "-" (U+002D) character in the name that is followed by
  // a lowercase ASCII letter, remove the "-" (U+002D) character and replace
  // the character that followed it by the same character converted to ASCII
  // uppercase.
  std::string property_name;
  bool preceded_by_hyphen = false;
  for (std::string::const_iterator attribute_name_iterator =
           attribute_name.begin() + kDataPrefixLength;
       attribute_name_iterator != attribute_name.end();
       ++attribute_name_iterator) {
    char attribute_name_character = *attribute_name_iterator;

    if (attribute_name_character == '-') {
      // Double hyphen in attribute name, preserve it.
      if (preceded_by_hyphen) {
        property_name += '-';
      } else {
        preceded_by_hyphen = true;
      }
      continue;
    }

    // Attribute name should not contain uppercase ASCII characters.
    if (base::ToLowerASCII(attribute_name_character) !=
        attribute_name_character) {
      return base::nullopt;
    }

    // Convert to uppercase character if preceded by hyphen.
    char property_name_character = attribute_name_character;
    if (preceded_by_hyphen) {
      preceded_by_hyphen = false;
      property_name_character = base::ToUpperASCII(property_name_character);

      // Non-letter character after hyphen, preserve the hyphen.
      if (property_name_character == attribute_name_character) {
        property_name += '-';
      }
    }

    property_name += property_name_character;
  }
  if (preceded_by_hyphen) {
    property_name += '-';
  }
  return property_name;
}

// See "The algorithm for setting names to certain values" at
// https://www.w3.org/TR/html50/dom.html#dom-dataset.
base::Optional<std::string> TryConvertPropertyNameToAttributeName(
    const std::string& property_name) {
  // Insert the string "data-" at the front of attribute name.
  std::string attribute_name = kDataPrefix;

  bool preceded_by_hyphen = false;
  for (std::string::const_iterator property_name_iterator =
           property_name.begin();
       property_name_iterator != property_name.end();
       ++property_name_iterator) {
    char property_name_character = *property_name_iterator;

    // If property name contains a "-" (U+002D) character followed by
    // a lowercase ASCII letter, abort these steps.
    if (preceded_by_hyphen &&
        base::ToUpperASCII(property_name_character) !=
            property_name_character) {
      return base::nullopt;
    }

    // For each uppercase ASCII letter in name, insert a "-" (U+002D) character
    // before the character and replace the character with the same character
    // converted to ASCII lowercase.
    if (base::ToLowerASCII(property_name_character) !=
        property_name_character) {
      attribute_name += '-';
      attribute_name += base::ToLowerASCII(property_name_character);
    } else {
      attribute_name += property_name_character;
    }

    preceded_by_hyphen = property_name_character == '-';
  }
  return attribute_name;
}

}  // namespace

DOMStringMap::DOMStringMap(const scoped_refptr<Element>& element)
    : element_(element) {
  GlobalStats::GetInstance()->Add(this);
}

base::Optional<std::string> DOMStringMap::AnonymousNamedGetter(
    const std::string& property_name, script::ExceptionState* exception_state) {
  base::Optional<std::string> attribute_name =
      TryConvertPropertyNameToAttributeName(property_name);
  if (attribute_name) {
    return element_->GetAttribute(*attribute_name);
  } else {
    exception_state->SetSimpleException(script::kSyntaxError,
                                        property_name.c_str());
    return base::nullopt;
  }
}

void DOMStringMap::AnonymousNamedSetter(
    const std::string& property_name, const std::string& value,
    script::ExceptionState* exception_state) {
  base::Optional<std::string> attribute_name =
      TryConvertPropertyNameToAttributeName(property_name);
  if (attribute_name) {
    element_->SetAttribute(*attribute_name, value);
  } else {
    exception_state->SetSimpleException(script::kSyntaxError,
                                        property_name.c_str());
  }
}

bool DOMStringMap::CanQueryNamedProperty(
    const std::string& property_name) const {
  base::Optional<std::string> attribute_name =
      TryConvertPropertyNameToAttributeName(property_name);
  // TODO: Throw a SyntaxError if attribute name is invalid once getters and
  // setters support throwing exceptions.
  return attribute_name && element_->HasAttribute(*attribute_name);
}

void DOMStringMap::EnumerateNamedProperties(
    script::PropertyEnumerator* enumerator) {
  for (Element::AttributeMap::const_iterator
           attribute_iterator = element_->attribute_map().begin(),
           attribute_end_iterator = element_->attribute_map().end();
       attribute_iterator != attribute_end_iterator; ++attribute_iterator) {
    base::Optional<std::string> property_name =
        TryConvertAttributeNameToPropertyName(attribute_iterator->first);
    if (property_name) {
      enumerator->AddProperty(*property_name);
    }
  }
}

DOMStringMap::~DOMStringMap() { GlobalStats::GetInstance()->Remove(this); }

}  // namespace dom
}  // namespace cobalt
