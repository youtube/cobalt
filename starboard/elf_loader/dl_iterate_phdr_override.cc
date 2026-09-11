// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "starboard/elf_loader/dl_iterate_phdr_override.h"

#include <dlfcn.h>
#include <link.h>
#include <string.h>

#include <atomic>

#include "starboard/elf_loader/evergreen_info.h"

namespace {

typedef int (*DlIteratePhdrFn)(int (*callback)(struct dl_phdr_info* info,
                                               size_t size,
                                               void* data),
                               void* data);

std::atomic<DlIteratePhdrFn> g_real_dl_iterate_phdr{nullptr};

}  // namespace

extern "C" {

void InitDlIteratePhdrOverride() {
  if (g_real_dl_iterate_phdr.load(std::memory_order_relaxed) == nullptr) {
    DlIteratePhdrFn real_fn =
        reinterpret_cast<DlIteratePhdrFn>(dlsym(RTLD_NEXT, "dl_iterate_phdr"));
    g_real_dl_iterate_phdr.store(real_fn, std::memory_order_release);
  }
}

__attribute__((constructor)) static void AutoInitDlIteratePhdrOverride() {
  InitDlIteratePhdrOverride();
}

SB_EXPORT_PLATFORM SB_NO_SANITIZE_ADDRESS int dl_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data),
    void* data) {
  DlIteratePhdrFn real_fn =
      g_real_dl_iterate_phdr.load(std::memory_order_acquire);

  int status = 0;
  if (real_fn) {
    status = real_fn(callback, data);
    if (status != 0) {
      return status;
    }
  }

  // Synthesize dl_phdr_info for libcobalt.so if Evergreen info is available.
  EvergreenInfo evergreen_info = {};
  if (GetEvergreenInfo(&evergreen_info) && evergreen_info.base_address != 0 &&
      evergreen_info.phdr_table != 0 && evergreen_info.phdr_table_num > 0) {
    struct dl_phdr_info info;
    memset(&info, 0, sizeof(info));
    info.dlpi_addr = static_cast<ElfW(Addr)>(evergreen_info.base_address);
    info.dlpi_name = evergreen_info.file_path_buf;
    info.dlpi_phdr =
        reinterpret_cast<const ElfW(Phdr)*>(evergreen_info.phdr_table);
    info.dlpi_phnum = static_cast<ElfW(Half)>(evergreen_info.phdr_table_num);

    status = callback(&info, sizeof(info), data);
  }

  return status;
}

}  // extern "C"
