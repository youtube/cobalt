// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEB_TESTING_STUB_ENVIRONMENT_SETTINGS_H_
#define COBALT_WEB_TESTING_STUB_ENVIRONMENT_SETTINGS_H_

#include "cobalt/base/debugger_hooks.h"
#include "cobalt/web/environment_settings.h"

namespace cobalt {
namespace web {
namespace testing {

class StubEnvironmentSettings : public EnvironmentSettings {
 public:
  StubEnvironmentSettings() : EnvironmentSettings(null_debugger_hooks_) {}
  ~StubEnvironmentSettings() override {}

 private:
  base::NullDebuggerHooks null_debugger_hooks_;
};

}  // namespace testing
}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_TESTING_STUB_ENVIRONMENT_SETTINGS_H_
