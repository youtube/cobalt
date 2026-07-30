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

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "gpu/command_buffer/tests/gl_test_setup_helper.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

class GlTestsSuite : public base::TestSuite {
 public:
  GlTestsSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

    gl_setup_ = std::make_unique<gpu::GLTestSetupHelper>();
  }

 private:
  std::unique_ptr<gpu::GLTestSetupHelper> gl_setup_;
};

}  // namespace

static int InitAndRunAllTests(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  mojo::core::Init();

  GlTestsSuite gl_tests_suite(argc, argv);
#if BUILDFLAG(IS_MAC)
  base::mac::ScopedNSAutoreleasePool pool;
#endif
  testing::InitGoogleMock(&argc, argv);
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&GlTestsSuite::Run, base::Unretained(&gl_tests_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
