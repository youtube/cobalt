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

#ifndef COBALT_CSSOM_CSS_DECLARED_STYLE_DECLARATION_H_
#define COBALT_CSSOM_CSS_DECLARED_STYLE_DECLARATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/script/exception_state.h"

namespace cobalt {
namespace cssom {

class CSSParser;

// A CSS declaration block is an ordered collection of CSS properties with their
// associated values, also named CSS declarations. In the DOM a CSS declaration
// block is a CSSStyleDeclaration object.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#css-declaration-blocks
// The CSSDeclaredStyleDeclaration implements a CSS Declaration block
// for declared styles, such as css style rules and inline styles.
class CSSDeclaredStyleDeclaration : public CSSStyleDeclaration {
 public:
  using CSSStyleDeclaration::SetProperty;

  explicit CSSDeclaredStyleDeclaration(CSSParser* css_parser);

  CSSDeclaredStyleDeclaration(const scoped_refptr<CSSDeclaredStyleData>& data,
                              CSSParser* css_parser);

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

  const scoped_refptr<CSSDeclaredStyleData>& data() const { return data_; }
  void set_mutation_observer(MutationObserver* observer) {
    mutation_observer_ = observer;
  }

  void AssignFrom(const CSSDeclaredStyleDeclaration& rhs);

 private:
  // From CSSStyleDeclaration.
  std::string GetDeclaredPropertyValueStringByKey(
      const PropertyKey key) const override;

  // Custom.
  void RecordMutation();

  scoped_refptr<CSSDeclaredStyleData> data_;

  CSSParser* const css_parser_;
  MutationObserver* mutation_observer_;

  DISALLOW_COPY_AND_ASSIGN(CSSDeclaredStyleDeclaration);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_DECLARED_STYLE_DECLARATION_H_
