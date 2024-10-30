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

#ifndef STARBOARD_EXTENSION_SOCKET_RECEIVE_MULTI_MSG_H_
#define STARBOARD_EXTENSION_SOCKET_RECEIVE_MULTI_MSG_H_

#include <stdint.h>

#include "starboard/socket.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SbSocketReceiveMultiMsgPacket {
  char* buffer = nullptr;
  int result = 0;
};

struct SbSocketReceiveMultiMsgResult {
  int result = 0;
  SbSocketReceiveMultiMsgPacket* packets = nullptr;
};

SbSocketReceiveMultiMsgResult* SbSocketReceiveMultiMsg(SbSocket socket,
                                                       char* buffer);

#define kCobaltExtensionSocketReceiveMultiMsgName \
  "dev.cobalt.extension.SocketReceiveMultiMsg"

typedef struct CobaltExtensionSocketReceiveMultiMsgApi {
  // Name should be the string |kCobaltExtensionSocketReceiveMultiMsgName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  unsigned int (*ReceiveMultiMsgBufferSize)(unsigned int num_messages,
                                            unsigned int message_size,
                                            unsigned int control_size);

  void (*ReceiveMultiMsgBufferInitialize)(unsigned int num_messages,
                                          unsigned int message_size,
                                          unsigned int control_size,
                                          char* data);

  SbSocketReceiveMultiMsgResult* (*ReceiveMultiMsg)(SbSocket socket,
                                                    char* buffer);

} CobaltExtensionSocketReceiveMultiMsgApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_SOCKET_RECEIVE_MULTI_MSG_H_
