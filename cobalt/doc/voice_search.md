# Enabling voice search in Cobalt

Cobalt enables voice search through either:

1. A subset of the [MediaRecorder Web API](https://www.w3.org/TR/mediastream-recording/#mediarecorder-api).
2. A subset of the [Speech Recognition Web API](https://w3c.github.io/speech-api/#speechreco-section)

Only one or the other can be used, and we recommend that the MediaRecorder API
is followed, as we are considering deprecating the Speech Recognition API.

In both approaches, in order to check whether to enable voice control or not,
web apps will call the [MediaDevices.enumerateDevices()](https://www.w3.org/TR/mediacapture-streams/#dom-mediadevices-enumeratedevices%28%29)
Web API function within which Cobalt will in turn call a subset of the
[Starboard SbMicrophone API](../../starboard/microphone.h).  In both approaches,
`SB_HAS_MICROPHONE` must be defined to 1.

## MediaRecorder API

To enable the MediaRecorder API in Cobalt, the complete
[SbMicrophone API](../../starboard/microphone.h) must be implemented, and
the `SB_HAS_SPEECH_RECOGNIZER` must be defined to 0.

## Speech Recognition API

In order to provide support for using this API, platforms must implement the
[Starboard SbSpeechRecognizer API](../../starboard/speech_recognizer.h) as well
as a subset of the [SbMicrophone API](../../starboard/microphone.h).

### Specific instructions to enable voice search

1. Define the `SB_HAS_SPEECH_RECOGNIZER` flag to have the value 1 in your
   platform's `configuration_public.h` file, and implement the
   [SbSpeechRecognizer API](../../starboard/speech_recognizer.h).
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
4. With `SB_HAS_SPEECH_RECOGNIZER` defined to 1, Cobalt will use the platform's
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
