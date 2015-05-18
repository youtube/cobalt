/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SCRIPT_ENVIRONMENT_SETTINGS_H_
#define SCRIPT_ENVIRONMENT_SETTINGS_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"

namespace cobalt {
namespace script {

// Environment Settings object as described in
// http://www.w3.org/html/wg/drafts/html/master/webappapis.html#environment-settings-object
class EnvironmentSettings {
 public:
  // Currently empty until we have a better idea of what we actually need from
  // this.

 protected:
  friend class scoped_ptr<EnvironmentSettings>;
};

}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_ENVIRONMENT_SETTINGS_H_
