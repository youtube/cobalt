// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "cobalt/browser/crash_annotator/crash_annotator_impl.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/system.h"

namespace crash_annotator {

void CrashAnnotatorImpl::SetString(const std::string& key,
                                   const std::string& value,
                                   SetStringCallback callback) {
  auto crash_handler_extension =
      static_cast<const CobaltExtensionCrashHandlerApi*>(
          SbSystemGetExtension(kCobaltExtensionCrashHandlerName));
  if (crash_handler_extension && crash_handler_extension->version >= 2) {
    std::move(callback).Run(
        crash_handler_extension->SetString(key.c_str(), value.c_str()));
  } else {
    // This method can only be supported if the platform implements version 2,
    // or greater, of this Starboard Extension.
    std::move(callback).Run(false);
  }
}

}  // namespace crash_annotator
