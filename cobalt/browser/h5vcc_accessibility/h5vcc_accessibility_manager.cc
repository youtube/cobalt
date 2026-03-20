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

#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_manager.h"

#include "base/check.h"
#include "base/no_destructor.h"

namespace cobalt {
namespace browser {

H5vccAccessibilityManager::H5vccAccessibilityManager() {
  DETACH_FROM_THREAD(thread_checker_);
}

H5vccAccessibilityManager::~H5vccAccessibilityManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
H5vccAccessibilityManager* H5vccAccessibilityManager::GetInstance() {
  static base::NoDestructor<H5vccAccessibilityManager> instance;
  return instance.get();
}

void H5vccAccessibilityManager::AddListener(
    mojo::PendingRemote<h5vcc_accessibility::mojom::H5vccAccessibilityClient>
        listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  listeners_.Add(std::move(listener));
}

void H5vccAccessibilityManager::OnTextToSpeechStateChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& listener : listeners_) {
    listener->NotifyTextToSpeechChange(enabled);
  }
}

}  // namespace browser
}  // namespace cobalt
