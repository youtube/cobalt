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

#ifndef COBALT_BROWSER_H5VCC_RUNTIME_DEEP_LINK_MANAGER_H_
#define COBALT_BROWSER_H5VCC_RUNTIME_DEEP_LINK_MANAGER_H_

#include <string>

#include "base/no_destructor.h"
#include "cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom.h"
#include "cobalt/common/cobalt_thread_checker.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace cobalt {
namespace browser {

using h5vcc_runtime::mojom::DeepLinkListener;

//  This class is a singleton that provides a central point for handling the
//  deep link URLs received from the OS.  It handles both initial deep links
//  (available at app startup) and warm start deep links (received while the app
//  is already running). It ensures that any deep link is delivered exactly once
//  to the JavaScript side. This class also manages a set of
//  mojom::DeepLinkListener, and forwards the deep link message to them.
//  DeepLinkManager is a singleton, and its instance is created on first access
//  via `GetInstance()`.  It is *never* destroyed (using `base::NoDestructor`).
//  DeepLinkManager is intended to be used on a single thread.
class DeepLinkManager {
 public:
  // Get the singleton instance.
  static DeepLinkManager* GetInstance();

  DeepLinkManager(const DeepLinkManager&) = delete;
  DeepLinkManager& operator=(const DeepLinkManager&) = delete;

  void set_deep_link(const std::string& url);
  const std::string& get_deep_link() const;

  // To ensure exactly-once delivery, the deeplink is erased after delivery to
  // the application.
  const std::string GetAndClearDeepLink();

  void AddListener(mojo::Remote<DeepLinkListener> listener_remote);
  void OnDeepLink(const std::string& url);

 private:
  friend class base::NoDestructor<DeepLinkManager>;

  DeepLinkManager();
  ~DeepLinkManager();

  std::string deep_link_;
  mojo::RemoteSet<DeepLinkListener> listeners_;

  COBALT_THREAD_CHECKER(thread_checker_);
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_H5VCC_RUNTIME_DEEP_LINK_MANAGER_H_
