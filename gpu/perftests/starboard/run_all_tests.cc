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

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

static int RunHelper(base::TestSuite* test_suite) {
  base::FeatureList::InitInstance(std::string(), std::string());
  std::unique_ptr<base::SingleThreadTaskExecutor> executor;
#if BUILDFLAG(IS_OZONE)
  executor = std::make_unique<base::SingleThreadTaskExecutor>(
      base::MessagePumpType::UI);
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#else
  executor = std::make_unique<base::SingleThreadTaskExecutor>(
      base::MessagePumpType::IO);
#endif

  CHECK(gl::init::InitializeGLOneOff(
      /*gpu_preference=*/gl::GpuPreference::kDefault));
  return test_suite->Run();
}

static int InitAndRunAllTests(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  base::CommandLine::Init(argc, argv);

  // Always run the perf tests serially, to avoid distorting
  // perf measurements with randomness resulting from running
  // in parallel.
  return base::LaunchUnitTestsSerially(
      argc, argv, base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
