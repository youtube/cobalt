---
layout: doc
title: "Enabling voice search in Cobalt"
---
# Enabling voice search in Cobalt

Cobalt enables voice search through a subset of the
[MediaRecorder Web API](https://www.w3.org/TR/mediastream-recording/#mediarecorder-api)

In order to check whether to enable voice control or not, web apps will call the
[MediaDevices.enumerateDevices()](https://www.w3.org/TR/mediacapture-streams/#dom-mediadevices-enumeratedevices%28%29)
Web API function within which Cobalt will in turn call a subset of the
[Starboard SbMicrophone API](../../starboard/microphone.h).

Partners can add microphone support and microphone gesture options using the
optional SoftMicPlatformService, detailed below.

## MediaRecorder API

To enable the MediaRecorder API in Cobalt, the complete
[SbMicrophone API](../../starboard/microphone.h) must be implemented.

Web applications are expected to use the MediaRecorder API. This in turn relies
on the SbMicrophone API as detailed above.

## SoftMicPlatformService

In `starboard/linux/shared/soft_mic_platform_service.cc` there is an example
stub implementation of the SoftMicPlatformService. Platforms can optionally
implement this [CobaltPlatformService](https://cobalt.dev/gen/cobalt/doc/\
platform_services.html) to specify if they support the `soft mic` and/or `hard mic`
for voice search. The `soft mic` refers to the software activation of the microphone
for voice search through the UI microphone button on the Youtube Web Application
search page. The `hard mic` refers to hardware button activation of the microphone
for voice search. Platforms can also specify the optional `micGesture`. This
specifies the type of UI prompt the YouTube Web Application should display to guide
the user to start voice search. The options include an empty or `null` value for no
prompt, `"TAP"` for tap the `soft mic` and/or `hard mic` to start voice search, or
`"HOLD"` for hold the `soft mic` and/or the `hard mic` to start voice search.

The Web Application messages to the platform will be singular strings, encoded with
enclosing quotation marks to make them JSON compliant:

```
"\"notifySearchActive\""


"\"notifySearchInactive\""
```

These messages notify the platform when the user is entering or exiting the Youtube
Web Application search page. Only a synchronous `true` or `false` response is sent
from the platform to confirm that the message was correctly received and parsed.

```
"\"getMicSupport\""
```

A similar synchronous `true` or `false` response is sent from the platform confirming
the message was correctly received and parsed. The platform will also send an
asynchronous string encoded JSON object with the above mentioned microphone
preferences:

```
"{
    'hasHardMicSupport' : boolean,
    'hasSoftMicSupport' : boolean,
    'micGesture' : string,
 }"
```
