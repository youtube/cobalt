// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_COMPUTED_STYLE_DECLARATION_H_
#define COBALT_CSSOM_CSS_COMPUTED_STYLE_DECLARATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/web_animations/animation_set.h"

namespace cobalt {
namespace cssom {

// A CSS declaration block is an ordered collection of CSS properties with their
// associated values, also named CSS declarations. In the DOM a CSS declaration
// block is a CSSStyleDeclaration object.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#css-declaration-blocks
// The CSSComputedStyleDeclaration implements a CSS Declaration block
// for computed styles.
class CSSComputedStyleDeclaration : public CSSStyleDeclaration {
 public:
  using CSSStyleDeclaration::SetProperty;

  CSSComputedStyleDeclaration() {}
  // From CSSStyleDeclaration.

  std::string css_text(script::ExceptionState* exception_state) const override;
  void set_css_text(const std::string& css_text,
                    script::ExceptionState* exception_state) override;

  unsigned int length() const override;

  base::optional<std::string> Item(unsigned int index) const override;

  void SetPropertyValue(const std::string& property_name,
                        const std::string& property_value,
                        script::ExceptionState* exception_state) override;

  void SetProperty(const std::string& property_name,
                   const std::string& property_value,
                   const std::string& priority,
                   script::ExceptionState* exception_state) override;

  // Custom.

  const scoped_refptr<const CSSComputedStyleData>& data() const {
    return data_;
  }
  void SetData(const scoped_refptr<const CSSComputedStyleData>& data);

  // Updates the pointer to the nearest CSSComputedStyleData ancestor,
  // inclusive, which has inherited properties declared.
  void UpdateInheritedData();

  // Returns whether or not this object or any ancestors have inherited
  // properties declared.
  bool HasInheritedProperties() const {
    return data_with_inherited_properties_ != NULL;
  }

  // Returns the reference to the property value for an inherited property.
  // Should only be called if HasInheritedProperties() returns true.
  const scoped_refptr<PropertyValue>& GetInheritedPropertyValueReference(
      PropertyKey key) const;

  const scoped_refptr<const web_animations::AnimationSet>& animations() const {
    return animations_;
  }
  void set_animations(
      const scoped_refptr<const web_animations::AnimationSet>& animations) {
    animations_ = animations;
  }

 private:
  // From CSSStyleDeclaration.
  std::string GetDeclaredPropertyValueStringByKey(
      const PropertyKey key) const override;

  // The CSSComputedStyleData owned by this object.
  scoped_refptr<const CSSComputedStyleData> data_;

  // The nearest CSSComputedStyleData ancestor, inclusive, which has inherited
  // properties declared.
  scoped_refptr<const CSSComputedStyleData> data_with_inherited_properties_;

  // All animation that applies to the above computed style.
  scoped_refptr<const web_animations::AnimationSet> animations_;

  DISALLOW_COPY_AND_ASSIGN(CSSComputedStyleDeclaration);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_COMPUTED_STYLE_DECLARATION_H_
