// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/iso/directory_internal.h"

#include "starboard/shared/iso/impl/directory_get_next.h"

#if SB_API_VERSION >= 12
bool SbDirectoryGetNext(SbDirectory directory,
                        char* out_entry,
                        size_t out_entry_size) {
  return ::starboard::shared::iso::impl::SbDirectoryGetNext(
      directory, out_entry, out_entry_size);
}
#else   // SB_API_VERSION >= 12
bool SbDirectoryGetNext(SbDirectory directory, SbDirectoryEntry* out_entry) {
  return ::starboard::shared::iso::impl::SbDirectoryGetNext(directory,
                                                            out_entry);
}
#endif  // SB_API_VERSION >= 12
