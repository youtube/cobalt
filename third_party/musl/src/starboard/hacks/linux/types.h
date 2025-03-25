// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_MUSL_SRC_STARBOARD_HACKS_LINUX_TYPES_H_
#define THIRD_PARTY_MUSL_SRC_STARBOARD_HACKS_LINUX_TYPES_H_

// TODO: b/406082241 - Remove files in starboard/hacks/
// This file is used to stub out any API's/code which is need for building
// upstream chromium code which is theoretically not needed in cobalt. We want
// to revisit all the hacks here and remove them via more elegant methods like
// GN flags, BUILDFLAGS etc.

// ../../third_party/libsync/src/sync.c:34:3: error: unknown type name '__u32'
//   __u32 value;
typedef unsigned int __u32;
//   ^
// ../../third_party/libsync/src/sync.c:36:3: error: unknown type name '__s32'
typedef int32_t __s32;

//./../third_party/libsync/src/sync.c:180:41: error: use of undeclared
// identifier '__u8'
typedef uint8_t __u8;

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_HACKS_LINUX_TYPES_H_
