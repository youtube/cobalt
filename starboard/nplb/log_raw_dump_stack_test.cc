// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// This test doesn't test that the appropriate output was produced, but ensures
// it compiles and runs without crashing.

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

SB_C_NOINLINE void DumpMoreStack() {
  SbLogRawDumpStack(0);
}

SB_C_NOINLINE void DumpEvenMoreStack() {
  DumpMoreStack();
}

SB_C_NOINLINE void DumpStackBigTime() {
  DumpEvenMoreStack();
}

SB_C_NOINLINE void IShouldBeSkipped() {
  SbLogRawDumpStack(1);
}

TEST(SbLogRawDumpStackTest, SunnyDay) {
  SbLogRawDumpStack(0);
}

TEST(SbLogRawDumpStackTest, SunnyDayDeeperStack) {
  DumpStackBigTime();
}

TEST(SbLogRawDumpStackTest, SunnyDaySkip) {
  IShouldBeSkipped();
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
