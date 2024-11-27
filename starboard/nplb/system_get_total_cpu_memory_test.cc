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

#include "starboard/common/log.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSystemGetTotalCPUMemoryTest, SunnyDay) {
  // We are currently expecting this number to be over 80 megabytes.
  EXPECT_LE(SB_INT64_C(80) * 1024 * 1024, SbSystemGetTotalCPUMemory());
}

TEST(SbSystemGetTotalCPUMemoryTest, PrintValues) {
  int64_t bytes_reserved = SbSystemGetTotalCPUMemory();
  int64_t bytes_in_use = SbSystemGetUsedCPUMemory();

  std::stringstream ss;
  ss << "\n"
     << "SbSystemGetTotalCPUMemory() = " << SbSystemGetTotalCPUMemory() << "\n"
     << "SbSystemGetUsedCPUMemory()  = " << SbSystemGetUsedCPUMemory()
     << "\n\n";
  SbLogRaw(ss.str().c_str());
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
