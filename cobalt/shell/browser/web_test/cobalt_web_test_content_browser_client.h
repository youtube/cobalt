// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SHELL_BROWSER_WEB_TEST_COBALT_WEB_TEST_CONTENT_BROWSER_CLIENT_H_
#define COBALT_SHELL_BROWSER_WEB_TEST_COBALT_WEB_TEST_CONTENT_BROWSER_CLIENT_H_

#include "cobalt/shell/browser/shell_content_browser_client.h"

namespace content {

class CobaltWebTestContentBrowserClient : public ShellContentBrowserClient {
 public:
  CobaltWebTestContentBrowserClient();
  ~CobaltWebTestContentBrowserClient() override;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_WEB_TEST_COBALT_WEB_TEST_CONTENT_BROWSER_CLIENT_H_
