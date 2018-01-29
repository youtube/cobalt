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

#ifndef COBALT_CSSOM_CSS_FONT_FACE_DECLARATION_DATA_H_
#define COBALT_CSSOM_CSS_FONT_FACE_DECLARATION_DATA_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_declaration_data.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// CSSFontFaceDeclarationData which has PropertyValue type properties only used
// internally and it is not exposed to JavaScript.
class CSSFontFaceDeclarationData : public CSSDeclarationData {
 public:
  CSSFontFaceDeclarationData();

  // From CSSDeclarationData
  bool IsSupportedPropertyKey(PropertyKey key) const override;
  scoped_refptr<PropertyValue> GetPropertyValue(PropertyKey key) const override;
  void SetPropertyValueAndImportance(
      PropertyKey key, const scoped_refptr<PropertyValue>& property_value,
      bool important) override {
    UNREFERENCED_PARAMETER(important);
    SetPropertyValue(key, property_value);
  }
  void ClearPropertyValueAndImportance(PropertyKey key) override {
    SetPropertyValue(key, NULL);
  }

  // Web API: CSSFontFaceRule
  //
  const scoped_refptr<PropertyValue>& family() const { return family_; }
  void set_family(const scoped_refptr<PropertyValue>& family) {
    family_ = family;
  }

  const scoped_refptr<PropertyValue>& src() const { return src_; }
  void set_src(const scoped_refptr<PropertyValue>& src) { src_ = src; }

  const scoped_refptr<PropertyValue>& style() const { return style_; }
  void set_style(const scoped_refptr<PropertyValue>& style) { style_ = style; }

  const scoped_refptr<PropertyValue>& weight() const { return weight_; }
  void set_weight(const scoped_refptr<PropertyValue>& weight) {
    weight_ = weight;
  }

  const scoped_refptr<PropertyValue>& unicode_range() const {
    return unicode_range_;
  }
  void set_unicode_range(const scoped_refptr<PropertyValue>& unicode_range) {
    unicode_range_ = unicode_range;
  }

  // Rest of public methods.

  void SetPropertyValue(PropertyKey key,
                        const scoped_refptr<PropertyValue>& property_value);

  void AssignFrom(const CSSFontFaceDeclarationData& rhs);

 private:
  scoped_refptr<PropertyValue>* GetPropertyValueReference(PropertyKey key);

  scoped_refptr<PropertyValue> family_;
  scoped_refptr<PropertyValue> src_;
  scoped_refptr<PropertyValue> style_;
  scoped_refptr<PropertyValue> weight_;
  scoped_refptr<PropertyValue> unicode_range_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_FONT_FACE_DECLARATION_DATA_H_
