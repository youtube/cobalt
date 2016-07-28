/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "cobalt/deprecated/platform_delegate.h"

#include <stdlib.h>
#include <windows.h>

#include <string>

#include "base/compiler_specific.h"

namespace cobalt {
namespace deprecated {
namespace {

class PlatformDelegateWin : public PlatformDelegate {
 public:
  PlatformDelegateWin();
  ~PlatformDelegateWin() OVERRIDE;
  std::string GetPlatformName() const OVERRIDE { return "Windows"; }
};

PlatformDelegateWin::PlatformDelegateWin() {
  char path_buffer[MAX_PATH];

  // Get the directory of the executable.
  HMODULE hModule = GetModuleHandle(NULL);
  GetModuleFileNameA(hModule, path_buffer, (sizeof(path_buffer)));
  std::string exe_path = path_buffer;
  unsigned last_bs = exe_path.rfind('\\');
  std::string exe_dir = exe_path.substr(0, last_bs);

  game_content_path_ = exe_dir + "\\content\\data";
  dir_source_root_ = exe_dir + "\\content\\dir_source_root";

  // Set screenshot_output_path.
  screenshot_output_path_ = exe_dir + "\\content\\logs";

  // Set logging_output_path.
  logging_output_path_ = exe_dir + "\\content\\logs";

  // Set tmp_path.
  GetTempPathA(MAX_PATH, path_buffer);
  temp_path_ = path_buffer;
}

PlatformDelegateWin::~PlatformDelegateWin() {}

}  // namespace

PlatformDelegate* PlatformDelegate::Create() { return new PlatformDelegateWin; }

}  // namespace deprecated
}  // namespace cobalt
