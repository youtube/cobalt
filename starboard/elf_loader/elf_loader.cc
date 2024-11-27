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

#include <string.h>
#include <vector>

#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/paths.h"
#include "starboard/common/time.h"
#include "starboard/configuration_constants.h"
#include "starboard/elf_loader/elf_loader_impl.h"
#include "starboard/elf_loader/evergreen_config.h"
#include "starboard/elf_loader/file_impl.h"
#include "starboard/extension/loader_app_metrics.h"
#include "starboard/extension/memory_mapped_file.h"
#include "starboard/system.h"

namespace starboard {
namespace elf_loader {

ElfLoader* ElfLoader::g_instance = NULL;

ElfLoader::ElfLoader() {
  ElfLoader* old_instance =
      reinterpret_cast<ElfLoader*>(SbAtomicAcquire_CompareAndSwapPtr(
          reinterpret_cast<SbAtomicPtr*>(&g_instance),
          reinterpret_cast<SbAtomicPtr>(reinterpret_cast<void*>(NULL)),
          reinterpret_cast<SbAtomicPtr>(this)));
  SB_DCHECK(!old_instance);

  impl_.reset(new ElfLoaderImpl());
}

ElfLoader::~ElfLoader() {
  ElfLoader* old_instance =
      reinterpret_cast<ElfLoader*>(SbAtomicAcquire_CompareAndSwapPtr(
          reinterpret_cast<SbAtomicPtr*>(&g_instance),
          reinterpret_cast<SbAtomicPtr>(this),
          reinterpret_cast<SbAtomicPtr>(reinterpret_cast<void*>(NULL))));
  SB_DCHECK(old_instance);
  SB_DCHECK(old_instance == this);
}

ElfLoader* ElfLoader::Get() {
  ElfLoader* elf_loader = reinterpret_cast<ElfLoader*>(
      SbAtomicAcquire_LoadPtr(reinterpret_cast<SbAtomicPtr*>(&g_instance)));
  SB_DCHECK(elf_loader);
  return elf_loader;
}

bool ElfLoader::Load(const std::string& library_path,
                     const std::string& content_path,
                     bool is_relative_path,
                     const void* (*custom_get_extension)(const char* name),
                     bool use_compression,
                     bool use_memory_mapped_file) {
  if (is_relative_path) {
    library_path_ = common::PrependContentPath(library_path);
    content_path_ = common::PrependContentPath(content_path);
  } else {
    library_path_ = library_path;
    content_path_ = content_path;
  }

  if (library_path_.empty() || content_path_.empty()) {
    SB_LOG(ERROR) << "|library_path_| and |content_path_| cannot be empty.";
    return false;
  }
  EvergreenConfig::Create(library_path_.c_str(), content_path_.c_str(),
                          custom_get_extension);
  SB_LOG(INFO) << "evergreen_config: content_path=" << content_path_;
  int64_t start_time_us = CurrentMonotonicTime();
  bool res = impl_->Load(library_path_.c_str(), use_compression,
                         use_memory_mapped_file);
  int64_t end_time_us = CurrentMonotonicTime();
  int64_t elf_load_duration_us = end_time_us - start_time_us;
  SB_LOG(INFO) << "Loading took: " << elf_load_duration_us / 1000 << " ms";
  auto metrics_extension =
      static_cast<const StarboardExtensionLoaderAppMetricsApi*>(
          SbSystemGetExtension(kStarboardExtensionLoaderAppMetricsName));
  if (metrics_extension &&
      strcmp(metrics_extension->name,
             kStarboardExtensionLoaderAppMetricsName) == 0 &&
      metrics_extension->version >= 2) {
    metrics_extension->SetElfLibraryStoredCompressed(use_compression);
    metrics_extension->SetElfLoadDurationMicroseconds(elf_load_duration_us);
  }

  return res;
}

void* ElfLoader::LookupSymbol(const char* symbol) {
  return impl_->LookupSymbol(symbol);
}

}  // namespace elf_loader
}  // namespace starboard
