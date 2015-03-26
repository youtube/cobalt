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

#ifndef CSSOM_CSS_RULE_LIST_H_
#define CSSOM_CSS_RULE_LIST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSStyleRule;
class CSSStyleSheet;

// The CSSRuleList interface represents an ordered collection of CSS
// style rules.
//   http://dev.w3.org/csswg/cssom/#the-cssrulelist-interface
class CSSRuleList : public base::SupportsWeakPtr<CSSRuleList>,
                    public script::Wrappable {
 public:
  explicit CSSRuleList(
      const scoped_refptr<const CSSStyleSheet>& css_style_sheet);

  // Web API: CSSRuleList
  //

  // Returns the index-th CSSRule object in the collection.
  // Returns null if there is no index-th object in the collection.
  scoped_refptr<CSSStyleRule> Item(unsigned int index) const;

  // Returns the number of CSSRule objects represented by the collection.
  unsigned int length() const;

 private:
  ~CSSRuleList();

  scoped_refptr<const CSSStyleSheet> css_style_sheet_;

  DISALLOW_COPY_AND_ASSIGN(CSSRuleList);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_RULE_LIST_H_
