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

#include <iostream>
#include <string>

#include "cobalt/base/wrap_main.h"
#include "cobalt/bindings/testing/window.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/standalone_javascript_runner.h"

using cobalt::bindings::testing::Window;
using cobalt::script::StandaloneJavascriptRunner;

namespace {

cobalt::script::StandaloneJavascriptRunner* g_javascript_runner = NULL;

void StartApplication(int argc, char** argv, const char* link,
                      const base::Closure& quit_closure,
                      SbTimeMonotonic timestamp) {
  scoped_refptr<Window> test_window = new Window();
  cobalt::script::JavaScriptEngine::Options javascript_engine_options;

  DCHECK(!g_javascript_runner);
  g_javascript_runner = new cobalt::script::StandaloneJavascriptRunner(
      base::MessageLoop::current()->task_runner(), javascript_engine_options,
      test_window);
  DCHECK(g_javascript_runner);
  g_javascript_runner->RunUntilDone(quit_closure);
}

void StopApplication() {
  DCHECK(g_javascript_runner);
  delete g_javascript_runner;
  g_javascript_runner = NULL;
}

}  // namespace

COBALT_WRAP_BASE_MAIN(StartApplication, StopApplication);
