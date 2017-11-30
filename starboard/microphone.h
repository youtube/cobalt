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

// Module Overview: Starboard Microphone module
//
// Defines functions for microphone creation, control, audio data fetching,
// and destruction. This module supports multiple calls to |SbMicrophoneOpen|
// and |SbMicrophoneClose|, and the implementation should handle multiple calls
// to one of those functions on the same microphone. For example, your
// implementation should handle cases where |SbMicrophoneOpen| is called twice
// on the same microphone without a call to |SbMicrophoneClose| in between.
//
// This API is not thread-safe and must be called from a single thread.
//
// How to use this API:
// 1. Call |SbMicrophoneGetAvailableInfos| to get a list of available microphone
//    information.
// 2. Create a supported microphone, using |SbMicrophoneCreate|, with enough
//    buffer size and sample rate. Use |SbMicrophoneIsSampleRateSupported| to
//    verify the sample rate.
// 3. Use |SbMicrophoneOpen| to open the microphone port and start recording
//    audio data.
// 4. Periodically read out the data from microphone with |SbMicrophoneRead|.
// 5. Call |SbMicrophoneClose| to close the microphone port and stop recording
//    audio data.
// 6. Destroy the microphone with |SbMicrophoneDestroy|.

#ifndef STARBOARD_MICROPHONE_H_
#define STARBOARD_MICROPHONE_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#if SB_HAS(MICROPHONE)

#if SB_RELEASE_CANDIDATE_API_VERSION >= SB_MICROPHONE_LABEL_API_VERSION
#define kSbMicrophoneLabelSize 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

// All possible microphone types.
typedef enum SbMicrophoneType {
  // Built-in microphone in camera.
  kSbMicrophoneCamera,

  // Microphone in the headset that can be a wired or wireless USB headset.
  kSbMicrophoneUSBHeadset,

  // Microphone in the VR headset.
  kSbMicrophoneVRHeadset,

  // Microphone in the analog headset.
  kSBMicrophoneAnalogHeadset,

  // Unknown microphone type. The microphone could be different than the other
  // enum descriptions or could fall under one of those descriptions.
  kSbMicrophoneUnknown,
} SbMicrophoneType;

// An opaque handle to an implementation-private structure that represents a
// microphone ID.
typedef struct SbMicrophoneIdPrivate* SbMicrophoneId;

// Well-defined value for an invalid microphone ID handle.
#define kSbMicrophoneIdInvalid ((SbMicrophoneId)NULL)

// Indicates whether the given microphone ID is valid.
static SB_C_INLINE bool SbMicrophoneIdIsValid(SbMicrophoneId id) {
  return id != kSbMicrophoneIdInvalid;
}

// Microphone information.
typedef struct SbMicrophoneInfo {
  // Microphone id.
  SbMicrophoneId id;

  // Microphone type.
  SbMicrophoneType type;

  // The microphone's maximum supported sampling rate.
  int max_sample_rate_hz;

  // The minimum read size required for each read from microphone.
  int min_read_size;

#if SB_RELEASE_CANDIDATE_API_VERSION >= SB_MICROPHONE_LABEL_API_VERSION
  // Name of the microphone. Can be empty. This should indicate the
  // friendly name of the microphone type. For example, "Headset Microphone".
  // The string must be null terminated.
  char label[kSbMicrophoneLabelSize];
#endif
} SbMicrophoneInfo;

// An opaque handle to an implementation-private structure that represents a
// microphone.
typedef struct SbMicrophonePrivate* SbMicrophone;

// Well-defined value for an invalid microphone handle.
#define kSbMicrophoneInvalid ((SbMicrophone)NULL)

// Indicates whether the given microphone is valid.
static SB_C_INLINE bool SbMicrophoneIsValid(SbMicrophone microphone) {
  return microphone != kSbMicrophoneInvalid;
}

// Retrieves all currently available microphone information and stores it in
// |out_info_array|. The return value is the number of the available
// microphones. If the number of available microphones is larger than
// |info_array_size|, then |out_info_array| is filled up with as many available
// microphones as possible and the actual number of available microphones is
// returned. A negative return value indicates that an internal error occurred.
//
// |out_info_array|: All currently available information about the microphone
//   is placed into this output parameter.
// |info_array_size|: The size of |out_info_array|.
SB_EXPORT int SbMicrophoneGetAvailable(SbMicrophoneInfo* out_info_array,
                                       int info_array_size);

// Indicates whether the microphone supports the sample rate.
SB_EXPORT bool SbMicrophoneIsSampleRateSupported(SbMicrophoneId id,
                                                 int sample_rate_in_hz);

// Creates a microphone with the specified ID, audio sample rate, and cached
// audio buffer size. Starboard only requires support for creating one
// microphone at a time, and implementations may return an error if a second
// microphone is created before the first is destroyed.
//
// The function returns the newly created SbMicrophone object. However, if you
// try to create a microphone that has already been initialized, if the sample
// rate is unavailable, or if the buffer size is invalid, the function should
// return |kSbMicrophoneInvalid|.
//
// |id|: The ID that will be assigned to the newly created SbMicrophone.
// |sample_rate_in_hz|: The new microphone's audio sample rate in Hz.
// |buffer_size_bytes|: The size of the buffer where signed 16-bit integer
//   audio data is temporarily cached to during the capturing. The audio data
//   is removed from the audio buffer if it has been read, and new audio data
//   can be read from this buffer in smaller chunks than this size. This
//   parameter must be set to a value greater than zero and the ideal size is
//   |2^n|.
SB_EXPORT SbMicrophone SbMicrophoneCreate(SbMicrophoneId id,
                                          int sample_rate_in_hz,
                                          int buffer_size_bytes);

// Opens the microphone port and starts recording audio on |microphone|.
//
// Once started, the client needs to periodically call |SbMicrophoneRead| to
// receive the audio data. If the microphone has already been started, this call
// clears the unread buffer. The return value indicates whether the microphone
// is open.
// |microphone|: The microphone that will be opened and will start recording
// audio.
SB_EXPORT bool SbMicrophoneOpen(SbMicrophone microphone);

// Closes the microphone port, stops recording audio on |microphone|, and
// clears the unread buffer if it is not empty. If the microphone has already
// been stopped, this call is ignored. The return value indicates whether the
// microphone is closed.
//
// |microphone|: The microphone to close.
SB_EXPORT bool SbMicrophoneClose(SbMicrophone microphone);

// Retrieves the recorded audio data from the microphone and writes that data
// to |out_audio_data|.
//
// The return value is zero or the positive number of bytes that were read.
// Neither the return value nor |audio_data_size| exceeds the buffer size.
// A negative return value indicates that an error occurred.
//
// This function should be called frequently. Otherwise, the microphone only
// buffers |buffer_size| bytes as configured in |SbMicrophoneCreate| and the
// new audio data is thrown out. No audio data is read from a stopped
// microphone.
//
// |microphone|: The microphone from which to retrieve recorded audio data.
// |out_audio_data|: The buffer to which the retrieved data will be written.
// |audio_data_size|: The number of requested bytes. If |audio_data_size| is
//   smaller than |min_read_size| of |SbMicrophoneInfo|, the extra audio data
//   that has already been read from the device is discarded.
SB_EXPORT int SbMicrophoneRead(SbMicrophone microphone,
                               void* out_audio_data,
                               int audio_data_size);

// Destroys a microphone. If the microphone is in started state, it is first
// stopped and then destroyed. Any data that has been recorded and not read
// is thrown away.
SB_EXPORT void SbMicrophoneDestroy(SbMicrophone microphone);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_HAS(MICROPHONE)

#endif  // STARBOARD_MICROPHONE_H_
