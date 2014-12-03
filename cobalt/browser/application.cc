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

#include "base/run_loop.h"
#include "cobalt/browser/application.h"

namespace cobalt {
namespace browser {

Application::Application() : ui_message_loop_(MessageLoop::TYPE_UI) {}

Application::~Application() { DCHECK(!ui_message_loop_.is_running()); }

void Application::Quit() {
  if (!quit_closure_.is_null()) {
    quit_closure_.Run();
  }
}

void Application::Run() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace browser
}  // namespace cobalt
