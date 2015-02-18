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

#ifndef DOM_TIME_RANGES_H_
#define DOM_TIME_RANGES_H_

#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The TimeRanges interface is used to describe a series of time ranges. Each of
// them has a start time and an end time.
//   http://www.w3.org/TR/html5/embedded-content-0.html#timeranges
class TimeRanges : public script::Wrappable {
 public:
  // Web API: TimeRanges
  //
  uint32 length() const;
  double Start(uint32 index) const;
  double End(uint32 index) const;

 private:
  std::vector<std::pair<double, double> > ranges_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_TIME_RANGES_H_
