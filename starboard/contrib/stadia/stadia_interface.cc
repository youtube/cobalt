// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/contrib/stadia/stadia_interface.h"

#include <dlfcn.h>
#include <string>

#include "starboard/common/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace contrib {
namespace stadia {

namespace {
void* LookupSymbol(void* handle, const std::string& name) {
  // Note, we assume that a NULL result from dlsym is abnormal in our case
  // (even though this is not true in the general case).
  void* symbol = dlsym(handle, name.c_str());
  if (!symbol) {
    SB_LOG(ERROR) << "Failed to find symbol for " << name << ": " << dlerror();
  }
  return symbol;
}
}  // namespace

StadiaInterface::StadiaInterface() {
  // Try to open the streaming client library.
  void* lib_ptr = dlopen("libstreaming_client.so", RTLD_LAZY);
  if (!lib_ptr) {
    SB_LOG(ERROR) << "Failed to open streaming client library: " << dlerror();
  }
  // Try look up the library's entry point.
  StadiaPluginHas = reinterpret_cast<StadiaPluginHasFunction>(
      LookupSymbol(lib_ptr, kStadiaPluginHas));
  StadiaPluginOpen = reinterpret_cast<StadiaPluginOpenFunction>(
      LookupSymbol(lib_ptr, kStadiaPluginOpen));
  StadiaPluginSendTo = reinterpret_cast<StadiaPluginSendToFunction>(
      LookupSymbol(lib_ptr, kStadiaPluginSendTo));
  StadiaPluginClose = reinterpret_cast<StadiaPluginCloseFunction>(
      LookupSymbol(lib_ptr, kStadiaPluginClose));
  StadiaInitialize = reinterpret_cast<StadiaInitializeFunction>(
      LookupSymbol(lib_ptr, kStadiaInitialize));

  // Return an empty optional if any entry point wasn't found.
  if (!StadiaPluginHas || !StadiaPluginOpen || !StadiaPluginSendTo ||
      !StadiaPluginClose || !StadiaInitialize) {
    SB_LOG(ERROR) << "Failed to look up one or more symbols, "
                  << "disabling streaming client.";
  }
}

}  // namespace stadia
}  // namespace contrib
}  // namespace starboard

// By default, the stadia_interface does not exist; it must be instantiated by
// the application.
const starboard::contrib::stadia::StadiaInterface* g_stadia_interface = nullptr;
