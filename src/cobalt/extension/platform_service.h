// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_EXTENSION_PLATFORM_SERVICE_H_
#define COBALT_EXTENSION_PLATFORM_SERVICE_H_

#include <stdint.h>

#include "starboard/configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CobaltExtensionPlatformServicePrivate
    CobaltExtensionPlatformServicePrivate;
typedef CobaltExtensionPlatformServicePrivate* CobaltExtensionPlatformService;

// Well-defined value for an invalid |Service|.
#define kCobaltExtensionPlatformServiceInvalid \
  ((CobaltExtensionPlatformService)0)

#define kCobaltExtensionPlatformServiceName \
  "dev.cobalt.extension.PlatformService"

// Checks whether a |CobaltExtensionPlatformService| is valid.
static SB_C_INLINE bool CobaltExtensionPlatformServiceIsValid(
    CobaltExtensionPlatformService service) {
  return service != kCobaltExtensionPlatformServiceInvalid;
}

// When a client receives a message from a service, the service will be passed
// in as the |context| here, with |data|, which has length |length|.
typedef void (*ReceiveMessageCallback)(void* context, const void* data,
                                       uint64_t length);

typedef struct CobaltExtensionPlatformServiceApi {
  // Name should be the string kCobaltExtensionPlatformServiceName.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Return whether the platform has service indicated by |name|.
  bool (*Has)(const char* name);

  // Open and return a service by name.
  //
  // |context|: pointer to context object for callback.
  // |name|: name of the service.
  // |receive_callback|: callback to run when Cobalt should receive data.
  CobaltExtensionPlatformService (*Open)(
      void* context, const char* name, ReceiveMessageCallback receive_callback);

  // Close the service passed in as |service|.
  void (*Close)(CobaltExtensionPlatformService service);

  // Send |data| of length |length| to |service|. If there is a synchronous
  // response, it will be returned via void* and |output_length| will be set
  // to its length. The returned void* will be owned by the caller, and must
  // be deallocated via SbMemoryDeallocate() by the caller when appropriate.
  // If there is no synchronous response, NULL will be returned and
  // |output_length| will be 0. The |invalid_state| will be set to true if the
  // service is not currently able to accept data, and otherwise will be set to
  // false.
  void* (*Send)(CobaltExtensionPlatformService service, void* data,
                uint64_t length, uint64_t* output_length, bool* invalid_state);
} CobaltExtensionPlatformServiceApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_PLATFORM_SERVICE_H_
