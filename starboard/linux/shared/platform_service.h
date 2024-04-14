// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_LINUX_SHARED_PLATFORM_SERVICE_H_
#define STARBOARD_LINUX_SHARED_PLATFORM_SERVICE_H_

#include <memory>
#include <string>

#include "starboard/extension/platform_service.h"

typedef struct PlatformServiceImpl {
  void* context;
  ReceiveMessageCallback receive_callback;

  PlatformServiceImpl(void* context, ReceiveMessageCallback receive_callback)
      : context(context), receive_callback(receive_callback) {}

  PlatformServiceImpl() = default;
} PlatformServiceImpl;

typedef struct CobaltPlatformServiceApi {
  // Name of Cobalt Platform Service.
  // Each feature should a unique name.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Return whether the platform has service indicated by |name|.
  bool (*Has)(const char* name);

  // Open a service by name.
  //
  // |context|: pointer to context object for callback.
  // |receive_callback|: callback to run when Cobalt should receive data.
  PlatformServiceImpl* (*Open)(void* context,
                               ReceiveMessageCallback receive_callback);

  // Perform operations before closing the service passed in as |service|.
  // Function Close shouldn't manually delete PlatformServiceImpl pointer,
  // because it is managed by unique_ptr in Platform Service.
  void (*Close)(PlatformServiceImpl* service);

  // Send |data| of length |length| to |service|. If there is a synchronous
  // response, it will be returned via void* and |output_length| will be set
  // to its length. The returned void* will be owned by the caller, and must
  // be deallocated via free() by the caller when appropriate.
  // If there is no synchronous response, NULL will be returned and
  // |output_length| will be 0. The |invalid_state| will be set to true if the
  // service is not currently able to accept data, and otherwise will be set to
  // false.
  void* (*Send)(PlatformServiceImpl* service,
                void* data,
                uint64_t length,
                uint64_t* output_length,
                bool* invalid_state);
} CobaltPlatformServiceApi;

typedef struct CobaltExtensionPlatformServicePrivate {
  void* context;
  ReceiveMessageCallback receive_callback;
  std::string name;
  std::unique_ptr<PlatformServiceImpl> platform_service_impl;
} CobaltExtensionPlatformServicePrivate;

// Well-defined value for an invalid |PlatformServiceImpl|.
#define kPlatformServiceImplInvalid (reinterpret_cast<PlatformServiceImpl*>(0))

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace shared {

const void* GetPlatformServiceApi();

}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_LINUX_SHARED_PLATFORM_SERVICE_H_
