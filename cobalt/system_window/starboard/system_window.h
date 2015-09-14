/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef SYSTEM_WINDOW_STARBOARD_SYSTEM_WINDOW_H_
#define SYSTEM_WINDOW_STARBOARD_SYSTEM_WINDOW_H_

#include "base/compiler_specific.h"
#include "cobalt/system_window/system_window.h"

namespace cobalt {
namespace system_window {

class SystemWindowStarboard : public SystemWindow {
 public:
  SystemWindowStarboard();
  ~SystemWindowStarboard() OVERRIDE;
};

}  // namespace system_window
}  // namespace cobalt

#endif  // SYSTEM_WINDOW_STARBOARD_SYSTEM_WINDOW_H_
