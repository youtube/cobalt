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

// Functions for interacting with characters.

#ifndef STARBOARD_CHARACTER_H_
#define STARBOARD_CHARACTER_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Converts the given 8-bit character (as an int) to uppercase in the current
// locale, returning the result, also an 8-bit character. If there is no
// uppercase version of the character, or the character is already uppercase, it
// will just return the character as-is.
int SbCharacterToUpper(int c);

// Converts the given 8-bit character (as an int) to lowercase in the current
// locale, returning the result, also an 8-bit character. If there is no
// lowercase version of the character, or the character is already lowercase, it
// will just return the character as-is.
int SbCharacterToLower(int c);

// Returns whether the given 8-bit character |c| (as an int) is a space in the
// current locale.
bool SbCharacterIsSpace(int c);

// Returns whether the given 8-bit character |c| (as an int) is uppercase in the
// current locale.
bool SbCharacterIsUpper(int c);

// Returns whether the given 8-bit character |c| (as an int) is a decimal digit
// in the current locale.
bool SbCharacterIsDigit(int c);

// Returns whether the given 8-bit character |c| (as an int) is a hexidecimal
// digit in the current locale.
bool SbCharacterIsHexDigit(int c);

// Returns whether the given 8-bit character |c| (as an int) is alphanumeric in
// the current locale.
bool SbCharacterIsAlphanumeric(int c);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_CHARACTER_H_
