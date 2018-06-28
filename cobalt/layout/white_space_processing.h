// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_WHITE_SPACE_PROCESSING_H_
#define COBALT_LAYOUT_WHITE_SPACE_PROCESSING_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace layout {

//   https://www.w3.org/TR/css-text-3/#white-space-property
bool DoesCollapseSegmentBreaks(
    const scoped_refptr<cssom::PropertyValue>& value);
bool DoesCollapseWhiteSpace(const scoped_refptr<cssom::PropertyValue>& value);
bool DoesAllowTextWrapping(const scoped_refptr<cssom::PropertyValue>& value);

// Performs white space collapsing and transformation that correspond to
// the phase I of the white space processing.
//   https://www.w3.org/TR/css3-text/#white-space-phase-1
void CollapseWhiteSpace(std::string* text);

bool FindNextNewlineSequence(const std::string& utf8_text, size_t index,
                             size_t* sequence_start, size_t* sequence_length);

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_WHITE_SPACE_PROCESSING_H_
