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

## Macros ##

### kSbSpeechRecognizerInvalid ###

Well-defined value for an invalid speech recognizer handle.

## Enums ##

### SbSpeechRecognizerError ###

Indicates what has gone wrong with the recognition.

#### Values ####

*   `kSbNoSpeechError`

    No speech was detected. Speech timed out.
*   `kSbAborted`

    Speech input was aborted somehow.
*   `kSbAudioCaptureError`

    Audio capture failed.
*   `kSbNetworkError`

    Some network communication that was required to complete the recognition
    failed.
*   `kSbNotAllowed`

    The implementation is not allowing any speech input to occur for reasons of
    security, privacy or user preference.
*   `kSbServiceNotAllowed`

    The implementation is not allowing the application requested speech service,
    but would allow some speech service, to be used either because the
    implementation doesn't support the selected one or because of reasons of
    security, privacy or user preference.
*   `kSbBadGrammar`

    There was an error in the speech recognition grammar or semantic tags, or
    the grammar format or semantic tag format is supported.
*   `kSbLanguageNotSupported`

    The language was not supported.

## Typedefs ##

### SbSpeechRecognizer ###

An opaque handle to an implementation-private structure that represents a speech
recognizer.

#### Definition ####

```
typedef struct SbSpeechRecognizerPrivate* SbSpeechRecognizer
```

### SbSpeechRecognizerErrorFunction ###

A function to notify that a speech recognition error occurred. `error`: The
occurred speech recognition error.

#### Definition ####

```
typedef void(* SbSpeechRecognizerErrorFunction)(void *context, SbSpeechRecognizerError error)
```

### SbSpeechRecognizerResultsFunction ###

A function to notify that the recognition results are ready. `results`: the list
of recognition results. `results_size`: the number of `results`. `is_final`:
indicates if the `results` is final.

#### Definition ####

```
typedef void(* SbSpeechRecognizerResultsFunction)(void *context, SbSpeechResult *results, int results_size, bool is_final)
```

### SbSpeechRecognizerSpeechDetectedFunction ###

A function to notify that the user has started to speak or stops speaking.
`detected`: true if the user has started to speak, and false if the user stops
speaking.

#### Definition ####

```
typedef void(* SbSpeechRecognizerSpeechDetectedFunction)(void *context, bool detected)
```

## Structs ##

### SbSpeechConfiguration ###

#### Members ####

*   `bool continuous`

    When the continuous value is set to false, the implementation MUST return no
    more than one final result in response to starting recognition. When the
    continuous attribute is set to true, the implementation MUST return zero or
    more final results representing multiple consecutive recognitions in
    response to starting recognition. This attribute setting does not affect
    interim results.
*   `bool interim_results`

    Controls whether interim results are returned. When set to true, interim
    results SHOULD be returned. When set to false, interim results MUST NOT be
    returned. This value setting does not affect final results.
*   `int max_alternatives`

    This sets the maximum number of SbSpeechResult in
    `SbSpeechRecognizerOnResults` callback.

### SbSpeechRecognizerHandler ###

Allows receiving notifications from the device when recognition related events
occur.

The void* context is passed to every function.

#### Members ####

*   `SbSpeechRecognizerSpeechDetectedFunction on_speech_detected`

    Function to notify the beginning/end of the speech.
*   `SbSpeechRecognizerErrorFunction on_error`

    Function to notify the speech error.
*   `SbSpeechRecognizerResultsFunction on_results`

    Function to notify that the recognition results are available.
*   `void * context`

    This is passed to handler functions as first argument.

### SbSpeechResult ###

The recognition response that is received from the recognizer.

#### Members ####

*   `char * transcript`

    The raw words that the user spoke.
*   `float confidence`

    A numeric estimate between 0 and 1 of how confident the recognition system
    is that the recognition is correct. A higher number means the system is more
    confident. NaN represents an unavailable confidence score.

## Functions ##

### SbSpeechRecognizerCancel ###

Cancels speech recognition. The speech recognizer stops listening to audio and
does not return any information. When `SbSpeechRecognizerCancel` is called, the
implementation MUST NOT collect additional audio, MUST NOT continue to listen to
the user, and MUST stop recognizing. This is important for privacy reasons. If
`SbSpeechRecognizerCancel` is called on a speech recognizer which is already
stopped or cancelled, the implementation MUST ignore the call.

#### Declaration ####

```
void SbSpeechRecognizerCancel(SbSpeechRecognizer recognizer)
```

### SbSpeechRecognizerCreate ###

Creates a speech recognizer with a speech recognizer handler.

If the system has a speech recognition service available, this function returns
the newly created handle.

If no speech recognition service is available on the device, this function
returns `kSbSpeechRecognizerInvalid`.

`SbSpeechRecognizerCreate` does not expect the passed SbSpeechRecognizerHandler
structure to live after `SbSpeechRecognizerCreate` is called, so the
implementation must copy the contents if necessary.

#### Declaration ####

```
SbSpeechRecognizer SbSpeechRecognizerCreate(const SbSpeechRecognizerHandler *handler)
```

### SbSpeechRecognizerDestroy ###

Destroys the given speech recognizer. If the speech recognizer is in the started
state, it is first stopped and then destroyed.

#### Declaration ####

```
void SbSpeechRecognizerDestroy(SbSpeechRecognizer recognizer)
```

### SbSpeechRecognizerIsValid ###

Indicates whether the given speech recognizer is valid.

#### Declaration ####

```
static bool SbSpeechRecognizerIsValid(SbSpeechRecognizer recognizer)
```

### SbSpeechRecognizerStart ###

Starts listening to audio and recognizing speech with the specified speech
configuration. If `SbSpeechRecognizerStart` is called on an already started
speech recognizer, the implementation MUST ignore the call and return false.

Returns whether the speech recognizer is started successfully.

#### Declaration ####

```
bool SbSpeechRecognizerStart(SbSpeechRecognizer recognizer, const SbSpeechConfiguration *configuration)
```

### SbSpeechRecognizerStop ###

Stops listening to audio and returns a result using just the audio that it has
already received. Once `SbSpeechRecognizerStop` is called, the implementation
MUST NOT collect additional audio and MUST NOT continue to listen to the user.
This is important for privacy reasons. If `SbSpeechRecognizerStop` is called on
a speech recognizer which is already stopped or being stopped, the
implementation MUST ignore the call.

#### Declaration ####

```
void SbSpeechRecognizerStop(SbSpeechRecognizer recognizer)
```

