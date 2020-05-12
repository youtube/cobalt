// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/elf_loader/elf_loader_sys_impl.h"

#include <dlfcn.h>

#include "starboard/common/log.h"
#include "starboard/elf_loader/exported_symbols.h"

namespace starboard {
namespace elf_loader {

ElfLoaderImpl::ElfLoaderImpl() {}

bool ElfLoaderImpl::Load(
    const char* name,
    const void* (*custom_get_extension)(const char* name)) {
  SB_LOG(INFO) << "Loading: " << name;

  // Creating the instance forces the binary to keep all the symbols.
  ExportedSymbols symbols;

  handle_ = dlopen(name, RTLD_NOW);
  if (!handle_) {
    SB_LOG(ERROR) << "dlopen failure: " << dlerror();
    return false;
  }
  return true;
}

void* ElfLoaderImpl::LookupSymbol(const char* symbol) {
  if (handle_) {
    void* p = dlsym(handle_, symbol);
    if (!p) {
      SB_LOG(ERROR) << "dlsym failure: " << dlerror();
    }
    return p;
  }
  return NULL;
}

ElfLoaderImpl::~ElfLoaderImpl() {
  if (handle_) {
    dlclose(handle_);
  }
}
}  // namespace elf_loader
}  // namespace starboard
