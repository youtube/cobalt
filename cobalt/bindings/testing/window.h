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

#ifndef COBALT_BINDINGS_TESTING_WINDOW_H_
#define COBALT_BINDINGS_TESTING_WINDOW_H_

#include <string>
#include <vector>

#include "cobalt/bindings/testing/global_interface_parent.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"

namespace cobalt {
namespace bindings {
namespace testing {

class Window : public GlobalInterfaceParent {
 public:
  virtual void WindowOperation() {}
  virtual std::string window_property() { return ""; }
  virtual void set_window_property(const std::string&) {}

  std::string GetStackTrace(
      const std::vector<script::StackFrame>& stack_frame) {
    return StackTraceToString(stack_frame);
  }
  scoped_refptr<Window> window() { return this; }

  DEFINE_WRAPPABLE_TYPE(Window);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_WINDOW_H_
