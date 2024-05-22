// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/profiler/module_cache.h"

#include "base/notreached.h"

namespace base {

class TModule : public ModuleCache::Module {
  public:
    TModule();
    uintptr_t GetBaseAddress() const override { return 0; }
    std::string GetId() const override { return ""; }
    FilePath GetDebugBasename() const override { return FilePath(); }
    size_t GetSize() const override { return 0; }
    bool IsNative() const override { return false; }
};

TModule::TModule() {}

// static
std::unique_ptr<const ModuleCache::Module> ModuleCache::CreateModuleForAddress(uintptr_t address) {
  NOTIMPLEMENTED();
  return std::make_unique<TModule>();
}

}  // namespace base
