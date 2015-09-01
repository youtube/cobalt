/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_CSS_RULE_H_
#define CSSOM_CSS_RULE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/cssom/cascade_priority.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSStyleSheet;
class CSSRuleVisitor;

// The CSSRule interface represents an abstract, base CSS style rule.
// Each distinct CSS style rule type is represented by a distinct interface
// that inherits from this interface.
//   http://dev.w3.org/csswg/cssom/#the-cssrule-interface
class CSSRule : public script::Wrappable,
                public base::SupportsWeakPtr<CSSRule> {
 public:
  // Web API: CSSRule
  enum Type {
    kStyleRule = 1,
    kCharsetRule = 2,
    kImportRule = 3,
    kMediaRule = 4,
    kFontFaceRule = 5,
    kPageRule = 6,
    kMarginRule = 9,
    kNamespaceRule = 10,
  };
  virtual Type type() const = 0;

  virtual std::string css_text() const = 0;
  virtual void set_css_text(const std::string &css_text) = 0;

  scoped_refptr<CSSRule> parent_rule() const { return parent_rule_.get(); }

  scoped_refptr<CSSStyleSheet> parent_style_sheet() const {
    return parent_style_sheet_.get();
  }

  // Custom, not in any spec.
  //

  void set_parent_rule(CSSRule *parent_rule) {
    parent_rule_ = base::AsWeakPtr(parent_rule);
  }
  void set_parent_style_sheet(CSSStyleSheet *parent_style_sheet) {
    parent_style_sheet_ = base::AsWeakPtr(parent_style_sheet);
  }

  int index() { return index_; }
  void set_index(int index) { index_ = index; }

  virtual void Accept(CSSRuleVisitor *visitor) = 0;
  virtual void AttachToCSSStyleSheet(CSSStyleSheet *style_sheet) = 0;

  DEFINE_WRAPPABLE_TYPE(CSSRule);

 protected:
  CSSRule() : index_(Appearance::kUnattached) {}
  virtual ~CSSRule() {}


 private:
  int index_;
  base::WeakPtr<CSSRule> parent_rule_;
  base::WeakPtr<CSSStyleSheet> parent_style_sheet_;

  DISALLOW_COPY_AND_ASSIGN(CSSRule);
};

typedef std::vector<scoped_refptr<CSSRule> > CSSRules;

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_RULE_H_
