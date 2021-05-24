// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Character module
//
// Provides functions for interacting with characters.

#ifndef STARBOARD_CHARACTER_H_
#define STARBOARD_CHARACTER_H_

#include "starboard/configuration.h"

#if SB_API_VERSION < 13
#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Converts the given 8-bit character (as an int) to uppercase in the current
// locale and returns an 8-bit character. If there is no uppercase version of
// the character, or the character is already uppercase, the function just
// returns the character as-is.
//
// |c|: The character to be converted.
SB_EXPORT int SbCharacterToUpper(int c);

// Converts the given 8-bit character (as an int) to lowercase in the current
// locale and returns an 8-bit character. If there is no lowercase version of
// the character, or the character is already lowercase, the function just
// returns the character as-is.
//
// |c|: The character to be converted.
SB_EXPORT int SbCharacterToLower(int c);

// Indicates whether the given 8-bit character |c| (as an int) is a space in
// the current locale.
//
// |c|: The character to be evaluated.
SB_EXPORT bool SbCharacterIsSpace(int c);

// Indicates whether the given 8-bit character |c| (as an int) is uppercase
// in the current locale.
//
// |c|: The character to be evaluated.
SB_EXPORT bool SbCharacterIsUpper(int c);

// Indicates whether the given 8-bit character |c| (as an int) is a
// decimal digit in the current locale.
//
// |c|: The character to be evaluated.
SB_EXPORT bool SbCharacterIsDigit(int c);

// Indicates whether the given 8-bit character |c| (as an int) is a hexadecimal
// in the current locale.
//
// |c|: The character to be evaluated.
SB_EXPORT bool SbCharacterIsHexDigit(int c);

// Indicates whether the given 8-bit character |c| (as an int) is alphanumeric
// in the current locale.
//
// |c|: The character to be evaluated.
SB_EXPORT bool SbCharacterIsAlphanumeric(int c);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_API_VERSION < 13
#endif  // STARBOARD_CHARACTER_H_
