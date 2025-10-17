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

#ifndef COBALT_TESTING_BROWSER_TESTS_COMMON_SHELL_CONTENT_TEST_CLIENT_H_
#define COBALT_TESTING_BROWSER_TESTS_COMMON_SHELL_CONTENT_TEST_CLIENT_H_

#include "cobalt/shell/common/shell_content_client.h"

namespace content {

class ShellContentTestClient : public ShellContentClient {
 public:
  ShellContentTestClient();
  ~ShellContentTestClient() override;

  std::u16string GetLocalizedString(int message_id) override;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_COMMON_SHELL_CONTENT_TEST_CLIENT_H_
