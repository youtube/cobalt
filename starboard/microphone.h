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

// Module Overview: Starboard Microphone module
//
// Defines functions for microphone creation, control, audio data fetching, and
// destruction. The implementation must handle multiple calls to
// |SbMicrophoneOpen| and |SbMicrophoneClose| on the same microphone. For
// example, calling |SbMicrophoneOpen| twice without an intervening
// |SbMicrophoneClose| must be handled gracefully.
//
// This API is not thread-safe and must be called from a single thread.
//
// API Usage:
//
// 1. Query available microphones by calling |SbMicrophoneGetAvailable|.
// 2. Create a supported microphone using |SbMicrophoneCreate|, specifying a
//    supported sample rate and buffer size. Verify the sample rate with
//    |SbMicrophoneIsSampleRateSupported|.
// 3. Open the microphone port and start recording using |SbMicrophoneOpen|.
// 4. Periodically read audio data using |SbMicrophoneRead|.
// 5. Close the port and stop recording using |SbMicrophoneClose|.
// 6. Destroy the microphone with |SbMicrophoneDestroy|.

#ifndef STARBOARD_MICROPHONE_H_
#define STARBOARD_MICROPHONE_H_

#include <stddef.h>

#include "starboard/configuration.h"
#include "starboard/export.h"

#define kSbMicrophoneLabelSize 256

#ifdef __cplusplus
extern "C" {
#endif

// All possible microphone types.
typedef enum SbMicrophoneType {
  // Built-in camera microphone.
  kSbMicrophoneCamera,

  // USB headset microphone (wired or wireless).
  kSbMicrophoneUSBHeadset,

  // VR headset microphone.
  kSbMicrophoneVRHeadset,

  // Analog headset microphone.
  kSBMicrophoneAnalogHeadset,

  // Unknown microphone type. Used if the microphone does not map to other enum
  // values.
  kSbMicrophoneUnknown,
} SbMicrophoneType;

// An opaque handle to an implementation-private structure that represents a
// microphone ID.
typedef struct SbMicrophoneIdPrivate* SbMicrophoneId;

// Well-defined value for an invalid microphone ID handle.
#define kSbMicrophoneIdInvalid ((SbMicrophoneId)NULL)

// Indicates whether the given microphone ID is valid.
static inline bool SbMicrophoneIdIsValid(SbMicrophoneId id) {
  return id != kSbMicrophoneIdInvalid;
}

// Microphone information.
typedef struct SbMicrophoneInfo {
  // Microphone ID.
  SbMicrophoneId id;

  // Microphone type.
  SbMicrophoneType type;

  // The microphone's maximum supported sampling rate.
  int max_sample_rate_hz;

  // The minimum read size (in bytes) required for each read.
  int min_read_size;

  // A user-friendly name for the microphone (for example, "Headset
  // Microphone"). Can be empty. The string must be null-terminated.
  char label[kSbMicrophoneLabelSize];
} SbMicrophoneInfo;

// An opaque handle to an implementation-private structure that represents a
// microphone.
typedef struct SbMicrophonePrivate* SbMicrophone;

// Well-defined value for an invalid microphone handle.
#define kSbMicrophoneInvalid ((SbMicrophone)NULL)

// Indicates whether the given microphone is valid.
static inline bool SbMicrophoneIsValid(SbMicrophone microphone) {
  return microphone != kSbMicrophoneInvalid;
}

// Retrieves information for all available microphones and stores it in
// |out_info_array|. Returns the number of available microphones. If this count
// exceeds |info_array_size|, the array is filled to capacity with available
// microphone info, and the total count of available microphones is returned. A
// negative return value indicates an error.
//
// * |out_info_array|: The destination buffer for available microphone info.
// * |info_array_size|: The capacity of |out_info_array|.
SB_EXPORT int SbMicrophoneGetAvailable(SbMicrophoneInfo* out_info_array,
                                       int info_array_size);

// Returns whether the microphone supports the specified sample rate.
SB_EXPORT bool SbMicrophoneIsSampleRateSupported(SbMicrophoneId id,
                                                 int sample_rate_in_hz);

// Creates a microphone with the specified ID, sample rate, and buffer size.
// Starboard only requires support for one active microphone at a time; creating
// a second microphone before destroying the first may fail.
//
// Returns the newly created |SbMicrophone| object. Returns
// |kSbMicrophoneInvalid| if the microphone is already initialized, the sample
// rate is unsupported, or the buffer size is invalid.
//
// * |id|: The ID that will be assigned to the newly created SbMicrophone.
// * |sample_rate_in_hz|: The new microphone's audio sample rate in Hz.
// * |buffer_size_bytes|: The size of the buffer where signed 16-bit integer
//   audio data is temporarily cached during capture. Audio data is removed from
//   the buffer after it is read. New data can be read from this buffer in
//   chunks smaller than the buffer size. This parameter must be greater than
//   zero, and ideally a power of two (`2^n`).
SB_EXPORT SbMicrophone SbMicrophoneCreate(SbMicrophoneId id,
                                          int sample_rate_in_hz,
                                          int buffer_size_bytes);

// Opens the microphone port and starts recording on |microphone|. Once started,
// call |SbMicrophoneRead| periodically to retrieve audio data. If the
// microphone is already open, this call clears the unread buffer. Returns
// `true` if the microphone is successfully opened.
//
// * |microphone|: The microphone to open.
SB_EXPORT bool SbMicrophoneOpen(SbMicrophone microphone);

// Closes the microphone port, stops recording on |microphone|, and clears any
// unread data from the buffer. If the microphone is already stopped, this call
// is ignored. Returns `true` if the microphone is successfully closed.
//
// * |microphone|: The microphone to close.
SB_EXPORT bool SbMicrophoneClose(SbMicrophone microphone);

// Retrieves the recorded audio data from the microphone and writes that data to
// |out_audio_data|.
//
// Returns the number of bytes read (greater than or equal to zero). The
// returned size does not exceed |audio_data_size| or the internal buffer size.
// Returns a negative value on error.
//
// Call this function frequently. If the internal buffer (configured in
// |SbMicrophoneCreate|) fills up, new audio data is discarded. You cannot read
// data from a stopped microphone.
//
// * |microphone|: The microphone to read from.
// * |out_audio_data|: The destination buffer for the read data.
// * |audio_data_size|: The number of bytes to read. If |audio_data_size| is
//   less than |min_read_size| (from |SbMicrophoneInfo|), any additional audio
//   data read from the device is discarded.
SB_EXPORT int SbMicrophoneRead(SbMicrophone microphone,
                               void* out_audio_data,
                               int audio_data_size);

// Destroys a microphone. If the microphone is recording, it is stopped before
// being destroyed. Any unread recorded data is discarded.
SB_EXPORT void SbMicrophoneDestroy(SbMicrophone microphone);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_MICROPHONE_H_
