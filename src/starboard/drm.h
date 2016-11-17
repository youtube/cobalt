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

// Module Overview: Starboard DRM module
//
// Provides definitions that allow for DRM support, which are common
// between Player and Decoder interfaces.

#ifndef STARBOARD_DRM_H_
#define STARBOARD_DRM_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Types -----------------------------------------------------------------

// Status of a particular media key.
// https://w3c.github.io/encrypted-media/#idl-def-MediaKeyStatus
typedef enum SbDrmKeyStatus {
  kSbDrmKeyStatusUsable,
  kSbDrmKeyStatusExpired,
  kSbDrmKeyStatusReleased,
  kSbDrmKeyStatusRestricted,
  kSbDrmKeyStatusDownscaled,
  kSbDrmKeyStatusPending,
  kSbDrmKeyStatusError,
} SbDrmKeyStatus;

// A mapping of clear and encrypted bytes for a single subsample. All
// subsamples within a sample must be encrypted with the same encryption
// parameters. The clear bytes always appear first in the sample.
typedef struct SbDrmSubSampleMapping {
  // How many bytes of the corresponding subsample are not encrypted
  int32_t clear_byte_count;

  // How many bytes of the corresponding subsample are encrypted.
  int32_t encrypted_byte_count;
} SbDrmSubSampleMapping;

// All the optional information needed per sample for encrypted samples.
typedef struct SbDrmSampleInfo {
  // The Initialization Vector needed to decrypt this sample.
  uint8_t initialization_vector[16];
  int initialization_vector_size;

  // The ID of the license (or key) required to decrypt this sample. For
  // PlayReady, this is the license GUID in packed little-endian binary form.
  uint8_t identifier[16];
  int identifier_size;

  // The number of subsamples in this sample, must be at least 1.
  int32_t subsample_count;

  // The clear/encrypted mapping of each subsample in this sample. This must be
  // an array of |subsample_count| mappings.
  const SbDrmSubSampleMapping* subsample_mapping;
} SbDrmSampleInfo;

// A handle to a DRM system which can be used with either an SbDecoder or a
// SbPlayer.
typedef struct SbDrmSystemPrivate* SbDrmSystem;

// A callback that will receive generated session update request when requested
// from a SbDrmSystem. |drm_system| will be the DRM system that
// SbDrmGenerateSessionUpdateRequest() was called on. |context| will be the same
// context that was passed into the call to SbDrmCreateSystem(). |session_id|
// can be NULL if there was an error generating the request.
typedef void (*SbDrmSessionUpdateRequestFunc)(SbDrmSystem drm_system,
                                              void* context,
                                              const void* session_id,
                                              int session_id_size,
                                              const void* content,
                                              int content_size,
                                              const char* url);

// A callback for notifications that a session has been added, and subsequent
// encrypted samples are actively ready to be decoded. |drm_system| will be the
// DRM system that SbDrmUpdateSession() was called on. |context| will be the
// same context passed into that call to SbDrmCreateSystem(). |succeeded| is
// whether the session was successfully updated or not.
typedef void (*SbDrmSessionUpdatedFunc)(SbDrmSystem drm_system,
                                        void* context,
                                        const void* session_id,
                                        int session_id_size,
                                        bool succeeded);

// --- Constants -------------------------------------------------------------

// An invalid SbDrmSystem.
#define kSbDrmSystemInvalid ((SbDrmSystem)NULL)

// --- Functions -------------------------------------------------------------

// Indicates whether |drm_system| is a valid SbDrmSystem.
static SB_C_FORCE_INLINE bool SbDrmSystemIsValid(SbDrmSystem drm) {
  return drm != kSbDrmSystemInvalid;
}

// Creates a new DRM system that can be used when constructing an SbPlayer
// or an SbDecoder.
//
// This function returns kSbDrmSystemInvalid if |key_system| is unsupported.
//
// Also see the documentation of SbDrmGenerateSessionUpdateRequest() and
// SbDrmUpdateSession() for more details.
//
// |key_system|: The DRM key system to be created. The value should be in the
// form of "com.example.somesystem" as suggested by
// https://w3c.github.io/encrypted-media/#key-system. All letters in the value
// should be lowercase and will be matched exactly with known DRM key systems
// of the platform.
// |context|: A value passed when any of this function's callback parameters
// are called.
// |update_request_callback|: A function that is called every time after
// SbDrmGenerateSessionUpdateRequest() is called.
// |session_updated_callback|: A function that is called every time after
// SbDrmUpdateSession() is called.
SB_EXPORT SbDrmSystem
SbDrmCreateSystem(const char* key_system,
                  void* context,
                  SbDrmSessionUpdateRequestFunc update_request_callback,
                  SbDrmSessionUpdatedFunc session_updated_callback);

// Asynchronously generates a session update request payload for
// |initialization_data|, of |initialization_data_size|, in case sensitive
// |type|, extracted from the media stream, in |drm_system|'s key system.
//
// This function calls |drm_system|'s |update_request_callback| function,
// which is defined when the DRM system is created by SbDrmCreateSystem. When
// calling that function, this function either sends |context| (also from
// |SbDrmCreateSystem|) and a populated request, or it sends NULL |session_id|
// if an error occurred.
//
// |drm_system|'s |context| may be used to distinguish callbacks from
// multiple concurrent calls to SbDrmGenerateSessionUpdateRequest(), and/or
// to route callbacks back to an object instance.
//
// Callbacks may be called either from the current thread before this function
// returns or from another thread.
//
// |drm_system|: The DRM system that defines the key system used for the
// session update request payload as well as the callback function that is
// called as a result of the function being called.
// |type|: The case-sensitive type of the session update request payload.
// |initialization_data|: The data for which the session update request payload
// is created.
// |initialization_data_size|: The size of the session update request payload.
SB_EXPORT void SbDrmGenerateSessionUpdateRequest(
    SbDrmSystem drm_system,
    const char* type,
    const void* initialization_data,
    int initialization_data_size);

// Update session with |key|, in |drm_system|'s key system, from the license
// server response. |request| must be the corresponding returned request from
// SbDrmGenerateSessionUpdateRequest(). Calls |session_updated_callback| with
// |context| and whether the update succeeded. |context| may be used to
// distinguish callbacks from multiple concurrent calls to SbDrmUpdateSession(),
// and/or to route callbacks back to an object instance.
//
// Once the session is successfully updated, an SbPlayer or SbDecoder associated
// with that DRM key system will be able to decrypt encrypted samples.
//
// |drm_system|'s |session_updated_callback| may called either from the
// current thread before this function returns or from another thread.
SB_EXPORT void SbDrmUpdateSession(SbDrmSystem drm_system,
                                  const void* key,
                                  int key_size,
                                  const void* session_id,
                                  int session_id_size);

// Clear any internal states/resources related to the specified |session_id|.
SB_EXPORT void SbDrmCloseSession(SbDrmSystem drm_system,
                                 const void* session_id,
                                 int session_id_size);

// Returns the number of keys installed in |drm_system|.
//
// |drm_system|: The system for which the number of installed keys is retrieved.
SB_EXPORT int SbDrmGetKeyCount(SbDrmSystem drm_system);

// Gets |out_key|, |out_key_size|, and |out_status| for the key with |index|
// in |drm_system|. Returns whether a key is installed at |index|.
// If not, the output parameters, which all must not be NULL, will not be
// modified.
SB_EXPORT bool SbDrmGetKeyStatus(SbDrmSystem drm_system,
                                 const void* session_id,
                                 int session_id_size,
                                 int index,
                                 void** out_key,
                                 int* out_key_size,
                                 SbDrmKeyStatus* out_status);

// Removes all installed keys for |drm_system|. Any outstanding session update
// requests are also invalidated.
//
// |drm_system|: The DRM system for which keys should be removed.
SB_EXPORT void SbDrmRemoveAllKeys(SbDrmSystem drm_system);

// Destroys |drm_system|, which implicitly removes all keys installed in it and
// invalidates all outstanding session update requests. A DRM system cannot be
// destroyed unless any associated SbPlayer or SbDecoder has first been
// destroyed.
//
// All callbacks are guaranteed to be finished when this function returns.
// As a result, if this function is called from a callback that is passed
// to SbDrmCreateSystem(), a deadlock will occur.
//
// |drm_system|: The DRM system to be destroyed.
SB_EXPORT void SbDrmDestroySystem(SbDrmSystem drm_system);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_DRM_H_
