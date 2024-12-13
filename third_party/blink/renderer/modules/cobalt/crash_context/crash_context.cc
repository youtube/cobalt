// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/crash_context/crash_context.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

bool CrashContext::setAnnotation(const String& key, const String& value) {
  // TODO(cobalt, b/383301493): actually implement this.

  LOG(INFO) << "setAnnotation: key=" << key.EncodeForDebugging() << " value="
            << value.EncodeForDebugging();
  return false;
}

}  // namespace blink
