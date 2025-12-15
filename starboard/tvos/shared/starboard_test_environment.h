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

#ifndef STARBOARD_TVOS_SHARED_STARBOARD_TEST_ENVIRONMENT_H_
#define STARBOARD_TVOS_SHARED_STARBOARD_TEST_ENVIRONMENT_H_

#include <memory>

#include "starboard/common/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {

class QueueApplication; // Forward declare QueueApplication

class StarboardTestEnvironment final : public ::testing::Environment {
 public:
  StarboardTestEnvironment(int argc, char** argv);
  ~StarboardTestEnvironment();

  void SetUp() final;
  void TearDown() final;

 private:
  std::unique_ptr<QueueApplication> application_; // Changed type and name
  CommandLine command_line_;
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_STARBOARD_TEST_ENVIRONMENT_H_
