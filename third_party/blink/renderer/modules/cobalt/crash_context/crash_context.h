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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_CRASH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_CRASH_CONTEXT_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CrashContext : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Objects of this class, which has only static members, shouldn't be created.
  CrashContext() = delete;

  static bool setAnnotation(const String& key, const String& value);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_CRASH_CONTEXT_H_
