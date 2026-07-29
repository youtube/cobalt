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

// This file provides a simple API for exposing Starboard as a
// library to another app.

#ifndef STARBOARD_SHARED_LIB_EXPORTED_STARBOARD_MAIN_H_
#define STARBOARD_SHARED_LIB_EXPORTED_STARBOARD_MAIN_H_

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

SB_EXPORT_PLATFORM int StarboardMain(int argc, char** argv);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_LIB_EXPORTED_STARBOARD_MAIN_H_
