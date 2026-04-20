// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard File module
//
// Defines file system input/output functions.

#ifndef STARBOARD_FILE_H_
#define STARBOARD_FILE_H_

#include <stdint.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Replaces the content of the file at |path| with |data|. Returns whether the
// contents of the file were replaced. The replacement of the content is an
// atomic operation. The file will either have all of the data, or none.
//
// |path|: The path to the file whose contents should be replaced.
// |data|: The data to replace the file contents with.
// |data_size|: The amount of |data|, in bytes, to be written to the file.
SB_EXPORT bool SbFileAtomicReplace(const char* path,
                                   const char* data,
                                   int64_t data_size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_FILE_H_
