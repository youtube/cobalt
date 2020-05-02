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

// A poem (POsix EMulation) for functions usually declared in <arpa/inet.h>

#ifndef STARBOARD_CLIENT_PORTING_POEM_INET_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_INET_POEM_H_

#if defined(STARBOARD)
#include "starboard/byte_swap.h"

#undef htonl
#define htonl(x) SB_HOST_TO_NET_U32(x)

#undef htons
#define htons(x) SB_HOST_TO_NET_U16(x)

#undef ntohl
#define ntohl(x) SB_NET_TO_HOST_U32(x)

#undef ntohs
#define ntohs(x) SB_NET_TO_HOST_U16(x)

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_POEM_INET_POEM_H_
