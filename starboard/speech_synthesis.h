// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Speech Synthesis API
//
// A basic text-to-speech API intended to be used for audio accessibility.
//
// Implementations of this API should audibly play back text to assist
// users in non-visual navigation of the application.
//
// Note that these functions do not have to be thread-safe. They must
// only be called from a single application thread.

#ifndef STARBOARD_SPEECH_SYNTHESIS_H_
#define STARBOARD_SPEECH_SYNTHESIS_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#if SB_API_VERSION >= 12 || SB_HAS(SPEECH_SYNTHESIS)

#ifdef __cplusplus
extern "C" {
#endif

#if SB_API_VERSION >= 12
// Returns whether the platform supports speech synthesis
SB_EXPORT bool SbSpeechSynthesisIsSupported();
#endif

// Enqueues |text|, a UTF-8 string, to be spoken.
// Returns immediately.
//
// Spoken language for the text should be the same as the locale returned
// by SbSystemGetLocaleId().
//
// If audio from previous SbSpeechSynthesisSpeak() invocations is still
// processing, the current speaking should continue and this new
// text should be queued to play when the previous utterances are complete.
SB_EXPORT void SbSpeechSynthesisSpeak(const char* text);

// Cancels all speaking and queued speech synthesis audio. Must
// return immediately.
SB_EXPORT void SbSpeechSynthesisCancel();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(SPEECH_SYNTHESIS)

#endif  // STARBOARD_SPEECH_SYNTHESIS_H_
