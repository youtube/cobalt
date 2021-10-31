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

#ifndef COBALT_DOM_TESTING_STUB_ENVIRONMENT_SETTINGS_H_
#define COBALT_DOM_TESTING_STUB_ENVIRONMENT_SETTINGS_H_

#include "cobalt/base/debugger_hooks.h"
#include "cobalt/dom/dom_settings.h"

namespace cobalt {
namespace dom {
namespace testing {

class StubEnvironmentSettings : public DOMSettings {
 public:
  explicit StubEnvironmentSettings(const Options& options = Options())
      : DOMSettings(0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                    nullptr, nullptr, null_debugger_hooks_, nullptr, options) {}
  ~StubEnvironmentSettings() override {}

 private:
  base::NullDebuggerHooks null_debugger_hooks_;
};

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_STUB_ENVIRONMENT_SETTINGS_H_
