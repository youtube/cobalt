// Copyright 2016 Google Inc. All Rights Reserved.
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

// Microphone creation, control, audio data fetching and destruction.
// Multiple calls to |SbMicrophoneOpen| and |SbMicrophoneClose| are allowed, and
// the implementation also should take care of same calls of open and close in a
// row on a microphone.
// This API is not threadsafe and must be called from a single thread.
//
// How to use this API:
// 1) |SbMicrophoneGetAvailableInfos| to get a list of available microphone
//    information.
// 2) Choose one to create microphone |SbMicrophoneCreate| with enough buffer
//    size and sample rate. The sample rate can be verified by
//    |SbMicrophoneIsSampleRateSupported|.
// 3) Open the microphone port and start recording audio data by
//    |SbMicrophoneOpen|.
// 4) Periodically read out the data from microphone by |SbMicrophoneRead|.
// 5) Close the microphone port and stop recording audio data by
//    |SbMicrophoneClose|.
// 6) Destroy the microphone |SbMicrophoneDestroy|.

#ifndef STARBOARD_MICROPHONE_H_
#define STARBOARD_MICROPHONE_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#if SB_HAS(MICROPHONE) && SB_VERSION(2)

#ifdef __cplusplus
extern "C" {
#endif

// All possible microphone types.
typedef enum SbMicrophoneType {
  // Build-in microphone in camera.
  kSbMicrophoneCamera,

  // Microphone in the headset which can be wire or wireless USB headset.
  kSbMicrophoneUSBHeadset,

  // Microphone in the VR headset.
  kSbMicrophoneVRHeadset,

  // Microphone in the analog headset.
  kSBMicrophoneAnalogHeadset,

  // Unknown microphone type. Microphone other than those listed or could be
  // either of those listed.
  kSbMicrophoneUnknown,
} SbMicrophoneType;

// An opaque handle to an implementation-private structure representing a
// microphone id.
typedef struct SbMicrophoneIdPrivate* SbMicrophoneId;

// Well-defined value for an invalid microphone id handle.
#define kSbMicrophoneIdInvalid ((SbMicrophoneId)NULL)

// Returns whether the given microphone id is valid.
static SB_C_INLINE bool SbMicrophoneIdIsValid(SbMicrophoneId id) {
  return id != kSbMicrophoneIdInvalid;
}

// Microphone information.
typedef struct SbMicrophoneInfo {
  // Microphone id.
  SbMicrophoneId id;

  // Microphone type.
  SbMicrophoneType type;

  // Microphone max supported sampling rate.
  int max_sample_rate_hz;
} SbMicrophoneInfo;

// An opaque handle to an implementation-private structure representing a
// microphone.
typedef struct SbMicrophonePrivate* SbMicrophone;

// Well-defined value for an invalid microphone handle.
#define kSbMicrophoneInvalid ((SbMicrophone)NULL)

// Returns whether the given microphone is valid.
static SB_C_INLINE bool SbMicrophoneIsValid(SbMicrophone microphone) {
  return microphone != kSbMicrophoneInvalid;
}

// Gets all currently-available microphone information and the results are
// stored in |out_info_array|. |info_array_size| is the size of
// |out_info_array|.
// Return value is the number of the available microphones. A negative return
// value indicates that either the |info_array_size| is too small or an internal
// error is occurred.
SB_EXPORT int SbMicrophoneGetAvailable(SbMicrophoneInfo* out_info_array,
                                       int info_array_size);

// Returns true if the sample rate is supported by the microphone.
SB_EXPORT bool SbMicrophoneIsSampleRateSupported(SbMicrophoneId id,
                                                 int sample_rate_in_hz);

// Creates a microphone with |id|, audio sample rate in HZ, and the size of
// the cached audio buffer.
//
// If you try to create a microphone that has already been initialized or
// the sample rate is unavailable or the buffer size is invalid, it should
// return |kSbMicrophoneInvalid|. |buffer_size_bytes| is the size of the buffer
// where signed 16-bit integer audio data is temporarily cached to during the
// capturing. The audio data will be removed from the audio buffer if it has
// been read. New audio data can be read from this buffer in smaller chunks than
// this size. |buffer_size_bytes| must be set to a value greater than zero and
// the ideal size is 2^n. We only require support for creating one microphone at
// a time, and that implementations may return an error if a second microphone
// is created before destroying the first.
SB_EXPORT SbMicrophone SbMicrophoneCreate(SbMicrophoneId id,
                                          int sample_rate_in_hz,
                                          int buffer_size_bytes);

// Opens the microphone port and starts recording audio on |microphone|.
//
// Once started, the client will have to periodically call |SbMicrophoneRead| to
// receive the audio data. If the microphone has already been started, this call
// will clear the unread buffer. The return value indicates if the microphone is
// open.
SB_EXPORT bool SbMicrophoneOpen(SbMicrophone microphone);

// Closes the microphone port and stops recording audio on |microphone|.
//
// Clear the unread buffer if it is not empty. If the microphone has already
// been stopped, this call would be ignored. The return value indicates if the
// microphone is closed.
SB_EXPORT bool SbMicrophoneClose(SbMicrophone microphone);

// Gets the recorded audio data from the microphone.
//
// |out_audio_data| is where the recorded audio data is written to.
// |audio_data_size| is the number of requested bytes. The return value is zero
// or the positive number of bytes that were read. Neither the return value nor
// |audio_data_size| exceeds the buffer size. Negative return value indicates
// an error. This function should be called frequently, otherwise microphone
// only buffers |buffer_size| bytes which is configured in |SbMicrophoneCreate|
// and the new audio data will be thrown out. No audio data will be read from a
// stopped microphone.
SB_EXPORT int SbMicrophoneRead(SbMicrophone microphone,
                               void* out_audio_data,
                               int audio_data_size);

// Destroys a microphone. If the microphone is in started state, it will be
// stopped first and then be destroyed. Any data that has been recorded and not
// read will be thrown away.
SB_EXPORT void SbMicrophoneDestroy(SbMicrophone microphone);

// Returns the Google Speech API key. The platform manufacturer is responsible
// for registering a Google Speech API key for their products. In the API
// Console (http://developers.google.com/console), you are able to enable the
// Speech APIs and generate a Speech API key.
SB_EXPORT const char* SbMicrophoneGetSpeechApiKey();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_HAS(MICROPHONE) && SB_VERSION(2)

#endif  // STARBOARD_MICROPHONE_H_
