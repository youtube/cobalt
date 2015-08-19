/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef CSSOM_CSS_MEDIA_RULE_H_
#define CSSOM_CSS_MEDIA_RULE_H_

#include "cobalt/cssom/css_grouping_rule.h"

namespace cobalt {
namespace cssom {

class MediaList;
class CSSRuleList;

// The CSSMediaRule interface represents an @media at-rule.
//   http://www.w3.org/TR/cssom/#the-cssmediarule-interface
class CSSMediaRule : public CSSGroupingRule {
 public:
  CSSMediaRule(const scoped_refptr<MediaList>& media_list,
               const scoped_refptr<CSSRuleList>& css_rule_list);

  // Custom, not in any spec.
  //

  DEFINE_WRAPPABLE_TYPE(CSSMediaRule);

 private:
  scoped_refptr<MediaList> media_list_;

  ~CSSMediaRule() OVERRIDE;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_MEDIA_RULE_H_
