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

// Definitions that allow for DRM support, common between Player and Decoder
// interfaces.

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

// Returns whether the |drm_system| is a valid SbDrmSystem.
static SB_C_FORCE_INLINE bool SbDrmSystemIsValid(SbDrmSystem drm) {
  return drm != kSbDrmSystemInvalid;
}

// Creates a new |key_system| DRM system that can be used when constructing an
// SbPlayer or an SbDecoder.  |key_system| should be in fhe form of
// "com.example.somesystem" as suggested by
// https://w3c.github.io/encrypted-media/#key-system.  All letters in
// |key_system| should be in lower case and will be matched exactly with known
// DRM key systems of the platform.
// |context| will be passed when any callback parameters of this function are
// called.
// |update_request_callback| is a callback that will be called every time after
// SbDrmGenerateSessionUpdateRequest() is called.
// |session_updated_callback| is a callback that will be called every time
// after  SbDrmUpdateSession() is called.
// Please refer to the document of SbDrmGenerateSessionUpdateRequest() and
// SbDrmUpdateSession() for more details.
// Returns kSbDrmSystemInvalid if the |key_system| is unsupported.
SB_EXPORT SbDrmSystem
SbDrmCreateSystem(const char* key_system,
                  void* context,
                  SbDrmSessionUpdateRequestFunc update_request_callback,
                  SbDrmSessionUpdatedFunc session_updated_callback);

// Asynchronously generates a session update request payload for
// |initialization_data|, of |initialization_data_size|, in case sensitive
// |type|, extracted from the media stream, in |drm_system|'s key system. Calls
// |update_request_callback| with |context| and either a populated request, or
// NULL |session_id| if an error occured.  |context| may be used to distinguish
// callbacks from multiple concurrent calls to
// SbDrmGenerateSessionUpdateRequest(), and/or to route callbacks back to an
// object instance.
//
// Callbacks may called from another thread or from the current thread before
// this function returns.
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
// with that system will be able to decrypt samples encrypted.
//
// |session_updated_callback| may called from another thread or from the current
// thread before this function returns.
SB_EXPORT void SbDrmUpdateSession(SbDrmSystem drm_system,
                                  const void* key,
                                  int key_size,
                                  const void* session_id,
                                  int session_id_size);

// Clear any internal states/resources related to the particular |session_id|.
SB_EXPORT void SbDrmCloseSession(SbDrmSystem drm_system,
                                 const void* session_id,
                                 int session_id_size);

// Gets the number of keys installed in the given |drm_system| system.
SB_EXPORT int SbDrmGetKeyCount(SbDrmSystem drm_system);

// Gets the |out_key|, |out_key_size|, and |out_status| for key with |index| in
// the given |drm_system| system. Returns whether a key is installed at |index|.
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
// requests will also be invalidated.
SB_EXPORT void SbDrmRemoveAllKeys(SbDrmSystem drm_system);

// Destroys |drm_system|, which implicitly removes all keys installed in it, and
// invalidates all outstanding session update requests. A DRM system cannot be
// destroyed unless any associated SbPlayer or SbDecoder has first been
// destroyed.
//
// All callbacks are guaranteed to be finished when this function returns.  So
// calling this function from a callback passed to SbDrmCreateSystem() will
// result in deadlock.
SB_EXPORT void SbDrmDestroySystem(SbDrmSystem drm_system);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_DRM_H_
