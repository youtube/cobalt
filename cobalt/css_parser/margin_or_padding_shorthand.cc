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

#include <memory>

#include "cobalt/css_parser/margin_or_padding_shorthand.h"

namespace cobalt {
namespace css_parser {

std::unique_ptr<MarginOrPaddingShorthand> MarginOrPaddingShorthand::TryCreate(
    const scoped_refptr<cssom::PropertyValue>& top,
    const scoped_refptr<cssom::PropertyValue>& right,
    const scoped_refptr<cssom::PropertyValue>& bottom,
    const scoped_refptr<cssom::PropertyValue>& left) {
  // User agents must ignore a declaration with an illegal value.
  //   https://www.w3.org/TR/CSS21/syndata.html#illegalvalues
  if (top.get() == NULL || right.get() == NULL || bottom.get() == NULL ||
      left.get() == NULL) {
    return std::unique_ptr<MarginOrPaddingShorthand>();
  }

  std::unique_ptr<MarginOrPaddingShorthand> shorthand(
      new MarginOrPaddingShorthand());
  shorthand->top = top;
  shorthand->right = right;
  shorthand->bottom = bottom;
  shorthand->left = left;
  return shorthand;
}

}  // namespace css_parser
}  // namespace cobalt
