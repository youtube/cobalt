// Copyright 2016 Google Inc. All Rights Reserved.
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

// A poem (POsix EMulation) for functions in wchar.h

#ifndef STARBOARD_CLIENT_PORTING_POEM_WCHAR_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_WCHAR_POEM_H_

#if defined(STARBOARD)

#if !defined(POEM_NO_EMULATION)

#include "starboard/string.h"

#undef vswprintf
#define vswprintf SbStringFormatWide
#undef wcsncmp
#define wcsncmp(s1, s2, c) SbStringCompareWide(s1, s2, c)

#endif  // POEM_NO_EMULATION

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_POEM_WCHAR_POEM_H_
