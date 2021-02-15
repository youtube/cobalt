---
layout: doc
title: "Starboard Module Reference: accessibility.h"
---

Provides access to the system options and settings related to accessibility.

## Enums ##

### SbAccessibilityCaptionCharacterEdgeStyle ###

Enum for possible closed captioning character edge styles.

#### Values ####

*   `kSbAccessibilityCaptionCharacterEdgeStyleNone`
*   `kSbAccessibilityCaptionCharacterEdgeStyleRaised`
*   `kSbAccessibilityCaptionCharacterEdgeStyleDepressed`
*   `kSbAccessibilityCaptionCharacterEdgeStyleUniform`
*   `kSbAccessibilityCaptionCharacterEdgeStyleDropShadow`

### SbAccessibilityCaptionColor ###

Enum for possible closed captioning colors.

#### Values ####

*   `kSbAccessibilityCaptionColorBlue`
*   `kSbAccessibilityCaptionColorBlack`
*   `kSbAccessibilityCaptionColorCyan`
*   `kSbAccessibilityCaptionColorGreen`
*   `kSbAccessibilityCaptionColorMagenta`
*   `kSbAccessibilityCaptionColorRed`
*   `kSbAccessibilityCaptionColorWhite`
*   `kSbAccessibilityCaptionColorYellow`

### SbAccessibilityCaptionFontFamily ###

Enum for possible closed captioning font families

#### Values ####

*   `kSbAccessibilityCaptionFontFamilyCasual`
*   `kSbAccessibilityCaptionFontFamilyCursive`
*   `kSbAccessibilityCaptionFontFamilyMonospaceSansSerif`
*   `kSbAccessibilityCaptionFontFamilyMonospaceSerif`
*   `kSbAccessibilityCaptionFontFamilyProportionalSansSerif`
*   `kSbAccessibilityCaptionFontFamilyProportionalSerif`
*   `kSbAccessibilityCaptionFontFamilySmallCapitals`

### SbAccessibilityCaptionFontSizePercentage ###

Enum for possible closed captioning font size percentages.

#### Values ####

*   `kSbAccessibilityCaptionFontSizePercentage25`
*   `kSbAccessibilityCaptionFontSizePercentage50`
*   `kSbAccessibilityCaptionFontSizePercentage75`
*   `kSbAccessibilityCaptionFontSizePercentage100`
*   `kSbAccessibilityCaptionFontSizePercentage125`
*   `kSbAccessibilityCaptionFontSizePercentage150`
*   `kSbAccessibilityCaptionFontSizePercentage175`
*   `kSbAccessibilityCaptionFontSizePercentage200`
*   `kSbAccessibilityCaptionFontSizePercentage225`
*   `kSbAccessibilityCaptionFontSizePercentage250`
*   `kSbAccessibilityCaptionFontSizePercentage275`
*   `kSbAccessibilityCaptionFontSizePercentage300`

### SbAccessibilityCaptionOpacityPercentage ###

Enum for possible closed captioning opacity percentages.

#### Values ####

*   `kSbAccessibilityCaptionOpacityPercentage0`
*   `kSbAccessibilityCaptionOpacityPercentage25`
*   `kSbAccessibilityCaptionOpacityPercentage50`
*   `kSbAccessibilityCaptionOpacityPercentage75`
*   `kSbAccessibilityCaptionOpacityPercentage100`

### SbAccessibilityCaptionState ###

Enum for possible states of closed captioning properties.

#### Values ####

*   `kSbAccessibilityCaptionStateUnsupported`

    The property is not supported by the system. The application should provide
    a way to set this property, otherwise it will not be changeable. For any
    given closed captioning property, if its corresponding state property has a
    value of `kSbAccessibilityCaptionStateUnsupported`, then its own value is
    undefined. For example, if
    `SbAccessibilityCaptionColor::background_color_state` has a value of
    `kSbAccessibilityCaptionStateUnsupported`, then the value of
    `SbAccessibilityCaptionColor::background_color` is undefined.
*   `kSbAccessibilityCaptionStateUnset`

    The property is supported by the system, but the user has not set it. The
    application should provide a default setting for the property to handle this
    case.
*   `kSbAccessibilityCaptionStateSet`

    The user has set this property as a system default, meaning that it should
    take priority over app defaults. If
    SbAccessibilityCaptionSettings.supportsOverride contains true, this value
    should be interpreted as explicitly saying "do not override." If it contains
    false, it is up to the application to interpret any additional meaning of
    this value.
*   `kSbAccessibilityCaptionStateOverride`

    This property should take priority over everything but application-level
    overrides, including video caption data. If
    SbAccessibilityCaptionSettings.supportsOverride contains false, then no
    fields of SbAccessibilityCaptionSettings will ever contain this value.

## Structs ##

### SbAccessibilityCaptionSettings ###

A group of settings related to system-level closed captioning settings, for
platforms that expose closed captioning settings.

#### Members ####

*   `SbAccessibilityCaptionColor background_color`
*   `SbAccessibilityCaptionState background_color_state`
*   `SbAccessibilityCaptionOpacityPercentage background_opacity`
*   `SbAccessibilityCaptionState background_opacity_state`
*   `SbAccessibilityCaptionCharacterEdgeStyle character_edge_style`
*   `SbAccessibilityCaptionState character_edge_style_state`
*   `SbAccessibilityCaptionColor font_color`
*   `SbAccessibilityCaptionState font_color_state`
*   `SbAccessibilityCaptionFontFamily font_family`
*   `SbAccessibilityCaptionState font_family_state`
*   `SbAccessibilityCaptionOpacityPercentage font_opacity`
*   `SbAccessibilityCaptionState font_opacity_state`
*   `SbAccessibilityCaptionFontSizePercentage font_size`
*   `SbAccessibilityCaptionState font_size_state`
*   `SbAccessibilityCaptionColor window_color`
*   `SbAccessibilityCaptionState window_color_state`
*   `SbAccessibilityCaptionOpacityPercentage window_opacity`
*   `SbAccessibilityCaptionState window_opacity_state`
*   `bool is_enabled`

    The `is_enabled` attribute determines if the user has chosen to enable
    closed captions on their system.
*   `bool supports_is_enabled`

    Some platforms support enabling or disabling captions, some support reading
    whether they are enabled from the system settings, and others support
    neither. As a result, there are separate checks for getting and setting the
    value that is contained in the `is_enabled` attribute. Modifying the
    attribute via `SbAccessibilitySetCaptionsEnabled` will change the setting
    system-wide. Attempting to read `is_enabled` when the value of
    `supports_is_enabled` is false will always return false. Attempting to set
    `is_enabled` via `SbAccessibilitySetCaptionsEnabled` when the value of
    `supports_set_enabled` is false will fail silently.
*   `bool supports_set_enabled`
*   `bool supports_override`

    Some platforms may specify that when setting a property, it should override
    data from video streams and application settings (unless the application has
    its own overrides). Depending on whether this attribute contains true or
    false, the values of `SbAccessibilityCaptionState` should be interpreted
    differently.

### SbAccessibilityDisplaySettings ###

#### Members ####

*   `bool has_high_contrast_text_setting`

    Whether this platform has a system setting for high contrast text or not.
*   `bool is_high_contrast_text_enabled`

    Whether the high contrast text setting is enabled or not.

### SbAccessibilityTextToSpeechSettings ###

A group of settings related to text-to-speech functionality, for platforms that
expose system settings for text-to-speech.

#### Members ####

*   `bool has_text_to_speech_setting`

    Whether this platform has a system setting for text-to-speech or not.
*   `bool is_text_to_speech_enabled`

    Whether the text-to-speech setting is enabled or not. This setting is only
    valid if `has_text_to_speech_setting` is set to true.

## Functions ##

### SbAccessibilityGetCaptionSettings ###

Get the platform's settings for system-level closed captions. This function
returns false if `caption_settings` is NULL or if it is not zero-initialized.

`caption_settings`: A pointer to a zero-initialized
SbAccessibilityTextToSpeechSettings struct.

#### Declaration ####

```
bool SbAccessibilityGetCaptionSettings(SbAccessibilityCaptionSettings *caption_settings)
```

### SbAccessibilityGetDisplaySettings ###

Get the platform settings related to high contrast text. This function returns
false if `out_settings` is NULL or if it is not zero-initialized.

`out_settings`: A pointer to a zero-initialized SbAccessibilityDisplaySettings*
struct.

#### Declaration ####

```
bool SbAccessibilityGetDisplaySettings(SbAccessibilityDisplaySettings *out_settings)
```

### SbAccessibilityGetTextToSpeechSettings ###

Get the platform settings related to the text-to-speech accessibility feature.
This function returns false if `out_settings` is NULL or if it is not zero-
initialized.

`out_settings`: A pointer to a zero-initialized
SbAccessibilityTextToSpeechSettings struct.

#### Declaration ####

```
bool SbAccessibilityGetTextToSpeechSettings(SbAccessibilityTextToSpeechSettings *out_settings)
```

### SbAccessibilitySetCaptionsEnabled ###

Modifies whether closed captions are enabled at a system level. This function
returns false if this feature is not supported by the platform, or if changing
the setting is unsuccessful. This function will modify the setting system-wide.

`enabled`: A boolean indicating whether captions should be turned on (true) or
off (false).

#### Declaration ####

```
bool SbAccessibilitySetCaptionsEnabled(bool enabled)
```
