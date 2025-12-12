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

#ifndef COBALT_TESTING_BROWSER_TESTS_CONTENT_BROWSER_TEST_CONTENT_BROWSER_CLIENT_H_
#define COBALT_TESTING_BROWSER_TESTS_CONTENT_BROWSER_TEST_CONTENT_BROWSER_CLIENT_H_

#include <string_view>

#include "cobalt/testing/browser_tests/browser/shell_content_browser_test_client.h"

namespace content {

// ContentBrowserClient implementation used in content browser tests.
// For tests that need to change the ContentBrowserClient subclass this
// class and it will take care of the registration for you (you don't need
// to call SetBrowserClientForTesting().
class ContentBrowserTestContentBrowserClient
    : public ShellContentBrowserClient {
 public:
  ContentBrowserTestContentBrowserClient();
  ~ContentBrowserTestContentBrowserClient() override;

  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_CONTENT_BROWSER_TEST_CONTENT_BROWSER_CLIENT_H_
