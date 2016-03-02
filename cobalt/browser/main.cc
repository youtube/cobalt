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

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/browser/application.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  cobalt::InitCobalt(argc, argv);

  MessageLoopForUI message_loop;
  base::PlatformThread::SetName("Main");
  message_loop.set_thread_name("Main");

  DCHECK(!message_loop.is_running());
  base::RunLoop run_loop;

  scoped_ptr<cobalt::browser::Application> application =
      cobalt::browser::CreateApplication();
  application->set_quit_closure(run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
