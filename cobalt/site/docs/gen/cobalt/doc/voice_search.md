---
layout: doc
title: "Enabling voice search in Cobalt"
---
# Enabling voice search in Cobalt

Cobalt enables voice search through either:

1. A subset of the [MediaRecorder Web API](https://www.w3.org/TR/mediastream-recording/#mediarecorder-api).
2. A subset of the [Speech Recognition Web API](https://w3c.github.io/speech-api/#speechreco-section)

Only one or the other can be used, and we recommend that the MediaRecorder API
is followed, as we are considering deprecating the Speech Recognition API.

**The Speech Recognition API is deprecated as of Starboard 13.**

In both approaches, in order to check whether to enable voice control or not,
web apps will call the [MediaDevices.enumerateDevices()](https://www.w3.org/TR/mediacapture-streams/#dom-mediadevices-enumeratedevices%28%29)
Web API function within which Cobalt will in turn call a subset of the
[Starboard SbMicrophone API](../../starboard/microphone.h).

## MediaRecorder API

To enable the MediaRecorder API in Cobalt, the complete
[SbMicrophone API](../../starboard/microphone.h) must be implemented, and
`SbSpeechRecognizerIsSupported()` must return `false`.

## Speech Recognition API - Deprecated

**The Speech Recognition API is deprecated as of Starboard 13.**

In order to provide support for using this API, platforms must implement the
[Starboard SbSpeechRecognizer API](../../starboard/speech_recognizer.h) as well
as a subset of the [SbMicrophone API](../../starboard/microphone.h).

### Specific instructions to enable voice search

1. Implement `SbSpeechRecognizerIsSupported()` to return `true`, and implement
   the [SbSpeechRecognizer API](../../starboard/speech_recognizer.h).
2. Implement the following subset of the
   [SbMicrophone API](../../starboard/microphone.h):
    - `SbMicrophoneGetAvailable()`
    - `SbMicrophoneCreate()`
    - `SbMicrophoneDestroy()`

   In particular, SbMicrophoneCreate() must return a valid microphone.  It is
   okay to stub out the other functions, e.g. have `SbMicrophoneOpen()`
   return `false`.
3. The YouTube app will display the mic icon on the search page when it detects
   valid microphone input devices using `MediaDevices.enumerateDevices()`.
4. With `SbSpeechRecognizerIsSupported()` implemented to return `true`, Cobalt
   will use the platform's
   [Starboard SbSpeechRecognizer API](../../starboard/speech_recognizer.h)
   implementation, and it will not actually read directly from the microphone
   via the [Starboard SbMicrophone API](../../starboard/microphone.h).

### Differences from versions of Cobalt <= 11

In previous versions of Cobalt, there was no way to dynamically disable
speech support besides modifying common Cobalt code to dynamically stub out the
Speech Recognition API when the platform does not support microphone input.
This is no longer necessary, web apps should now rely on
`MediaDevices.enumerateDevices()` to determine whether voice support is enabled
or not.

### Speech Recognition API is deprecated in Starboard 13 ###

Web applications are expected to use the MediaRecorder API. This in turn relies
on the SbMicrophone API as detailed above.
