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
  const std::string starboard_library_path =
      std::string(OUT_DIRECTORY) + "/starboard/libstarboard_platform_group.so";
  const std::string target_library_path =
      std::string(OUT_DIRECTORY) + "/lib" + SB_LOADER_MODULE + ".so";

  printf("Loader: %s\n", __FILE__);
  printf("Loading: %s\n", starboard_library_path.c_str());
  void* starboard_library = dlopen(starboard_library_path.c_str(), RTLD_NOW);
  if (!starboard_library) {
    printf("dlopen failure: %s", dlerror());
    return 1;
  }

  printf("Loading: %s\n", target_library_path.c_str());
  void* target_library = dlopen(target_library_path.c_str(), RTLD_NOW);
  if (!target_library) {
    printf("dlopen failure: %s", dlerror());
    return 1;
  }

  void* sb_event_callback = dlsym(target_library, "SbEventHandle");
  if (!sb_event_callback) {
    printf("dlsym failure: %s", dlerror());
    return 1;
  }

  auto result = SbRunStarboardMain(
      argc, argv, reinterpret_cast<SbEventHandleCallback>(sb_event_callback));

  dlclose(target_library);
  dlclose(starboard_library);
  return result;
}
