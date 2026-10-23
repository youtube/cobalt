// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/app/cobalt_main_delegate.h"

#include "base/command_line.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

// Mirrors BasicStartupComplete(): the switch is always present before
// ApplyPreloadGpuShaderDiskCache() runs.
base::CommandLine MakeCommandLine(const char* url) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII("url", url);
  command_line.AppendSwitch(switches::kDisableGpuShaderDiskCache);
  return command_line;
}

constexpr char kPreloadUrl[] = "https://www.youtube.com/tv?launch=preload";
constexpr char kPlainUrl[] = "https://www.youtube.com/tv";

TEST(CobaltMainDelegateTest, PreloadLaunchKeepsGpuShaderDiskCache) {
  base::CommandLine command_line = MakeCommandLine(kPreloadUrl);
  CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line,
                                                     /*is_visible=*/false);
  EXPECT_FALSE(command_line.HasSwitch(switches::kDisableGpuShaderDiskCache));
}

TEST(CobaltMainDelegateTest, VisibleLaunchDisablesGpuShaderDiskCache) {
  base::CommandLine command_line = MakeCommandLine(kPlainUrl);
  CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line,
                                                     /*is_visible=*/true);
  EXPECT_TRUE(command_line.HasSwitch(switches::kDisableGpuShaderDiskCache));
}

TEST(CobaltMainDelegateTest,
     VisibleLaunchWithPreloadParamDisablesGpuShaderDiskCache) {
  // launch=preload on a visible (kSbEventTypeStart) launch is not a preload.
  base::CommandLine command_line = MakeCommandLine(kPreloadUrl);
  CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line,
                                                     /*is_visible=*/true);
  EXPECT_TRUE(command_line.HasSwitch(switches::kDisableGpuShaderDiskCache));
}

TEST(CobaltMainDelegateTest,
     ConcealedLaunchWithoutPreloadParamDisablesGpuShaderDiskCache) {
  base::CommandLine command_line = MakeCommandLine(kPlainUrl);
  CobaltMainDelegate::ApplyPreloadGpuShaderDiskCache(&command_line,
                                                     /*is_visible=*/false);
  EXPECT_TRUE(command_line.HasSwitch(switches::kDisableGpuShaderDiskCache));
}

}  // namespace
}  // namespace cobalt
