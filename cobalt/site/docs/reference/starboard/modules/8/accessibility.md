---
layout: doc
title: "Starboard Module Reference: accessibility.h"
---

Provides access to the system options and settings related to accessibility.

## Structs ##

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

