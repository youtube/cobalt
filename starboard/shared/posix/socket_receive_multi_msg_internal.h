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

#ifndef STARBOARD_SHARED_POSIX_SOCKET_RECEIVE_MULTI_MSG_INTERNAL_H_
#define STARBOARD_SHARED_POSIX_SOCKET_RECEIVE_MULTI_MSG_INTERNAL_H_

#include "starboard/extension/socket_receive_multi_msg.h"
#include "starboard/socket.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned int SbSocketReceiveMultiMsgBufferSize(unsigned int num_messages,
                                               unsigned int message_size,
                                               unsigned int control_size);

void SbSocketReceiveMultiMsgBufferInitialize(unsigned int num_messages,
                                             unsigned int message_size,
                                             unsigned int control_size,
                                             char* data);

SbSocketReceiveMultiMsgResult* SbSocketReceiveMultiMsg(SbSocket socket,
                                                       char* buffer);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_POSIX_SOCKET_RECEIVE_MULTI_MSG_INTERNAL_H_
