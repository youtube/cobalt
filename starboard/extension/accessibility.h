// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Accessibility module
//
// Provides access to the system options and settings related to accessibility.

#ifndef STARBOARD_EXTENSION_ACCESSIBILITY_H_
#define STARBOARD_EXTENSION_ACCESSIBILITY_H_

#include <stdint.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionAccessibilityName \
  "dev.starboard.extension.Accessibility"

// A callback function that is called when the system's text-to-speech
// settings have changed. The |context| parameter is the value that was
// passed to |RegisterOnTextToSpeechStateChangedCallback|.
typedef void (*SbOnTextToSpeechStateChangedCallback)(void* context);

// A group of settings related to text-to-speech functionality, for platforms
// that expose system settings for text-to-speech.
typedef struct SbAccessibilityTextToSpeechSettings {
  // Whether this platform has a system setting for text-to-speech or not.
  bool has_text_to_speech_setting;

  // Whether the text-to-speech setting is enabled or not. This setting is only
  // valid if |has_text_to_speech_setting| is set to true.
  bool is_text_to_speech_enabled;
} SbAccessibilityTextToSpeechSettings;

typedef struct SbAccessibilityDisplaySettings {
  // Whether this platform has a system setting for high contrast text or not.
  bool has_high_contrast_text_setting;

  // Whether the high contrast text setting is enabled or not.
  bool is_high_contrast_text_enabled;
} SbAccessibilityDisplaySettings;

// Enum for possible closed captioning character edge styles.
typedef enum SbAccessibilityCaptionCharacterEdgeStyle {
  kSbAccessibilityCaptionCharacterEdgeStyleNone,
  kSbAccessibilityCaptionCharacterEdgeStyleRaised,
  kSbAccessibilityCaptionCharacterEdgeStyleDepressed,
  kSbAccessibilityCaptionCharacterEdgeStyleUniform,
  kSbAccessibilityCaptionCharacterEdgeStyleDropShadow,
} SbAccessibilityCaptionCharacterEdgeStyle;

// Enum for possible closed captioning colors.
typedef enum SbAccessibilityCaptionColor {
  kSbAccessibilityCaptionColorBlue,
  kSbAccessibilityCaptionColorBlack,
  kSbAccessibilityCaptionColorCyan,
  kSbAccessibilityCaptionColorGreen,
  kSbAccessibilityCaptionColorMagenta,
  kSbAccessibilityCaptionColorRed,
  kSbAccessibilityCaptionColorWhite,
  kSbAccessibilityCaptionColorYellow,
} SbAccessibilityCaptionColor;

// Enum for possible closed captioning font families
typedef enum SbAccessibilityCaptionFontFamily {
  kSbAccessibilityCaptionFontFamilyCasual,
  kSbAccessibilityCaptionFontFamilyCursive,
  kSbAccessibilityCaptionFontFamilyMonospaceSansSerif,
  kSbAccessibilityCaptionFontFamilyMonospaceSerif,
  kSbAccessibilityCaptionFontFamilyProportionalSansSerif,
  kSbAccessibilityCaptionFontFamilyProportionalSerif,
  kSbAccessibilityCaptionFontFamilySmallCapitals,
} SbAccessibilityCaptionFontFamily;

// Enum for possible closed captioning font size percentages.
typedef enum SbAccessibilityCaptionFontSizePercentage {
  kSbAccessibilityCaptionFontSizePercentage25,
  kSbAccessibilityCaptionFontSizePercentage50,
  kSbAccessibilityCaptionFontSizePercentage75,
  kSbAccessibilityCaptionFontSizePercentage100,
  kSbAccessibilityCaptionFontSizePercentage125,
  kSbAccessibilityCaptionFontSizePercentage150,
  kSbAccessibilityCaptionFontSizePercentage175,
  kSbAccessibilityCaptionFontSizePercentage200,
  kSbAccessibilityCaptionFontSizePercentage225,
  kSbAccessibilityCaptionFontSizePercentage250,
  kSbAccessibilityCaptionFontSizePercentage275,
  kSbAccessibilityCaptionFontSizePercentage300,
} SbAccessibilityCaptionFontSizePercentage;

// Enum for possible closed captioning opacity percentages.
typedef enum SbAccessibilityCaptionOpacityPercentage {
  kSbAccessibilityCaptionOpacityPercentage0,
  kSbAccessibilityCaptionOpacityPercentage25,
  kSbAccessibilityCaptionOpacityPercentage50,
  kSbAccessibilityCaptionOpacityPercentage75,
  kSbAccessibilityCaptionOpacityPercentage100,
} SbAccessibilityCaptionOpacityPercentage;

// Enum for possible states of closed captioning properties.
typedef enum SbAccessibilityCaptionState {
  // The property is not supported by the system. The application should provide
  // a way to set this property, otherwise it will not be changeable.
  // For any given closed captioning property, if its corresponding state
  // property has a value of |kSbAccessibilityCaptionStateUnsupported|, then its
  // own value is undefined.  For example, if
  // |SbAccessibilityCaptionColor::background_color_state| has a value of
  // |kSbAccessibilityCaptionStateUnsupported|, then the value of
  // |SbAccessibilityCaptionColor::background_color| is undefined.
  kSbAccessibilityCaptionStateUnsupported = 0,

  // The property is supported by the system, but the user has not set it.
  // The application should provide a default setting for the property to
  // handle this case.
  kSbAccessibilityCaptionStateUnset,

  // The user has set this property as a system default, meaning that it should
  // take priority over app defaults. If
  // SbAccessibilityCaptionSettings.supportsOverride contains true, this value
  // should be interpreted as explicitly saying "do not override." If it
  // contains false, it is up to the application to interpret any additional
  // meaning of this value.
  kSbAccessibilityCaptionStateSet,

  // This property should take priority over everything but application-level
  // overrides, including video caption data.  If
  // SbAccessibilityCaptionSettings.supportsOverride contains false, then no
  // fields of SbAccessibilityCaptionSettings will ever contain this value.
  kSbAccessibilityCaptionStateOverride,
} SbAccessibilityCaptionState;

// A group of settings related to system-level closed captioning settings, for
// platforms that expose closed captioning settings.
typedef struct SbAccessibilityCaptionSettings {
  SbAccessibilityCaptionColor background_color;
  SbAccessibilityCaptionState background_color_state;

  SbAccessibilityCaptionOpacityPercentage background_opacity;
  SbAccessibilityCaptionState background_opacity_state;

  SbAccessibilityCaptionCharacterEdgeStyle character_edge_style;
  SbAccessibilityCaptionState character_edge_style_state;

  SbAccessibilityCaptionColor font_color;
  SbAccessibilityCaptionState font_color_state;

  SbAccessibilityCaptionFontFamily font_family;
  SbAccessibilityCaptionState font_family_state;

  SbAccessibilityCaptionOpacityPercentage font_opacity;
  SbAccessibilityCaptionState font_opacity_state;

  SbAccessibilityCaptionFontSizePercentage font_size;
  SbAccessibilityCaptionState font_size_state;

  SbAccessibilityCaptionColor window_color;
  SbAccessibilityCaptionState window_color_state;

  SbAccessibilityCaptionOpacityPercentage window_opacity;
  SbAccessibilityCaptionState window_opacity_state;

  // The |is_enabled| attribute determines if the user has chosen to enable
  // closed captions on their system.
  bool is_enabled;

  // Some platforms support enabling or disabling captions, some support reading
  // whether they are enabled from the system settings, and others support
  // neither. As a result, there are separate checks for getting and setting
  // the value that is contained in the |is_enabled| attribute. Modifying the
  // attribute via |SbAccessibilitySetCaptionsEnabled| will change the setting
  // system-wide. Attempting to read |is_enabled| when the value of
  // |supports_is_enabled| is false will always return false. Attempting to set
  // |is_enabled| via |SbAccessibilitySetCaptionsEnabled| when the value of
  // |supports_set_enabled| is false will fail silently.
  bool supports_is_enabled;
  bool supports_set_enabled;

  // Some platforms may specify that when setting a property, it should override
  // data from video streams and application settings (unless the application
  // has its own overrides). Depending on whether this attribute contains true
  // or false, the values of |SbAccessibilityCaptionState| should be interpreted
  // differently.
  bool supports_override;
} SbAccessibilityCaptionSettings;

typedef struct StarboardExtensionAccessibilityApi {
  // Name should be the string
  // |kStarboardExtensionAccessibilityName|. This helps to validate that
  // the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // Gets the platform settings related to the text-to-speech accessibility
  // feature. This function returns false if |out_settings| is NULL or if it is
  // not zero-initialized.
  //
  // |out_settings|: A pointer to a zero-initialized
  //    SbAccessibilityTextToSpeechSettings struct.
  bool (*GetTextToSpeechSettings)(
      SbAccessibilityTextToSpeechSettings* out_settings);

  // Gets the platform settings related to high contrast text.
  // This function returns false if |out_settings| is NULL or if it is
  // not zero-initialized.
  //
  // |out_settings|: A pointer to a zero-initialized
  //    SbAccessibilityDisplaySettings* struct.
  bool (*GetDisplaySettings)(SbAccessibilityDisplaySettings* out_settings);

  // Gets the platform's settings for system-level closed captions. This
  // function returns false if |caption_settings| is NULL or if it is not
  // zero-initialized.
  //
  // |caption_settings|: A pointer to a zero-initialized
  //    SbAccessibilityTextToSpeechSettings struct.
  bool (*GetCaptionSettings)(SbAccessibilityCaptionSettings* caption_settings);

  // Modifies whether closed captions are enabled at a system level. This
  // function returns false if this feature is not supported by the platform, or
  // if changing the setting is unsuccessful. This function will modify the
  // setting system-wide.
  //
  // |enabled|: A boolean indicating whether captions should be turned on (true)
  //    or off (false).
  bool (*SetCaptionsEnabled)(bool enabled);

  // This function, if implemented, will be called when the system's
  // text-to-speech settings have changed. This allows the application to
  // respond to user changes made in the system settings.
  void (*RegisterOnTextToSpeechStateChangedCallback)(
      SbOnTextToSpeechStateChangedCallback callback,
      void* context);
} StarboardExtensionAccessibilityApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_ACCESSIBILITY_H_
