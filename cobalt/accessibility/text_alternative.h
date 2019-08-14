// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_ACCESSIBILITY_TEXT_ALTERNATIVE_H_
#define COBALT_ACCESSIBILITY_TEXT_ALTERNATIVE_H_

#include <string>

#include "cobalt/dom/node.h"

namespace cobalt {
namespace accessibility {

// Compute the Text Alternative for the node per the wai-aria specification:
// https://www.w3.org/TR/2014/REC-wai-aria-implementation-20140320/#mapping_additional_nd_te
std::string ComputeTextAlternative(const scoped_refptr<dom::Node>& node);

}  // namespace accessibility
}  // namespace cobalt

#endif  // COBALT_ACCESSIBILITY_TEXT_ALTERNATIVE_H_
