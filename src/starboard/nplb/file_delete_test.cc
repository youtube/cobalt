// Copyright 2015 Google Inc. All Rights Reserved.
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

// SbFileDelete is otherwise tested in all the other unit tests.

#include <string>

#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbFileDeleteTest, NonExistentFileErrors) {
  std::string filename = starboard::nplb::GetRandomFilename();
  EXPECT_FALSE(SbFileExists(filename.c_str()));

  bool result = SbFileDelete(filename.c_str());
  EXPECT_FALSE(result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
