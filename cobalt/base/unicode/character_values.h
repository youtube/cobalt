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

#ifndef COBALT_BASE_UNICODE_CHARACTER_VALUES_H_
#define COBALT_BASE_UNICODE_CHARACTER_VALUES_H_

#include "third_party/icu/source/common/unicode/uchar.h"

namespace base {
namespace unicode {

// These character values come from the Unicode character table, and include the
// values used by Cobalt.
// http://unicode-table.com/en/

const UChar kActivateArabicFormShapingCharacter = 0x206D;
const UChar kActivateSymmetricSwappingCharacter = 0x206B;
const UChar kArabicLetterMarkCharacter = 0x061C;
const UChar kCarriageReturnCharacter = 0x000D;
const UChar kFirstStrongIsolateCharacter = 0x2068;
const UChar kFormFeedCharacter = 0x000C;
const UChar kHorizontalTabulationCharacter = 0x0009;
const UChar kInhibitArabicFormShapingCharacter = 0x206C;
const UChar kInhibitSymmetricSwappingCharacter = 0x206A;
const UChar kLeftToRightEmbeddingCharacter = 0x202A;
const UChar kLeftToRightIsolateCharacter = 0x2066;
const UChar kLeftToRightMarkCharacter = 0x200E;
const UChar kLeftToRightOverrideCharacter = 0x202D;
const UChar kNationalDigitShapesCharacter = 0x206E;
const UChar kNominalDigitShapesCharacter = 0x206F;
const UChar kNewLineCharacter = 0x000A;
const UChar kNoBreakSpaceCharacter = 0x00A0;
const UChar kObjectReplacementCharacter = 0xFFFC;
const UChar kPopDirectionalFormattingCharacter = 0x202C;
const UChar kPopDirectionalIsolateCharacter = 0x2069;
const UChar kRightToLeftEmbeddingCharacter = 0x202B;
const UChar kRightToLeftIsolateCharacter = 0x2067;
const UChar kRightToLeftMarkCharacter = 0x200F;
const UChar kRightToLeftOverrideCharacter = 0x202E;
const UChar kSoftHyphenCharacter = 0x00AD;
const UChar kSpaceCharacter = 0x0020;
const UChar kZeroWidthJoinerCharacter = 0x200D;
const UChar kZeroWidthNoBreakSpaceCharacter = 0xFEFF;
const UChar kZeroWidthNonJoinerCharacter = 0x200C;
const UChar kZeroWidthSpaceCharacter = 0x200B;

}  // namespace unicode
}  // namespace base

#endif  // COBALT_BASE_UNICODE_CHARACTER_VALUES_H_
