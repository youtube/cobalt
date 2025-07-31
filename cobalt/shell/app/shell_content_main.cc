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

#include "cobalt/shell/app/shell_content_main.h"

#include "build/build_config.h"
#include "cobalt/shell/app/shell_main_delegate.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_MAC)
int ContentMain(int argc, const char** argv) {
  bool is_browsertest = false;
  std::string browser_test_flag(std::string("--") + switches::kBrowserTest);
  for (int i = 0; i < argc; ++i) {
    if (browser_test_flag == argv[i]) {
      is_browsertest = true;
      break;
    }
  }
  content::ShellMainDelegate delegate(is_browsertest);
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(std::move(params));
}
#endif  // BUILDFLAG(IS_MAC)
