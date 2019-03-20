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

#include "cobalt/css_parser/flex_shorthand_property_parse_structures.h"

#include "cobalt/cssom/keyword_value.h"

namespace cobalt {
namespace css_parser {

void FlexShorthand::ReplaceNullWithInitialValues() {
  if (!grow) {
    grow = cssom::KeywordValue::GetInitial();
  }

  if (!shrink) {
    shrink = cssom::KeywordValue::GetInitial();
  }

  if (!basis) {
    basis = cssom::KeywordValue::GetInitial();
  }
}

void FlexFlowShorthand::ReplaceNullWithInitialValues() {
  if (!direction) {
    direction = cssom::KeywordValue::GetInitial();
  }

  if (!wrap) {
    wrap = cssom::KeywordValue::GetInitial();
  }
}

}  // namespace css_parser
}  // namespace cobalt
