// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "build/build_config.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "starboard/event.h"
#include "starboard/system.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace {
int RunAllBenchmarks(int argc, char** argv) {
  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  return 0;
}
}  // namespace

// When we are building Evergreen we need to export SbEventHandle so that the
// ELF loader can find and invoke it.
#if SB_IS(MODULAR)
SB_EXPORT
#endif  // SB_IS(MODULAR)
STARBOARD_WRAP_SIMPLE_MAIN(RunAllBenchmarks)

// This is how to build and run this benchmark.
//
// On Linux:
// $ autoninja -C out/linux-x64x11_qa starboard/benchmark:benchmark
// $ ./out/linux-x64x11_qa/benchmark --benchmark_filter="BM_*"
//
// On Android:
// $ autoninja -C out/android-arm_qa starboard/benchmark:benchmark
// $ adb push out/android-arm_qa/benchmark /data/local/tmp/
// $ adb shell chmod +x /data/local/tmp/benchmark
// $ adb shell /data/local/tmp/benchmark --benchmark_filter="BM_*"
//

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
#if BUILDFLAG(IS_STARBOARD)
  return SbRunStarboardMain(argc, argv, SbEventHandle);
#else
  return RunAllBenchmarks(argc, argv);
#endif  // BUILDFLAG(IS_STARBOARD)
}
#endif  // !SB_IS(EVERGREEN)
