// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/locale/update_default_locale_manager.h"

#include "base/check.h"
#include "base/no_destructor.h"

namespace cobalt {
namespace browser {

UpdateDefaultLocaleManager::UpdateDefaultLocaleManager() = default;

UpdateDefaultLocaleManager::~UpdateDefaultLocaleManager() = default;

// static
UpdateDefaultLocaleManager* UpdateDefaultLocaleManager::GetInstance() {
  static base::NoDestructor<UpdateDefaultLocaleManager> provider;
  return provider.get();
}

void UpdateDefaultLocaleManager::Update() {
  if (!update_closure_) {
    return;
  }
  update_closure_.Run();
}

void UpdateDefaultLocaleManager::set_update(base::RepeatingClosure closure) {
  update_closure_ = closure;
}

}  // namespace browser
}  // namespace cobalt
