---
layout: doc
title: "Starboard Module Reference: speech_synthesis.h"
---

A basic text-to-speech API intended to be used for audio accessibilty.

Implementations of this API should audibly play back text to assist users in
non-visual navigation of the application.

Note that these functions do not have to be thread-safe. They must only be
called from a single application thread.

## Functions ##

### SbSpeechSynthesisCancel ###

Cancels all speaking and queued speech synthesis audio. Must return immediately.

#### Declaration ####

```
void SbSpeechSynthesisCancel()
```

### SbSpeechSynthesisSpeak ###

Enqueues `text`, a UTF-8 string, to be spoken. Returns immediately.

Spoken language for the text should be the same as the locale returned by
SbSystemGetLocaleId().

If audio from previous SbSpeechSynthesisSpeak() invocations is still processing,
the current speaking should continue and this new text should be queued to play
when the previous utterances are complete.

#### Declaration ####

```
void SbSpeechSynthesisSpeak(const char *text)
```

