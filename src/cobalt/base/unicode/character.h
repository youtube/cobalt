// Copyright (C) 2016 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef COBALT_BASE_UNICODE_CHARACTER_H_
#define COBALT_BASE_UNICODE_CHARACTER_H_

#include "cobalt/base/unicode/character_values.h"

#include "third_party/icu/source/common/unicode/uchar.h"

namespace base {
namespace unicode {

// Returns true if any character is contained from a script that requires
// complex text shaping.
bool ContainsComplexScript(const UChar* characters, unsigned len);

// Returns true if the character should be treated as a space character during
// glyph lookup.
bool TreatAsSpace(UChar32 c);

// Returns true if the character should be treated as a zero-width space
// character during glyph lookup.
// NOTE: Complex shaping characters are treated as zero width spaces.
bool TreatAsZeroWidthSpace(UChar32 c);

// Returns true if the character should treated as a zero-width space character
// during glyph lookup within a complex script.
// NOTE: Complex shaping characters are not treated as zero width spaces.
bool TreatAsZeroWidthSpaceInComplexScript(UChar32 c);

// Returns true if this character is an emoji skin tone modifier.
bool IsEmojiModifier(UChar32 c);

// Returns a character normalized for spaces and zero width spaces prior to
// glyph lookup.
// NOTE: Complex shaping characters are normalized to zero width spaces.
UChar32 NormalizeSpaces(UChar32 character);

// Returns a character normalized for spaces and zero width spaces prior to
// glyph lookup within a complex script.
// NOTE: Complex shaping characters are not normalized to zero width spaces.
UChar32 NormalizeSpacesInComplexScript(UChar32 character);

}  // namespace unicode
}  // namespace base

#endif  // COBALT_BASE_UNICODE_CHARACTER_H_
