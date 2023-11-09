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

#include "starboard/shared/posix/runtime_linking.h"

#include <dlfcn.h>
#include "starboard/extension/runtime_linking.h"

namespace starboard {
namespace shared {
namespace posix {

namespace {

void* OpenLibrary(const char* file_name) {
  return dlopen(file_name, RTLD_NOW);
}

void* LookupSymbol(void* handle, const char* symbol_name) {
  return dlsym(handle, symbol_name);
}

bool CloseLibrary(void* handle) {
  return dlclose(handle) == 0 ? true : false;
}

const CobaltExtensionRuntimeLinkingApi kRuntimeLinkingApi = {
    kCobaltExtensionRuntimeLinkingName, 1, &OpenLibrary, &LookupSymbol,
    &CloseLibrary};

}  // namespace

const void* GetRuntimeLinkingApi() {
  return &kRuntimeLinkingApi;
}

}  // namespace posix
}  // namespace shared
}  // namespace starboard
