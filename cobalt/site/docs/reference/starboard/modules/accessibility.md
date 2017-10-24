---
layout: doc
title: "Starboard Module Reference: accessibility.h"
---

Provides access to the system options and settings related to accessibility.

## Structs

### SbAccessibilityDisplaySettings

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>bool</code><br>        <code>has_high_contrast_text_setting</code></td>    <td>Whether this platform has a system setting for high contrast text or not.</td>  </tr>
  <tr>
    <td><code>bool</code><br>        <code>is_high_contrast_text_enabled</code></td>    <td>Whether the high contrast text setting is enabled or not.</td>  </tr>
</table>

### SbAccessibilityTextToSpeechSettings

A group of settings related to text-to-speech functionality, for platforms
that expose system settings for text-to-speech.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>bool</code><br>        <code>has_text_to_speech_setting</code></td>    <td>Whether this platform has a system setting for text-to-speech or not.</td>  </tr>
  <tr>
    <td><code>bool</code><br>        <code>is_text_to_speech_enabled</code></td>    <td>Whether the text-to-speech setting is enabled or not. This setting is only
valid if <code>has_text_to_speech_setting</code> is set to true.</td>  </tr>
</table>

## Functions

### SbAccessibilityGetDisplaySettings

**Description**

Get the platform settings related to high contrast text.
This function returns false if `out_settings` is NULL or if it is
not zero-initialized.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbAccessibilityGetDisplaySettings-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbAccessibilityGetDisplaySettings-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbAccessibilityGetDisplaySettings-declaration">
<pre>
SB_EXPORT bool SbAccessibilityGetDisplaySettings(
    SbAccessibilityDisplaySettings* out_settings);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbAccessibilityGetDisplaySettings-stub">

```
#include "starboard/accessibility.h"

#include "starboard/memory.h"

bool SbAccessibilityGetDisplaySettings(
    SbAccessibilityDisplaySettings* out_setting) {
  if (!out_setting ||
      !SbMemoryIsZero(out_setting,
                      sizeof(SbAccessibilityDisplaySettings))) {
    return false;
  }
  out_setting->has_high_contrast_text_setting = false;
  return true;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbAccessibilityDisplaySettings*</code><br>        <code>out_settings</code></td>
    <td>A pointer to a zero-initialized
SbAccessibilityDisplaySettings* struct.</td>
  </tr>
</table>

### SbAccessibilityGetTextToSpeechSettings

**Description**

Get the platform settings related to the text-to-speech accessibility
feature. This function returns false if `out_settings` is NULL or if it is
not zero-initialized.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbAccessibilityGetTextToSpeechSettings-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbAccessibilityGetTextToSpeechSettings-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbAccessibilityGetTextToSpeechSettings-declaration">
<pre>
SB_EXPORT bool SbAccessibilityGetTextToSpeechSettings(
    SbAccessibilityTextToSpeechSettings* out_settings);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbAccessibilityGetTextToSpeechSettings-stub">

```
#include "starboard/accessibility.h"

#include "starboard/memory.h"

bool SbAccessibilityGetTextToSpeechSettings(
    SbAccessibilityTextToSpeechSettings* out_setting) {
  if (!out_setting ||
      !SbMemoryIsZero(out_setting,
                      sizeof(SbAccessibilityTextToSpeechSettings))) {
    return false;
  }
  out_setting->has_text_to_speech_setting = false;
  return true;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbAccessibilityTextToSpeechSettings*</code><br>        <code>out_settings</code></td>
    <td>A pointer to a zero-initialized
SbAccessibilityTextToSpeechSettings struct.</td>
  </tr>
</table>

