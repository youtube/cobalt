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

#ifndef COBALT_TESTING_BROWSER_TESTS_UTILITY_SHELL_CONTENT_UTILITY_CLIENT_H_
#define COBALT_TESTING_BROWSER_TESTS_UTILITY_SHELL_CONTENT_UTILITY_CLIENT_H_

#include "content/public/test/audio_service_test_helper.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/utility/content_utility_client.h"

namespace content {

class ShellContentUtilityClient : public ContentUtilityClient {
 public:
  explicit ShellContentUtilityClient();

  ShellContentUtilityClient(const ShellContentUtilityClient&) = delete;
  ShellContentUtilityClient& operator=(const ShellContentUtilityClient&) =
      delete;

  ~ShellContentUtilityClient() override;

  // ContentUtilityClient:
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  void RegisterIOThreadServices(mojo::ServiceFactory& services) override;

 private:
  std::unique_ptr<NetworkServiceTestHelper> network_service_test_helper_;
  std::unique_ptr<AudioServiceTestHelper> audio_service_test_helper_;
  bool register_sandbox_status_helper_ = false;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_UTILITY_SHELL_CONTENT_UTILITY_CLIENT_H_
