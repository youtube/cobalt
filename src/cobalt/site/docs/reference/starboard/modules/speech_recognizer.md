---
layout: doc
title: "Starboard Module Reference: speech_recognizer.h"
---

Defines a streaming speech recognizer API. It provides access to the platform
speech recognition service.<br>
Note that there can be only one speech recognizer. Attempting to create a
second speech recognizer without destroying the first one will result in
undefined behavior.<br>
|SbSpeechRecognizerCreate|, |SbSpeechRecognizerStart|,
|SbSpeechRecognizerStop|, |SbSpeechRecognizerCancel| and
|SbSpeechRecognizerDestroy| should be called from a single thread. Callbacks
defined in |SbSpeechRecognizerHandler| will happen on another thread, so
calls back into the SbSpeechRecognizer from the callback thread are
disallowed.

## Enums

### SbSpeechRecognizerError

Indicates what has gone wrong with the recognition.

**Values**

*   `kSbNoSpeechError` - No speech was detected. Speech timed out.
*   `kSbAborted` - Speech input was aborted somehow.
*   `kSbAudioCaptureError` - Audio capture failed.
*   `kSbNetworkError` - Some network communication that was required to complete the recognitionfailed.
*   `kSbNotAllowed` - The implementation is not allowing any speech input to occur for reasons ofsecurity, privacy or user preference.
*   `kSbServiceNotAllowed` - The implementation is not allowing the application requested speechservice, but would allow some speech service, to be used either because theimplementation doesn't support the selected one or because of reasons ofsecurity, privacy or user preference.
*   `kSbBadGrammar` - There was an error in the speech recognition grammar or semantic tags, orthe grammar format or semantic tag format is supported.
*   `kSbLanguageNotSupported` - The language was not supported.

## Structs

### SbSpeechConfiguration

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>bool</code><br>        <code>continuous</code></td>    <td>When the continuous value is set to false, the implementation MUST
return no more than one final result in response to starting recognition.
When the continuous attribute is set to true, the implementation MUST
return zero or more final results representing multiple consecutive
recognitions in response to starting recognition. This attribute setting
does not affect interim results.</td>  </tr>
  <tr>
    <td><code>bool</code><br>        <code>interim_results</code></td>    <td>Controls whether interim results are returned. When set to true, interim
results SHOULD be returned. When set to false, interim results MUST NOT be
returned. This value setting does not affect final results.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>max_alternatives</code></td>    <td>This sets the maximum number of SbSpeechResult in
<code>SbSpeechRecognizerOnResults</code> callback.</td>  </tr>
</table>

### SbSpeechRecognizer

An opaque handle to an implementation-private structure that represents a
speech recognizer.

### SbSpeechResult

The recognition response that is received from the recognizer.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>float</code><br>        <code>confidence</code></td>    <td>The raw words that the user spoke.
A numeric estimate between 0 and 1 of how confident the recognition system
is that the recognition is correct. A higher number means the system is
more confident. NaN represents an unavailable confidence score.</td>  </tr>
</table>

## Functions

### SbSpeechRecognizerCancel

**Description**

Cancels speech recognition. The speech recognizer stops listening to
audio and does not return any information. When `SbSpeechRecognizerCancel` is
called, the implementation MUST NOT collect additional audio, MUST NOT
continue to listen to the user, and MUST stop recognizing. This is important
for privacy reasons. If `SbSpeechRecognizerCancel` is called on a speech
recognizer which is already stopped or cancelled, the implementation MUST
ignore the call.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSpeechRecognizerCancel-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSpeechRecognizerCancel-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSpeechRecognizerCancel-declaration">
<pre>
SB_EXPORT void SbSpeechRecognizerCancel(SbSpeechRecognizer recognizer);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSpeechRecognizerCancel-stub">

```
#include "starboard/speech_recognizer.h"

#if SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5

void SbSpeechRecognizerCancel(SbSpeechRecognizer /*recognizer*/) {}

#endif  // SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSpeechRecognizer</code><br>
        <code>recognizer</code></td>
    <td> </td>
  </tr>
</table>

### SbSpeechRecognizerCreate

**Description**

Creates a speech recognizer with a speech recognizer handler.<br>
If the system has a speech recognition service available, this function
returns the newly created handle.<br>
If no speech recognition service is available on the device, this function
returns `kSbSpeechRecognizerInvalid`.<br>
`SbSpeechRecognizerCreate` does not expect the passed
SbSpeechRecognizerHandler structure to live after `SbSpeechRecognizerCreate`
is called, so the implementation must copy the contents if necessary.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSpeechRecognizerCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSpeechRecognizerCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSpeechRecognizerCreate-declaration">
<pre>
SB_EXPORT SbSpeechRecognizer
SbSpeechRecognizerCreate(const SbSpeechRecognizerHandler* handler);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSpeechRecognizerCreate-stub">

```
#include "starboard/speech_recognizer.h"

#if SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5

SbSpeechRecognizer SbSpeechRecognizerCreate(
    const SbSpeechRecognizerHandler* /*handler*/) {
  return kSbSpeechRecognizerInvalid;
}

#endif  // SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const SbSpeechRecognizerHandler*</code><br>        <code>handler</code></td>
    <td> </td>  </tr>
</table>

### SbSpeechRecognizerDestroy

**Description**

Destroys the given speech recognizer. If the speech recognizer is in the
started state, it is first stopped and then destroyed.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSpeechRecognizerDestroy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSpeechRecognizerDestroy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSpeechRecognizerDestroy-declaration">
<pre>
SB_EXPORT void SbSpeechRecognizerDestroy(SbSpeechRecognizer recognizer);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSpeechRecognizerDestroy-stub">

```
#include "starboard/speech_recognizer.h"

#if SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5

void SbSpeechRecognizerDestroy(SbSpeechRecognizer /*recognizer*/) {}

#endif  // SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSpeechRecognizer</code><br>
        <code>recognizer</code></td>
    <td> </td>
  </tr>
</table>

### SbSpeechRecognizerIsValid

**Description**

Indicates whether the given speech recognizer is valid.

**Declaration**

```
static SB_C_INLINE bool SbSpeechRecognizerIsValid(
    SbSpeechRecognizer recognizer) {
  return recognizer != kSbSpeechRecognizerInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSpeechRecognizer</code><br>
        <code>recognizer</code></td>
    <td> </td>
  </tr>
</table>

### SbSpeechRecognizerStart

**Description**

Starts listening to audio and recognizing speech with the specified speech
configuration. If `SbSpeechRecognizerStart` is called on an already
started speech recognizer, the implementation MUST ignore the call and return
false.<br>
Returns whether the speech recognizer is started successfully.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSpeechRecognizerStart-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSpeechRecognizerStart-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSpeechRecognizerStart-declaration">
<pre>
SB_EXPORT bool SbSpeechRecognizerStart(
    SbSpeechRecognizer recognizer,
    const SbSpeechConfiguration* configuration);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSpeechRecognizerStart-stub">

```
#include "starboard/speech_recognizer.h"

#if SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5

bool SbSpeechRecognizerStart(SbSpeechRecognizer /*recognizer*/,
                             const SbSpeechConfiguration* /*configuration*/) {
  return false;
}

#endif  // SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSpeechRecognizer</code><br>
        <code>recognizer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const SbSpeechConfiguration*</code><br>
        <code>configuration</code></td>
    <td> </td>
  </tr>
</table>

### SbSpeechRecognizerStop

**Description**

Stops listening to audio and returns a result using just the audio that it
has already received. Once `SbSpeechRecognizerStop` is called, the
implementation MUST NOT collect additional audio and MUST NOT continue to
listen to the user. This is important for privacy reasons. If
`SbSpeechRecognizerStop` is called on a speech recognizer which is already
stopped or being stopped, the implementation MUST ignore the call.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSpeechRecognizerStop-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSpeechRecognizerStop-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSpeechRecognizerStop-declaration">
<pre>
SB_EXPORT void SbSpeechRecognizerStop(SbSpeechRecognizer recognizer);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSpeechRecognizerStop-stub">

```
#include "starboard/speech_recognizer.h"

#if SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5

void SbSpeechRecognizerStop(SbSpeechRecognizer /*recognizer*/) {}

#endif  // SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSpeechRecognizer</code><br>
        <code>recognizer</code></td>
    <td> </td>
  </tr>
</table>

