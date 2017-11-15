// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef BASE_FILE_PATH_CONFIGURATION_H_
#define BASE_FILE_PATH_CONFIGURATION_H_

#include "starboard/configuration.h"

#if !defined(FILE_PATH_USES_WIN_SEPARATORS) && (SB_FILE_SEP_CHAR == '\\')
#define FILE_PATH_USES_WIN_SEPARATORS 1
#endif

#if !defined(FILE_PATH_USES_DRIVE_LETTERS) && (SB_FILE_SEP_CHAR == '\\')
#define FILE_PATH_USES_DRIVE_LETTERS 1
#endif

#endif  // BASE_FILE_PATH_CONFIGURATION_H_
