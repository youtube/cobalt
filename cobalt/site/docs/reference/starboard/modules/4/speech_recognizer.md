---
layout: doc
title: "Starboard Module Reference: speech_recognizer.h"
---

Defines a streaming speech recognizer API. It provides access to the platform
speech recognition service.

Note that there can be only one speech recognizer. Attempting to create a second
speech recognizer without destroying the first one will result in undefined
behavior.

`SbSpeechRecognizerCreate`, `SbSpeechRecognizerStart`, `SbSpeechRecognizerStop`,
`SbSpeechRecognizerCancel` and `SbSpeechRecognizerDestroy` should be called from
a single thread. Callbacks defined in `SbSpeechRecognizerHandler` will happen on
another thread, so calls back into the SbSpeechRecognizer from the callback
thread are disallowed.
