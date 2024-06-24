// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_directory_abi_wrappers.h"
#include <string.h>
#include <algorithm>
#include "starboard/common/log.h"

int __abi_wrap_readdir_r(DIR* dirp,
                         struct musl_dirent* musl_entry,
                         struct musl_dirent** musl_result) {
  // readdir_r segfaults if any of those parameters are missing.
  SB_CHECK(dirp);
  SB_CHECK(musl_entry);
  SB_CHECK(musl_result);

  struct dirent entry = {0};  // The type from platform toolchain.
  struct dirent* result = nullptr;
  int retval = readdir_r(dirp, &entry, &result);
  if (retval != 0) {
    return retval;
  }
#if !SB_HAS_QUIRK(INCOMPLETE_DIRENT_STRUCTURE)
  musl_entry->d_ino = entry.d_ino;
  musl_entry->d_off = entry.d_off;
#endif
  musl_entry->d_reclen = entry.d_reclen;
  musl_entry->d_type = entry.d_type;

  memset(musl_entry->d_name, 0, sizeof(musl_entry->d_name));
  constexpr auto minlen =
      std::min(sizeof(musl_entry->d_name), sizeof(entry.d_name));
  memcpy(musl_entry->d_name, entry.d_name, minlen);
  if (result == nullptr) {
    *musl_result = nullptr;
  } else {
    *musl_result = musl_entry;
  }
  return 0;
}
