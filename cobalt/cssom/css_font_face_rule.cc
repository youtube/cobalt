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

#include "cobalt/cssom/css_font_face_rule.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "cobalt/base/source_location.h"
#include "cobalt/cssom/css_declaration_util.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/style_sheet_list.h"

namespace cobalt {
namespace cssom {
namespace {

struct NonTrivialStaticFields {
  NonTrivialStaticFields()
      : location(base::SourceLocation(CSSFontFaceRule::GetSourceLocationName(),
                                      1, 1)) {}

  const base::SourceLocation location;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

// |non_trivial_static_fields| will be lazily created on the first time it's
// accessed.
base::LazyInstance<NonTrivialStaticFields>::DestructorAtExit
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

CSSFontFaceRule::CSSFontFaceRule() : data_(new CSSFontFaceDeclarationData) {}

CSSFontFaceRule::CSSFontFaceRule(
    const scoped_refptr<CSSFontFaceDeclarationData>& data)
    : data_(data) {
  DCHECK(data_.get());
}

std::string CSSFontFaceRule::css_text(
    script::ExceptionState* exception_state) const {
  std::string css_text;
  AppendPropertyDeclaration(GetPropertyName(kFontFamilyProperty),
                            data_->family(), &css_text);
  AppendPropertyDeclaration(GetPropertyName(kSrcProperty), data_->src(),
                            &css_text);
  AppendPropertyDeclaration(GetPropertyName(kFontStyleProperty), data_->style(),
                            &css_text);
  AppendPropertyDeclaration(GetPropertyName(kFontWeightProperty),
                            data_->weight(), &css_text);
  AppendPropertyDeclaration(GetPropertyName(kUnicodeRangeProperty),
                            data_->unicode_range(), &css_text);

  return css_text;
}

void CSSFontFaceRule::set_css_text(const std::string& css_text,
                                   script::ExceptionState* exception_state) {
  DCHECK(parent_style_sheet());
  scoped_refptr<CSSFontFaceDeclarationData> declaration =
      parent_style_sheet()->css_parser()->ParseFontFaceDeclarationList(
          css_text, non_trivial_static_fields.Get().location);

  if (declaration) {
    data_ = declaration;
    RecordMutation();
  }
}

std::string CSSFontFaceRule::family() const {
  return data_->family() ? data_->family()->ToString() : "";
}

void CSSFontFaceRule::set_family(const std::string& family) {
  SetPropertyValue(GetPropertyName(kFontFamilyProperty), family);
}

std::string CSSFontFaceRule::src() const {
  return data_->src() ? data_->src()->ToString() : "";
}

void CSSFontFaceRule::set_src(const std::string& src) {
  SetPropertyValue(GetPropertyName(kSrcProperty), src);
}

std::string CSSFontFaceRule::style() const {
  return data_->style() ? data_->style()->ToString() : "";
}

void CSSFontFaceRule::set_style(const std::string& style) {
  SetPropertyValue(GetPropertyName(kFontStyleProperty), style);
}

std::string CSSFontFaceRule::weight() const {
  return data_->weight() ? data_->weight()->ToString() : "";
}

void CSSFontFaceRule::set_weight(const std::string& weight) {
  SetPropertyValue(GetPropertyName(kFontWeightProperty), weight);
}

std::string CSSFontFaceRule::unicode_range() const {
  return data_->unicode_range() ? data_->unicode_range()->ToString() : "";
}

void CSSFontFaceRule::set_unicode_range(const std::string& unicode_range) {
  SetPropertyValue(GetPropertyName(kUnicodeRangeProperty), unicode_range);
}

void CSSFontFaceRule::Accept(CSSRuleVisitor* visitor) {
  visitor->VisitCSSFontFaceRule(this);
}

void CSSFontFaceRule::AttachToCSSStyleSheet(CSSStyleSheet* style_sheet) {
  set_parent_style_sheet(style_sheet);
}

std::string CSSFontFaceRule::GetPropertyValue(PropertyKey key) {
  return data_->GetPropertyValue(key) ? data_->GetPropertyValue(key)->ToString()
                                      : "";
}

void CSSFontFaceRule::SetPropertyValue(const std::string& property_name,
                                       const std::string& property_value) {
  DCHECK(parent_style_sheet());
  parent_style_sheet()->css_parser()->ParsePropertyIntoDeclarationData(
      property_name, property_value,
      base::SourceLocation(non_trivial_static_fields.Get().location),
      data_.get());

  RecordMutation();
}

// font-family and src are required for an @font-face rule to be valid:
//   https://www.w3.org/TR/css3-fonts/#descdef-font-family
//   https://www.w3.org/TR/css3-fonts/#descdef-src
bool CSSFontFaceRule::IsValid() const {
  return !family().empty() && !src().empty();
}

CSSFontFaceRule::~CSSFontFaceRule() {}

void CSSFontFaceRule::RecordMutation() {
  DCHECK(parent_style_sheet());
  DCHECK(parent_style_sheet()->ParentStyleSheetList());

  // Trigger layout update.
  parent_style_sheet()->ParentStyleSheetList()->OnCSSMutation();
}

}  // namespace cssom
}  // namespace cobalt
