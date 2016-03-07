/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
  CSSComputedStyleDeclaration() {}
  // From CSSStyleDeclaration.

  std::string css_text(script::ExceptionState* exception_state) const OVERRIDE;
  void set_css_text(const std::string& css_text,
                    script::ExceptionState* exception_state) OVERRIDE;

  unsigned int length() const OVERRIDE;

  base::optional<std::string> Item(unsigned int index) const OVERRIDE;

  void SetPropertyValue(const std::string& property_name,
                        const std::string& property_value,
                        script::ExceptionState* exception_state) OVERRIDE;

  // Custom.

  const scoped_refptr<const CSSComputedStyleData>& data() const {
    return data_;
  }
  void set_data(const scoped_refptr<const CSSComputedStyleData>& data) {
    data_ = data;
  }

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
      const PropertyKey key) const OVERRIDE;
  scoped_refptr<const CSSComputedStyleData> data_;

  // All animation that applies to  the above computed style.
  scoped_refptr<const web_animations::AnimationSet> animations_;

  DISALLOW_COPY_AND_ASSIGN(CSSComputedStyleDeclaration);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_COMPUTED_STYLE_DECLARATION_H_
