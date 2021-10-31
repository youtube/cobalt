// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/crash_handler.h"

#include "cobalt/extension/crash_handler.h"
#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "third_party/crashpad/wrapper/wrapper.h"

namespace starboard {
namespace common {

namespace {

bool OverrideCrashpadAnnotations(CrashpadAnnotations* crashpad_annotations) {
  CrashpadAnnotations annotations;
  memset(&annotations, 0, sizeof(CrashpadAnnotations));
  memcpy(&annotations, crashpad_annotations, sizeof(CrashpadAnnotations));
  return third_party::crashpad::wrapper::AddAnnotationsToCrashpad(annotations);
}

const CobaltExtensionCrashHandlerApi kCrashHandlerApi = {
    kCobaltExtensionCrashHandlerName, 1, &OverrideCrashpadAnnotations,
};

}  // namespace

const void* GetCrashHandlerApi() {
  return &kCrashHandlerApi;
}

}  // namespace common
}  // namespace starboard
