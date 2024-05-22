// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/test/test_suite.h"
#include "base/threading/platform_thread.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/path_provider.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
int InitAndRunAllTests(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;
  base::TestSuite test_suite(argc, argv);
  base::PathService::RegisterProvider(&cobalt::PathProvider,
                                      cobalt::paths::PATH_COBALT_START,
                                      cobalt::paths::PATH_COBALT_END);

  // Copy the Starboard thread name to the PlatformThread name.
  char thread_name[128] = {'\0'};
  pthread_getname_np(pthread_self(), thread_name, 127);

  base::PlatformThread::SetName(thread_name);
  return test_suite.Run();
}
}  // namespace

STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests);
