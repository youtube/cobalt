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
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/perf_test_suite.h"
#include "mojo/core/test/scoped_mojo_support.h"
#include "mojo/core/test/test_support_impl.h"
#include "mojo/public/tests/test_support_private.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

static int InitAndRunAllTests(int argc, char** argv) {
  CHECK(base::CommandLine::Init(argc, argv));
  const auto& cmd = *base::CommandLine::ForCurrentProcess();
  auto features = std::make_unique<base::FeatureList>();
  features->InitFromCommandLine(
      cmd.GetSwitchValueASCII(switches::kEnableFeatures),
      cmd.GetSwitchValueASCII(switches::kDisableFeatures));
  base::FeatureList::SetInstance(std::move(features));

  base::PerfTestSuite test(argc, argv);
  mojo::core::test::ScopedMojoSupport mojo_support;
  mojo::test::TestSupport::Init(new mojo::core::test::TestSupportImpl());
  return test.Run();
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
