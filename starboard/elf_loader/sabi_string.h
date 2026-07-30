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

#ifndef STARBOARD_ELF_LOADER_SABI_STRING_H_
#define STARBOARD_ELF_LOADER_SABI_STRING_H_

#include "starboard/export.h"

extern "C" {

// Retrieve the SABI ID. The function is exported by all Evergreen
// binaries for verification purposes. The loader should call it
// to enforce matching of the Evergreen SABI to its own SABI configuration.
SB_IMPORT const char* GetEvergreenSabiString();

}  // extern "C"

// Check whether the |sabi_function| returns the same string as SB_SABI_JSON_ID.
bool CheckSabi(const char* (*sabi_function)());

#endif  // STARBOARD_ELF_LOADER_SABI_STRING_H_
