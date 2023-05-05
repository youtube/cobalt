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
#include <stdio.h>
#include <time.h>

#include <string>

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/event.h"

int main(int argc, char** argv) {
  const std::string s_libraryPath1 =
      std::string(OUT_DIRECTORY) + "/starboard/libstarboard_platform_group.so";
  const std::string s_libraryPath2 =
      std::string(OUT_DIRECTORY) + "/lib" + SB_LOADER_MODULE + ".so";
  int startResult;
  printf("Loader: %s\n", __FILE__);
  printf("Loading: %s\n", s_libraryPath1.c_str());
  void* handle1 = dlopen(s_libraryPath1.c_str(), RTLD_NOW);
  if (!handle1) {
    printf("dlopen failure: %s", dlerror());
    return 1;
  }

  printf("Loading: %s\n", s_libraryPath2.c_str());
  void* handle2 = dlopen(s_libraryPath2.c_str(), RTLD_NOW);
  if (!handle2) {
    printf("dlopen failure: %s", dlerror());
    return 1;
  }

  void* callback = nullptr;
  callback = dlsym(handle2, "SbEventHandle");
  if (!callback) {
    printf("dlsym failure: %s", dlerror());
  }

  auto result = SbRunStarboardMain(
      argc, argv, reinterpret_cast<SbEventHandleCallback>(callback));

  dlclose(handle1);
  dlclose(handle2);
  return result;
}
