---
layout: doc
title: "Starboard Module Reference: speech_synthesis.h"
---

A basic text-to-speech API intended to be used for audio accessibilty.<br>
Implementations of this API should audibly play back text to assist
users in non-visual navigation of the application.<br>
Note that these functions do not have to be thread-safe. They must
only be called from a single application thread.

## Functions

### SbSpeechSynthesisCancel

**Description**

Cancels all speaking and queued speech synthesis audio. Must
return immediately.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSpeechSynthesisCancel-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSpeechSynthesisCancel-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSpeechSynthesisCancel-declaration">
<pre>
SB_EXPORT void SbSpeechSynthesisCancel();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSpeechSynthesisCancel-stub">

```
#include "starboard/speech_synthesis.h"

#if !SB_HAS(SPEECH_SYNTHESIS)
#error If speech synthesis not enabled on this platform, please exclude it\
       from the build
#endif

void SbSpeechSynthesisCancel() {}
```

  </div>
</div>

### SbSpeechSynthesisSpeak

**Description**

Enqueues `text`, a UTF-8 string, to be spoken.
Returns immediately.<br>
Spoken language for the text should be the same as the locale returned
by SbSystemGetLocaleId().<br>
If audio from previous <code>SbSpeechSynthesisSpeak()</code> invocations is still
processing, the current speaking should continue and this new
text should be queued to play when the previous utterances are complete.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSpeechSynthesisSpeak-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSpeechSynthesisSpeak-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSpeechSynthesisSpeak-declaration">
<pre>
SB_EXPORT void SbSpeechSynthesisSpeak(const char* text);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSpeechSynthesisSpeak-stub">

```
#include "starboard/speech_synthesis.h"

#if !SB_HAS(SPEECH_SYNTHESIS)
#error If speech synthesis not enabled on this platform, please exclude it\
       from the build
#endif

void SbSpeechSynthesisSpeak(const char* text) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>
        <code>text</code></td>
    <td> </td>
  </tr>
</table>

