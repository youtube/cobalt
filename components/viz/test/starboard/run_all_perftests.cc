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

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/viz_test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "skia/ext/event_tracer_impl.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

static int InitAndRunAllTests(int argc, char** argv) {
  viz::VizTestSuite test_suite(argc, argv);

  mojo::core::Init();

  InitSkiaEventTracer();

  viz::TestGpuServiceHolder::DoNotResetOnTestExit();

  // Always run the perf tests serially, to avoid distorting
  // perf measurements with randomness resulting from running
  // in parallel.
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&viz::VizTestSuite::Run, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
