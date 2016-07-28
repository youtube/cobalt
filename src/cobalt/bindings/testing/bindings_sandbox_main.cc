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

#include <iostream>
#include <string>

#include "cobalt/base/wrap_main.h"
#include "cobalt/bindings/testing/window.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/source_code.h"

#include "cobalt/script/standalone_javascript_runner.h"

using cobalt::bindings::testing::Window;
using cobalt::script::StandaloneJavascriptRunner;

namespace {

int SandboxMain(int argc, char** argv) {
  scoped_refptr<Window> test_window = new Window();
  StandaloneJavascriptRunner standalone_runner(test_window);
  standalone_runner.RunInteractive();
  return 0;
}

}  // namespace

COBALT_WRAP_SIMPLE_MAIN(SandboxMain);
