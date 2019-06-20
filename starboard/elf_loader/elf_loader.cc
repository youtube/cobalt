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

#include "starboard/elf_loader/elf_loader.h"
#include "starboard/common/log.h"
#include "starboard/elf_loader/elf_loader_impl.h"
#include "starboard/elf_loader/file_impl.h"

namespace starboard {
namespace elf_loader {

ElfLoader::~ElfLoader() {}

bool ElfLoader::Load(const char* file_name) {
  return impl_->Load(file_name);
}

void* ElfLoader::LookupSymbol(const char* symbol) {
  return impl_->LookupSymbol(symbol);
}

ElfLoader::ElfLoader() {
  impl_.reset(new ElfLoaderImpl());
}

}  // namespace elf_loader
}  // namespace starboard
