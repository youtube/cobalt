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
// limitations under the License.

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "cobalt/cssom/time_list_value.h"

namespace cobalt {
namespace cssom {

std::string TimeListValue::ToString() const {
  std::string result;
  for (size_t i = 0; i < value().size(); ++i) {
    if (!result.empty()) result.append(", ");
    int64 in_ms = value()[i].InMilliseconds();
    int64 truncated_to_seconds = in_ms / base::Time::kMillisecondsPerSecond;
    if (in_ms == base::Time::kMillisecondsPerSecond * truncated_to_seconds) {
      result.append(base::StringPrintf("%" PRIu64 "s", truncated_to_seconds));
    } else {
      result.append(base::StringPrintf("%" PRIu64 "ms", in_ms));
    }
  }
  return result;
}

}  // namespace cssom
}  // namespace cobalt
