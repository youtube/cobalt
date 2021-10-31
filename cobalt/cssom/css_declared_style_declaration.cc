// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_declared_style_declaration.h"

#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/mutation_observer.h"

namespace cobalt {
namespace cssom {

namespace {

struct NonTrivialStaticFields {
  NonTrivialStaticFields()
      : location(base::SourceLocation(
            CSSStyleDeclaration::GetSourceLocationName(), 1, 1)) {}

  const base::SourceLocation location;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

// |non_trivial_static_fields| will be lazily created on the first time it's
// accessed.
base::LazyInstance<NonTrivialStaticFields>::DestructorAtExit
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

CSSDeclaredStyleDeclaration::CSSDeclaredStyleDeclaration(CSSParser* css_parser)
    : css_parser_(css_parser), mutation_observer_(NULL) {}

CSSDeclaredStyleDeclaration::CSSDeclaredStyleDeclaration(
    const scoped_refptr<CSSDeclaredStyleData>& data, CSSParser* css_parser)
    : data_(data), css_parser_(css_parser), mutation_observer_(NULL) {
  DCHECK(data_.get());
}

// This returns the result of serializing a CSS declaration block.
// The current implementation does not handle shorthands.
//   https://www.w3.org/TR/cssom/#serialize-a-css-declaration-block
std::string CSSDeclaredStyleDeclaration::css_text(
    script::ExceptionState* exception_state) const {
  return data_ ? data_->SerializeCSSDeclarationBlock() : std::string();
}

void CSSDeclaredStyleDeclaration::set_css_text(
    const std::string& css_text, script::ExceptionState* exception_state) {
  TRACE_EVENT0("cobalt::cssom", "CSSDeclaredStyleDeclaration::set_css_text");
  DCHECK(css_parser_);
  scoped_refptr<CSSDeclaredStyleData> declaration =
      css_parser_->ParseStyleDeclarationList(
          css_text, non_trivial_static_fields.Get().location);

  bool changed = true;

  if (declaration) {
    if (data_ && *data_ == *declaration) {
      changed = false;
    }
    data_ = declaration;
  } else {
    if (!data_) {
      changed = false;
    }
    data_ = NULL;
  }

  if (changed) {
    RecordMutation();
  }
}

// The length attribute must return the number of CSS declarations in the
// declarations.
//   https://www.w3.org/TR/cssom/#dom-cssstyledeclaration-length
unsigned int CSSDeclaredStyleDeclaration::length() const {
  return data_ ? data_->length() : 0;
}

// The item(index) method must return the property name of the CSS declaration
// at position index.
//   https://www.w3.org/TR/cssom/#dom-cssstyledeclaration-item
base::Optional<std::string> CSSDeclaredStyleDeclaration::Item(
    unsigned int index) const {
  const char* item = data_ ? data_->Item(index) : NULL;
  return item ? base::Optional<std::string>(item) : base::nullopt;
}

std::string CSSDeclaredStyleDeclaration::GetDeclaredPropertyValueStringByKey(
    const PropertyKey key) const {
  if (key > kMaxLonghandPropertyKey) {
    // Shorthand properties are never directly stored as declared properties,
    // but are expanded into their longhand property components during parsing.
    // TODO: Implement serialization of css values, see
    // https://www.w3.org/TR/cssom-1/#serializing-css-values
    DCHECK_LE(key, kMaxEveryPropertyKey);
    NOTIMPLEMENTED();
    DLOG(WARNING) << "Unsupported property query for \"" << GetPropertyName(key)
                  << "\": Returning of property value strings is not "
                     "supported for shorthand properties.";
    return std::string();
  }
  return (data_ && key != kNoneProperty) ? data_->GetPropertyValueString(key)
                                         : std::string();
}

void CSSDeclaredStyleDeclaration::SetPropertyValue(
    const std::string& property_name, const std::string& property_value,
    script::ExceptionState* exception_state) {
  DCHECK(css_parser_);
  if (!data_) {
    data_ = new CSSDeclaredStyleData();
  }
  css_parser_->ParsePropertyIntoDeclarationData(
      property_name, property_value, non_trivial_static_fields.Get().location,
      data_.get());

  RecordMutation();
}

void CSSDeclaredStyleDeclaration::SetProperty(
    const std::string& property_name, const std::string& property_value,
    const std::string& priority, script::ExceptionState* exception_state) {
  DLOG(INFO) << "CSSDeclaredStyleDeclaration::SetProperty(" << property_name
             << "," << property_value << "," << priority << ")";
  DCHECK(css_parser_);
  if (!data_) {
    data_ = new CSSDeclaredStyleData();
  }
  css_parser_->ParsePropertyIntoDeclarationData(
      property_name, property_value, non_trivial_static_fields.Get().location,
      data_.get());

  RecordMutation();
}

void CSSDeclaredStyleDeclaration::RecordMutation() {
  if (mutation_observer_) {
    // Trigger layout update.
    mutation_observer_->OnCSSMutation();
  }
}

void CSSDeclaredStyleDeclaration::AssignFrom(
    const CSSDeclaredStyleDeclaration& rhs) {
  if (!rhs.data_) {
    data_ = NULL;
    return;
  }
  if (!data_) {
    data_ = new CSSDeclaredStyleData();
  }
  data_->AssignFrom(*rhs.data_);
}

}  // namespace cssom
}  // namespace cobalt
