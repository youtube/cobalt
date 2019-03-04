// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_COMPUTED_STYLE_UTILS_H_
#define COBALT_CSSOM_COMPUTED_STYLE_UTILS_H_

#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/keyword_value.h"

namespace cobalt {
namespace cssom {

// Returns true if "overflow" should be treated as cropped. This is true for
// overflow "auto", "hidden", and "scroll".
//   https://www.w3.org/TR/CSS21/visufx.html#overflow
inline bool IsOverflowCropped(
    const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style) {
  return computed_style->overflow() == cssom::KeywordValue::GetAuto() ||
         computed_style->overflow() == cssom::KeywordValue::GetHidden() ||
         computed_style->overflow() == cssom::KeywordValue::GetScroll();
}

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_COMPUTED_STYLE_UTILS_H_
