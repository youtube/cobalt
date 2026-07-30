//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "third_party/starboard/rdk/shared/accessibility_data.h"

namespace WPEFramework {

ENUM_CONVERSION_BEGIN(SbAccessibilityCaptionCharacterEdgeStyle)
  { kSbAccessibilityCaptionCharacterEdgeStyleNone, _TXT("None") },
  { kSbAccessibilityCaptionCharacterEdgeStyleRaised, _TXT("Raised") },
  { kSbAccessibilityCaptionCharacterEdgeStyleDepressed, _TXT("Depressed") },
  { kSbAccessibilityCaptionCharacterEdgeStyleUniform, _TXT("Uniform") },
  { kSbAccessibilityCaptionCharacterEdgeStyleDropShadow, _TXT("DropShadow") },
ENUM_CONVERSION_END(SbAccessibilityCaptionCharacterEdgeStyle);

ENUM_CONVERSION_BEGIN(SbAccessibilityCaptionColor)
  { kSbAccessibilityCaptionColorBlue, _TXT("Blue") },
  { kSbAccessibilityCaptionColorBlack, _TXT("Black") },
  { kSbAccessibilityCaptionColorCyan, _TXT("Cyan") },
  { kSbAccessibilityCaptionColorGreen, _TXT("Green") },
  { kSbAccessibilityCaptionColorMagenta, _TXT("Magenta") },
  { kSbAccessibilityCaptionColorRed, _TXT("Red") },
  { kSbAccessibilityCaptionColorWhite, _TXT("White") },
  { kSbAccessibilityCaptionColorYellow, _TXT("Yellow") },
ENUM_CONVERSION_END(SbAccessibilityCaptionColor);

ENUM_CONVERSION_BEGIN(SbAccessibilityCaptionFontFamily)
  { kSbAccessibilityCaptionFontFamilyCasual, _TXT("Casual") },
  { kSbAccessibilityCaptionFontFamilyCursive, _TXT("Cursive") },
  { kSbAccessibilityCaptionFontFamilyMonospaceSansSerif, _TXT("MonospaceSansSerif") },
  { kSbAccessibilityCaptionFontFamilyMonospaceSerif, _TXT("MonospaceSerif") },
  { kSbAccessibilityCaptionFontFamilyProportionalSansSerif, _TXT("ProportionalSansSerif") },
  { kSbAccessibilityCaptionFontFamilyProportionalSerif, _TXT("ProportionalSerif") },
  { kSbAccessibilityCaptionFontFamilySmallCapitals, _TXT("SmallCapitals") },
ENUM_CONVERSION_END(SbAccessibilityCaptionFontFamily);

ENUM_CONVERSION_BEGIN(SbAccessibilityCaptionFontSizePercentage)
  { kSbAccessibilityCaptionFontSizePercentage25, _TXT("25") },
  { kSbAccessibilityCaptionFontSizePercentage50, _TXT("50") },
  { kSbAccessibilityCaptionFontSizePercentage75, _TXT("75") },
  { kSbAccessibilityCaptionFontSizePercentage100, _TXT("100") },
  { kSbAccessibilityCaptionFontSizePercentage125, _TXT("125") },
  { kSbAccessibilityCaptionFontSizePercentage150, _TXT("150") },
  { kSbAccessibilityCaptionFontSizePercentage175, _TXT("175") },
  { kSbAccessibilityCaptionFontSizePercentage200, _TXT("200") },
  { kSbAccessibilityCaptionFontSizePercentage225, _TXT("225") },
  { kSbAccessibilityCaptionFontSizePercentage250, _TXT("250") },
  { kSbAccessibilityCaptionFontSizePercentage275, _TXT("275") },
  { kSbAccessibilityCaptionFontSizePercentage300, _TXT("300") },
ENUM_CONVERSION_END(SbAccessibilityCaptionFontSizePercentage);

ENUM_CONVERSION_BEGIN(SbAccessibilityCaptionOpacityPercentage)
  { kSbAccessibilityCaptionOpacityPercentage0, _TXT("0") },
  { kSbAccessibilityCaptionOpacityPercentage25, _TXT("25") },
  { kSbAccessibilityCaptionOpacityPercentage50, _TXT("50") },
  { kSbAccessibilityCaptionOpacityPercentage75, _TXT("75") },
  { kSbAccessibilityCaptionOpacityPercentage100, _TXT("100") },
ENUM_CONVERSION_END(SbAccessibilityCaptionOpacityPercentage);

}
