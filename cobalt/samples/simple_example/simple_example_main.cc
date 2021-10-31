// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/samples/simple_example/simple_example.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cobalt/base/wrap_main.h"

namespace {

void MultiplyAdd(int m, int a, int b) {
  const cobalt::samples::SimpleExample example(m);
  const int result = example.MultiplyAdd(a, b);

  LOG(INFO) << base::StringPrintf("MultiplyAdd(%d*%d + %d) = %d", m, a, b,
                                  result);
}

void RunCode() {
  MultiplyAdd(1, 1, 3);
  MultiplyAdd(1, 4, -2);
  MultiplyAdd(1, 2, 10);
}

// Argument list should be passed as char**, not char*[] in order to compile
// on Windows. See main_wrapper_win.cc for details.
int SandboxMain(int argc, char** argv) {
  RunCode();
  return 0;
}

}  // namespace

COBALT_WRAP_SIMPLE_MAIN(SandboxMain);
