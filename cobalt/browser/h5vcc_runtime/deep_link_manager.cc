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

#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"

#include "base/check.h"
#include "base/no_destructor.h"

namespace cobalt {
namespace browser {

DeepLinkManager::DeepLinkManager() {
  COBALT_DETACH_FROM_THREAD(thread_checker_);
}

DeepLinkManager::~DeepLinkManager() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
DeepLinkManager* DeepLinkManager::GetInstance() {
  static base::NoDestructor<DeepLinkManager> provider;
  return provider.get();
}

void DeepLinkManager::set_deep_link(const std::string& url) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  deep_link_ = url;
}
const std::string& DeepLinkManager::get_deep_link() const {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return deep_link_;
}

const std::string DeepLinkManager::GetAndClearDeepLink() {
  const std::string value = get_deep_link();
  if (!value.empty()) {
    set_deep_link("");
  }
  return value;
}

void DeepLinkManager::AddListener(
    mojo::Remote<DeepLinkListener> listener_remote) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (listeners_.empty() && !deep_link_.empty()) {
    listener_remote->NotifyDeepLink(GetAndClearDeepLink());
  }

  // mojo::RemoteSet removes the listener_remote automatically when the Mojo
  // client disconnects
  listeners_.Add(std::move(listener_remote));
}

void DeepLinkManager::OnDeepLink(const std::string& url) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (listeners_.empty()) {
    // Deeplink is held until a callback is registered, at which point it will
    // be consumed."
    set_deep_link(url);
  } else {
    // No need to worry about race condition because all access to the
    // mojo::RemoteSet happens on one thread.
    for (auto& listener : listeners_) {
      listener->NotifyDeepLink(url);
    }
  }
}

}  // namespace browser
}  // namespace cobalt
