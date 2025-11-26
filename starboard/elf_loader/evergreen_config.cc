// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/elf_loader/evergreen_config.h"

#include <memory>
#include <mutex>

#include "starboard/common/log.h"

namespace elf_loader {

static std::mutex g_evergreen_config_mutex;
static std::unique_ptr<EvergreenConfig> g_evergreen_config;

void EvergreenConfig::Create(
    const char* library_path,
    const char* content_path,
    const void* (*custom_get_extension)(const char* name)) {
  std::lock_guard lock(g_evergreen_config_mutex);
  g_evergreen_config.reset(
      new EvergreenConfig(library_path, content_path, custom_get_extension));
}

const EvergreenConfig* EvergreenConfig::GetInstance() {
  std::lock_guard lock(g_evergreen_config_mutex);
  SB_CHECK(g_evergreen_config)
      << "EvergreenConfig::GetInstance() called but g_evergreen_config is null "
         "and not initialized.";
  return g_evergreen_config.get();
}

EvergreenConfig::EvergreenConfig(
    const char* library_path,
    const char* content_path,
    const void* (*custom_get_extension)(const char* name))
    : library_path_(library_path),
      content_path_(content_path),
      custom_get_extension_(custom_get_extension) {}
}  // namespace elf_loader
