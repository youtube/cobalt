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
#include "base/threading/thread_checker.h"

namespace cobalt {
namespace browser {

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

 private:
  friend class base::NoDestructor<DeepLinkManager>;

  DeepLinkManager();
  ~DeepLinkManager();

  std::string deep_link_ GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_H5VCC_RUNTIME_DEEP_LINK_MANAGER_H_
