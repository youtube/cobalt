// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Module Overview: Starboard speech recognizer module
//
// Defines a streaming speech recognizer API. It provides access to the platform
// speech recognition service.
//
// Note that there can be only one speech recognizer. Attempting to create a
// second speech recognizer without destroying the first one will result in
// undefined behavior.
//
// |SbSpeechRecognizerCreate|, |SbSpeechRecognizerStart|,
// |SbSpeechRecognizerStop|, |SbSpeechRecognizerCancel| and
// |SbSpeechRecognizerDestroy| should be called from a single thread. Callbacks
// defined in |SbSpeechRecognizerHandler| will happen on another thread, so
// calls back into the SbSpeechRecognizer from the callback thread are
// disallowed.

#ifndef STARBOARD_SPEECH_RECOGNIZER_H_
#define STARBOARD_SPEECH_RECOGNIZER_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#if SB_API_VERSION >= 13
#error Speech Recognizer is deprecated, switch to microphone implementation.
#endif

#if SB_API_VERSION >= 12 || SB_HAS(SPEECH_RECOGNIZER)

#ifdef __cplusplus
extern "C" {
#endif

// An opaque handle to an implementation-private structure that represents a
// speech recognizer.
typedef struct SbSpeechRecognizerPrivate* SbSpeechRecognizer;

// Well-defined value for an invalid speech recognizer handle.
#define kSbSpeechRecognizerInvalid ((SbSpeechRecognizer)NULL)

// Indicates whether the given speech recognizer is valid.
static SB_C_INLINE bool SbSpeechRecognizerIsValid(
    SbSpeechRecognizer recognizer) {
  return recognizer != kSbSpeechRecognizerInvalid;
}

// Indicates what has gone wrong with the recognition.
typedef enum SbSpeechRecognizerError {
  // No speech was detected. Speech timed out.
  kSbNoSpeechError,
  // Speech input was aborted somehow.
  kSbAborted,
  // Audio capture failed.
  kSbAudioCaptureError,
  // Some network communication that was required to complete the recognition
  // failed.
  kSbNetworkError,
  // The implementation is not allowing any speech input to occur for reasons of
  // security, privacy or user preference.
  kSbNotAllowed,
  // The implementation is not allowing the application requested speech
  // service, but would allow some speech service, to be used either because the
  // implementation doesn't support the selected one or because of reasons of
  // security, privacy or user preference.
  kSbServiceNotAllowed,
  // There was an error in the speech recognition grammar or semantic tags, or
  // the grammar format or semantic tag format is supported.
  kSbBadGrammar,
  // The language was not supported.
  kSbLanguageNotSupported,
} SbSpeechRecognizerError;

// The recognition response that is received from the recognizer.
typedef struct SbSpeechResult {
  // The raw words that the user spoke.
  char* transcript;
  // A numeric estimate between 0 and 1 of how confident the recognition system
  // is that the recognition is correct. A higher number means the system is
  // more confident. NaN represents an unavailable confidence score.
  float confidence;
} SbSpeechResult;

typedef struct SbSpeechConfiguration {
  // When the continuous value is set to false, the implementation MUST
  // return no more than one final result in response to starting recognition.
  // When the continuous attribute is set to true, the implementation MUST
  // return zero or more final results representing multiple consecutive
  // recognitions in response to starting recognition. This attribute setting
  // does not affect interim results.
  bool continuous;
  // Controls whether interim results are returned. When set to true, interim
  // results SHOULD be returned. When set to false, interim results MUST NOT be
  // returned. This value setting does not affect final results.
  bool interim_results;
  // This sets the maximum number of SbSpeechResult in
  // |SbSpeechRecognizerOnResults| callback.
  int max_alternatives;
} SbSpeechConfiguration;

// A function to notify that the user has started to speak or stops speaking.
// |detected|: true if the user has started to speak, and false if the user
// stops speaking.
typedef void (*SbSpeechRecognizerSpeechDetectedFunction)(void* context,
                                                         bool detected);

// A function to notify that a speech recognition error occurred.
// |error|: The occurred speech recognition error.
typedef void (*SbSpeechRecognizerErrorFunction)(void* context,
                                                SbSpeechRecognizerError error);

// A function to notify that the recognition results are ready.
// |results|: the list of recognition results.
// |results_size|: the number of |results|.
// |is_final|: indicates if the |results| is final.
typedef void (*SbSpeechRecognizerResultsFunction)(void* context,
                                                  SbSpeechResult* results,
                                                  int results_size,
                                                  bool is_final);

// Allows receiving notifications from the device when recognition related
// events occur.
//
// The void* context is passed to every function.
struct SbSpeechRecognizerHandler {
  // Function to notify the beginning/end of the speech.
  SbSpeechRecognizerSpeechDetectedFunction on_speech_detected;

  // Function to notify the speech error.
  SbSpeechRecognizerErrorFunction on_error;

  // Function to notify that the recognition results are available.
  SbSpeechRecognizerResultsFunction on_results;

  // This is passed to handler functions as first argument.
  void* context;
};

#if SB_API_VERSION >= 12
// Returns whether the platform supports SbSpeechRecognizer.
SB_EXPORT bool SbSpeechRecognizerIsSupported();
#endif

// Creates a speech recognizer with a speech recognizer handler.
//
// If the system has a speech recognition service available, this function
// returns the newly created handle.
//
// If no speech recognition service is available on the device, this function
// returns |kSbSpeechRecognizerInvalid|.
//
// |SbSpeechRecognizerCreate| does not expect the passed
// SbSpeechRecognizerHandler structure to live after |SbSpeechRecognizerCreate|
// is called, so the implementation must copy the contents if necessary.
SB_EXPORT SbSpeechRecognizer
SbSpeechRecognizerCreate(const SbSpeechRecognizerHandler* handler);

// Starts listening to audio and recognizing speech with the specified speech
// configuration. If |SbSpeechRecognizerStart| is called on an already
// started speech recognizer, the implementation MUST ignore the call and return
// false.
//
// Returns whether the speech recognizer is started successfully.
SB_EXPORT bool SbSpeechRecognizerStart(
    SbSpeechRecognizer recognizer,
    const SbSpeechConfiguration* configuration);

// Stops listening to audio and returns a result using just the audio that it
// has already received. Once |SbSpeechRecognizerStop| is called, the
// implementation MUST NOT collect additional audio and MUST NOT continue to
// listen to the user. This is important for privacy reasons. If
// |SbSpeechRecognizerStop| is called on a speech recognizer which is already
// stopped or being stopped, the implementation MUST ignore the call.
SB_EXPORT void SbSpeechRecognizerStop(SbSpeechRecognizer recognizer);

// Cancels speech recognition. The speech recognizer stops listening to
// audio and does not return any information. When |SbSpeechRecognizerCancel| is
// called, the implementation MUST NOT collect additional audio, MUST NOT
// continue to listen to the user, and MUST stop recognizing. This is important
// for privacy reasons. If |SbSpeechRecognizerCancel| is called on a speech
// recognizer which is already stopped or cancelled, the implementation MUST
// ignore the call.
SB_EXPORT void SbSpeechRecognizerCancel(SbSpeechRecognizer recognizer);

// Destroys the given speech recognizer. If the speech recognizer is in the
// started state, it is first stopped and then destroyed.
SB_EXPORT void SbSpeechRecognizerDestroy(SbSpeechRecognizer recognizer);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_API_VERSION >= 12 || SB_HAS(SPEECH_RECOGNIZER)

#endif  // STARBOARD_SPEECH_RECOGNIZER_H_
