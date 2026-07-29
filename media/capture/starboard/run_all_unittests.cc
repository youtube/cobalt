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

#include <stdio.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "testing/gtest/include/gtest/gtest.h"

class MojoEnabledTestEnvironment final : public testing::Environment {
 public:
  MojoEnabledTestEnvironment() : mojo_ipc_thread_("MojoIpcThread") {}

  ~MojoEnabledTestEnvironment() final = default;

  void SetUp() final {
    mojo::core::Init();
    mojo_ipc_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    mojo_ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        mojo_ipc_thread_.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);
    VLOG(1) << "Mojo initialized";
  }

  void TearDown() final {
    mojo_ipc_support_.reset();
    VLOG(1) << "Mojo IPC tear down";
  }

 private:
  base::Thread mojo_ipc_thread_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> mojo_ipc_support_;
};

static int InitAndRunAllTests(int argc, char* argv[]) {
  base::TestSuite test_suite(argc, argv);
  testing::AddGlobalTestEnvironment(new MojoEnabledTestEnvironment());
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
