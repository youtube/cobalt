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

// Starboard IO base function header for compress/uncompress .zip

#ifndef _ZLIBIOSTARBOARD_H_
#define _ZLIBIOSTARBOARD_H_

#include "ioapi.h"
#include "third_party/zlib/zlib.h"

#ifdef __cplusplus
extern "C" {
#endif

void fill_starboard_filefunc OF((zlib_filefunc_def * pzlib_filefunc_def));
void fill_starboard_filefunc64 OF((zlib_filefunc64_def * pzlib_filefunc_def));

#ifdef __cplusplus
}
#endif

#endif  // _ZLIBIOSTARBOARD_H_
