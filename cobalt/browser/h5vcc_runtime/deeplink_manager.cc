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

#include "cobalt/browser/h5vcc_runtime/deeplink_manager.h"

#include "base/check.h"
#include "base/no_destructor.h"

namespace cobalt {
namespace browser {


DeepLinkManager::DeepLinkManager() = default;

DeepLinkManager::~DeepLinkManager() = default;

// static
DeepLinkManager* DeepLinkManager::GetInstance() {
  static base::NoDestructor<DeepLinkManager> provider;
  return provider.get();
}

void DeepLinkManager::SetDeepLink(const std::string& deeplink) {
  deeplink_ = deeplink;
}

const std::string& DeepLinkManager::GetDeepLink() const {
  return deeplink_;
}

void DeepLinkManager::AddListener(mojo::Remote<h5vcc_runtime::mojom::DeepLinkListener> listener_remote) {
  LOG(INFO) << "ColinL: DeepLinkManager::AddListener.";
  // mojo::RemoteSet removes the listener_remote automatically when the Mojo client disconnects
  return listeners_.Add(std::move(listener_remote));
}

void DeepLinkManager::OnWarmStartupDeepLink(const std::string& deeplink) {
    // reillyg@ said 
    // most classes are implemented as only accessible by a single thread. 
    // So in this case all access to the mojo::RemoteSet happens on one thread 
    // so the removal of an entry would not happen until you were done iterating over it.
    // Utilities such as SEQUENCE_CHECKER allow you to assert that an object is only accessed 
    // by a single thread (sequence technically). You'll find that inside the implementation of many Mojo classes.

    // So we do not need to consider the race condition here that when the looper is reading the iterator, 
    // and the mojom client is disconnected and the listener is removed from the collection.

    LOG(INFO) << "ColinL: DeepLinkManager::OnWarmStartupDeepLink.";
    if (listeners_.empty()) {
      LOG(INFO) << "ColinL: DeepLinkManager::OnWarmStartupDeepLink. -- listeners_.empty()";
      // No callback registered, store it.
      SetDeepLink(deeplink);
    } else {
      for (auto& listener : listeners_) {
        LOG(INFO) << "ColinL: DeepLinkManager::OnWarmStartupDeepLink. -- listener->OnDeepLink();";
        listener->OnDeepLink(deeplink);
      }
    }
}

}  // namespace browser
}  // namespace cobalt