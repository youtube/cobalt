// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "third_party/ipcz/src/test/multinode_test.h"
#include "third_party/ipcz/src/test_buildflags.h"

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
#include "third_party/ipcz/src/test/test_child_launcher.h"
#endif

static int InitAndRunAllTests(int argc, char** argv) {
#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
  ipcz::test::TestChildLauncher::Initialize(argc, argv);
#endif

  base::TestSuite test_suite(argc, argv);
  ipcz::test::RegisterMultinodeTests();

  mojo::core::Init(mojo::core::Configuration{
      .is_broker_process = !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestChildProcess),
  });
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  mojo::core::ScopedIPCSupport ipc_support(
      test_io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
