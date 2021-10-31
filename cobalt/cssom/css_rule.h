// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_CSS_RULE_H_
#define COBALT_CSSOM_CSS_RULE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSRuleVisitor;
class CSSStyleRule;
class CSSMediaRule;
class CSSStyleSheet;

// The CSSRule interface represents an abstract, base CSS style rule.
// Each distinct CSS style rule type is represented by a distinct interface
// that inherits from this interface.
//   https://www.w3.org/TR/2013/WD-cssom-20131205/#the-cssrule-interface
class CSSRule : public script::Wrappable,
                public base::SupportsWeakPtr<CSSRule> {
 public:
  // Web API: CSSRule
  //
  typedef uint16 Type;
  enum TypeInternal {
    kStyleRule = 1,
    kCharsetRule = 2,
    kImportRule = 3,
    kMediaRule = 4,
    kFontFaceRule = 5,
    kPageRule = 6,
    kKeyframesRule = 7,
    kKeyframeRule = 8,
    kMarginRule = 9,
    kNamespaceRule = 10,
  };
  virtual Type type() const = 0;

  virtual std::string css_text(
      script::ExceptionState *exception_state) const = 0;
  virtual void set_css_text(const std::string &css_text,
                            script::ExceptionState *exception_state) = 0;

  scoped_refptr<CSSRule> parent_rule() const;

  scoped_refptr<CSSStyleSheet> parent_style_sheet() const;

  // Custom, not in any spec.
  //
  void set_parent_rule(CSSRule *parent_rule);

  void set_parent_style_sheet(CSSStyleSheet *parent_style_sheet);

  int index() const { return index_; }

  // This may be overridden for derived classes representing groups of rules
  // such that when the parent rule's index is updated, the children's rules
  // are also updated.
  virtual void SetIndex(int index) { index_ = index; }

  // Returns how many indices this rule consumes within its parent rule list.
  // It is useful to override this value for rule groups such that each inner
  // rule is represented as an index within all its parents.
  virtual int IndexWidth() const { return 1; }

  virtual void Accept(CSSRuleVisitor *visitor) = 0;
  virtual void AttachToCSSStyleSheet(CSSStyleSheet *style_sheet) = 0;
  virtual CSSStyleRule *AsCSSStyleRule() { return NULL; }
  virtual CSSMediaRule *AsCSSMediaRule() { return NULL; }

  DEFINE_WRAPPABLE_TYPE(CSSRule);

 protected:
  CSSRule();
  virtual ~CSSRule() {}

 private:
  int index_ = 0;
  base::WeakPtr<CSSRule> parent_rule_;
  base::WeakPtr<CSSStyleSheet> parent_style_sheet_;

  DISALLOW_COPY_AND_ASSIGN(CSSRule);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_RULE_H_
