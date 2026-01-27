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

#pragma once

#include <core/JSON.h>
#include <core/Enumerate.h>
#include "starboard/extension/accessibility.h"

namespace WPEFramework {

namespace JsonData {

namespace Accessibility {

  struct AccessibilityData : public Core::JSON::Container {

    struct CCSettingsData : public Core::JSON::Container {
      CCSettingsData()
        : Core::JSON::Container() {
        Add(_T("isenabled"), &IsEnabled);
        Add(_T("backgroundcolor"), &BackgroundColor);
        Add(_T("backgroundopacity"), &BackgroundOpacity);
        Add(_T("characteredgestyle"), &CharacterEdgeStyle);
        Add(_T("fontcolor"), &FontColor);
        Add(_T("fontfamily"), &FontFamily);
        Add(_T("fontopacity"), &FontOpacity);
        Add(_T("fontsize"), &FontSize);
        Add(_T("windowcolor"), &WindowColor);
        Add(_T("windowopacity"), &WindowOpacity);
      }
      CCSettingsData(const CCSettingsData&) = delete;
      CCSettingsData& operator=(const CCSettingsData&) = delete;

      Core::JSON::Boolean IsEnabled;
      Core::JSON::EnumType<SbAccessibilityCaptionColor> BackgroundColor;
      Core::JSON::EnumType<SbAccessibilityCaptionOpacityPercentage> BackgroundOpacity;
      Core::JSON::EnumType<SbAccessibilityCaptionCharacterEdgeStyle> CharacterEdgeStyle;
      Core::JSON::EnumType<SbAccessibilityCaptionColor> FontColor;
      Core::JSON::EnumType<SbAccessibilityCaptionFontFamily> FontFamily;
      Core::JSON::EnumType<SbAccessibilityCaptionOpacityPercentage> FontOpacity;
      Core::JSON::EnumType<SbAccessibilityCaptionFontSizePercentage> FontSize;
      Core::JSON::EnumType<SbAccessibilityCaptionColor> WindowColor;
      Core::JSON::EnumType<SbAccessibilityCaptionOpacityPercentage> WindowOpacity;
    };

    struct TextDisplaySettingsData : public Core::JSON::Container {
      TextDisplaySettingsData()
        : Core::JSON::Container() {
        Add(_T("ishighcontrasttextenabled"), &IsHighContrastTextEnabled);
      }
      TextDisplaySettingsData(const TextDisplaySettingsData&) = delete;
      TextDisplaySettingsData& operator=(const TextDisplaySettingsData&) = delete;

      Core::JSON::Boolean IsHighContrastTextEnabled;
    };

    AccessibilityData()
      : Core::JSON::Container() {
      Add(_T("closedcaptions"), &ClosedCaptions);
      Add(_T("textdisplay"), &TextDisplay);
    }
    AccessibilityData(const AccessibilityData&) = delete;
    AccessibilityData& operator=(const AccessibilityData&) = delete;

    CCSettingsData ClosedCaptions;
    TextDisplaySettingsData TextDisplay;
  };

}  // namespace Accessibility

}  // namespace JsonData

ENUM_CONVERSION_HANDLER(SbAccessibilityCaptionCharacterEdgeStyle);
ENUM_CONVERSION_HANDLER(SbAccessibilityCaptionColor);
ENUM_CONVERSION_HANDLER(SbAccessibilityCaptionFontFamily);
ENUM_CONVERSION_HANDLER(SbAccessibilityCaptionFontSizePercentage);
ENUM_CONVERSION_HANDLER(SbAccessibilityCaptionOpacityPercentage);

}  // namespace WPEFramework
