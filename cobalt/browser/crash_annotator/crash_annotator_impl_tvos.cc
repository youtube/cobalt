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
#include "cobalt/browser/cobalt_crash_annotations.h"
#include "cobalt/browser/crash_annotator/crash_annotator_impl.h"

namespace crash_annotator {

void CrashAnnotatorImpl::SetString(const std::string& key,
                                   const std::string& value,
                                   SetStringCallback callback) {
  cobalt::browser::CobaltCrashAnnotations::GetInstance()->SetAnnotation(key,
                                                                        value);
  std::move(callback).Run(true);
}

}  // namespace crash_annotator
