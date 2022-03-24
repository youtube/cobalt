// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEB_ENVIRONMENT_SETTINGS_H_
#define COBALT_WEB_ENVIRONMENT_SETTINGS_H_

#include <memory>
#include <string>

#include "cobalt/base/debugger_hooks.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace web {

class Context;

// Environment Settings object as described in
// https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#environment-settings-objects
class EnvironmentSettings : public script::EnvironmentSettings {
 public:
  EnvironmentSettings() {}
  explicit EnvironmentSettings(const base::DebuggerHooks& debugger_hooks)
      : script::EnvironmentSettings(debugger_hooks) {}
  ~EnvironmentSettings() override {}

  Context* context() const {
    DCHECK(context_);
    return context_;
  }
  void set_context(Context* context) { context_ = context; }

 protected:
  friend std::unique_ptr<EnvironmentSettings>::deleter_type;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnvironmentSettings);

  Context* context_ = nullptr;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_ENVIRONMENT_SETTINGS_H_
