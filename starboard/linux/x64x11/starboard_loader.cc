// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include <dlfcn.h>

#include <iostream>

#include "starboard/event.h"

int main(int argc, char** argv) {
  static const char* s_target_lib_path = "lib" SB_LOADER_MODULE ".so";
  int start_result;

  void* handle_ = dlopen(s_target_lib_path, RTLD_LAZY);
  if (!handle_) {
    std::cerr << "dlopen failure: " << dlerror() << std::endl;
  }

  void* callback = nullptr;
  callback = dlsym(handle_, "SbEventHandle");
  if (!callback) {
    std::cerr << "dlsym failure: " << dlerror() << std::endl;
  }
  return SbRunStarboardMain(argc, argv,
                            reinterpret_cast<SbEventHandleCallback>(callback));
}
