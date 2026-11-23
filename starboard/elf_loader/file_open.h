// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ELF_LOADER_FILE_OPEN_H_
#define STARBOARD_ELF_LOADER_FILE_OPEN_H_

#include <stdint.h>

namespace elf_loader {

// Opens |name| without copying it when the platform stores it inside another
// file. Only Android implements this, for uncompressed assets in the APK.
// Returns a descriptor for the containing file and sets |offset| and |size| to
// |name|'s bytes in it. Returns -1 otherwise, and the caller falls back to
// open().
int OpenFileInPlace(const char* name, int64_t* offset, int64_t* size);

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_FILE_OPEN_H_
