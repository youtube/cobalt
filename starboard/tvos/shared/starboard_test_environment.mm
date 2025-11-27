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

#include "starboard/tvos/shared/starboard_test_environment.h"

#import "starboard/tvos/shared/application_darwin.h"

namespace starboard {

StarboardTestEnvironment::StarboardTestEnvironment(int argc, char** argv)
    : command_line_(argc, argv) {}

StarboardTestEnvironment::~StarboardTestEnvironment() = default;

void StarboardTestEnvironment::SetUp() {
  application_darwin_ = std::make_unique<ApplicationDarwin>(
      std::make_unique<CommandLine>(command_line_));
}

void StarboardTestEnvironment::TearDown() {
  application_darwin_.reset();
}

}  // namespace starboard
