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

#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "starboard/event.h"
#include "starboard/system.h"
#include "third_party/google_benchmark/include/benchmark/benchmark.h"

namespace {
int RunAllBenchmarks(int argc, char** argv) {
  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  return 0;
}
}  // namespace

// When we are building Evergreen we need to export SbEventHandle so that the
// ELF loader can find and invoke it.
#if SB_IS(EVERGREEN)
SB_EXPORT
#endif  // SB_IS(EVERGREEN)
STARBOARD_WRAP_SIMPLE_MAIN(RunAllBenchmarks);
