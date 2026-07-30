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
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

namespace {

class GpuTestSuite : public base::TestSuite {
 public:
  GpuTestSuite(int argc, char** argv);

  GpuTestSuite(const GpuTestSuite&) = delete;
  GpuTestSuite& operator=(const GpuTestSuite&) = delete;

  ~GpuTestSuite() override;
};

GpuTestSuite::GpuTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

GpuTestSuite::~GpuTestSuite() = default;

}  // namespace

static int InitAndRunAllTests(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  GpuTestSuite test_suite(argc, argv);

  mojo::core::Init();

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
