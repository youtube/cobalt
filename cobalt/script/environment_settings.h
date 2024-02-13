// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_ENVIRONMENT_SETTINGS_H_
#define COBALT_SCRIPT_ENVIRONMENT_SETTINGS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/loader/origin.h"
#include "url/gurl.h"
namespace cobalt {
namespace script {

// Environment Settings object as described in
// https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#environment-settings-objects
class EnvironmentSettings {
 public:
  explicit EnvironmentSettings(
      const base::DebuggerHooks& debugger_hooks = null_debugger_hooks_);
  virtual ~EnvironmentSettings() {}

  // The API id.
  //   https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-id
  const std::string& id() const { return uuid_; }

  // The API base URL.
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#api-base-url
  virtual const GURL& base_url() const { return creation_url(); }

  // The API creation URL
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#concept-environment-creation-url
  void set_creation_url(const GURL& url) { creation_url_ = url; }
  const GURL& creation_url() const { return creation_url_; }

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-settings-object-origin
  // TODO(b/244368134): Replace with url::Origin.
  virtual loader::Origin GetOrigin() const {
    return loader::Origin(base_url().DeprecatedGetOriginAsURL());
  }

  const base::DebuggerHooks& debugger_hooks() const { return debugger_hooks_; }

 protected:
  friend std::unique_ptr<EnvironmentSettings>::deleter_type;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnvironmentSettings);

  std::string uuid_;
  static const base::NullDebuggerHooks null_debugger_hooks_;
  const base::DebuggerHooks& debugger_hooks_;
  GURL creation_url_;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_ENVIRONMENT_SETTINGS_H_
