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
// limitations under the License.`

#ifndef COBALT_LAYOUT_LINE_WRAPPING_H_
#define COBALT_LAYOUT_LINE_WRAPPING_H_

namespace cobalt {
namespace layout {

enum WrapAtPolicy {
  kWrapAtPolicyBefore,
  kWrapAtPolicyLastOpportunityWithinWidth,
  kWrapAtPolicyLastOpportunity,
  kWrapAtPolicyFirstOpportunity,
};

// Wrapping is only performed at an allowed break point, called a soft wrap
// opportunity.
//   https://www.w3.org/TR/css-text-3/#line-breaking
// 'normal': Lines may break only at allowed break points.
// 'break-word': An unbreakable 'word' may be broken at an an arbitrary point...
//   https://www.w3.org/TR/css-text-3/#overflow-wrap
enum WrapOpportunityPolicy {
  kWrapOpportunityPolicyNormal,
  kWrapOpportunityPolicyBreakWord,
  kWrapOpportunityPolicyBreakWordOrNormal,
};

enum WrapResult {
  kWrapResultNoWrap,
  kWrapResultWrapBefore,
  kWrapResultSplitWrap,
};

bool ShouldProcessWrapOpportunityPolicy(
    WrapOpportunityPolicy wrap_opportunity_policy,
    bool does_style_allow_break_word);

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LINE_WRAPPING_H_
