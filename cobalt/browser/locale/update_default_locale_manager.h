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

#ifndef COBALT_BROWSER_LOCALE_UPDATE_DEFAULT_LOCALE_MANAGER_H_
#define COBALT_BROWSER_LOCALE_UPDATE_DEFAULT_LOCALE_MANAGER_H_

#include "base/functional/callback.h"
#include "base/no_destructor.h"

namespace cobalt {
namespace browser {

class UpdateDefaultLocaleManager {
 public:
  // Get the singleton instance.
  static UpdateDefaultLocaleManager* GetInstance();

  UpdateDefaultLocaleManager(const UpdateDefaultLocaleManager&) = delete;
  UpdateDefaultLocaleManager& operator=(const UpdateDefaultLocaleManager&) =
      delete;

  void Update();
  void set_update(base::RepeatingClosure closure);

 private:
  friend class base::NoDestructor<UpdateDefaultLocaleManager>;

  UpdateDefaultLocaleManager();
  ~UpdateDefaultLocaleManager();

  base::RepeatingClosure update_closure_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_LOCALE_UPDATE_DEFAULT_LOCALE_MANAGER_H_
