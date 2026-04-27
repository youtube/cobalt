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

#ifndef COBALT_SHELL_BROWSER_WEB_TEST_COBALT_WEB_TEST_BROWSER_MAIN_RUNNER_H_
#define COBALT_SHELL_BROWSER_WEB_TEST_COBALT_WEB_TEST_BROWSER_MAIN_RUNNER_H_

#include "content/public/common/main_function_params.h"

namespace content {

class CobaltWebTestBrowserMainRunner {
 public:
  CobaltWebTestBrowserMainRunner();
  ~CobaltWebTestBrowserMainRunner();

  void Initialize();
  void RunBrowserMain(content::MainFunctionParams parameters);
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_WEB_TEST_COBALT_WEB_TEST_BROWSER_MAIN_RUNNER_H_
