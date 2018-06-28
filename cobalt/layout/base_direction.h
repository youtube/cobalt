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

#ifndef COBALT_LAYOUT_BASE_DIRECTION_H_
#define COBALT_LAYOUT_BASE_DIRECTION_H_

namespace cobalt {
namespace layout {

// BaseDirection is used to indicate the inline direction in which content is
// ordered on a line and defines on which sides "start" and "end" of a line are,
// and also to indicate the default bidirectional orientation of text in a
// paragraph.
// https://www.w3.org/TR/css-writing-modes-3/#inline-base-direction
// http://unicode.org/reports/tr9/#BD5
enum BaseDirection {
  kRightToLeftBaseDirection,
  kLeftToRightBaseDirection,
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_BASE_DIRECTION_H_
