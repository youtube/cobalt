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

#ifndef THIRD_PARTY_MUSL_SRC_STARBOARD_HACKS_SYS_CDEFS_H_
#define THIRD_PARTY_MUSL_SRC_STARBOARD_HACKS_SYS_CDEFS_H_

// TODO: b/406082241 - Remove files in starboard/hacks/
// This file is used to stub out any API's/code which is need for building
// upstream chromium code which is theoretically not needed in cobalt. We want
// to revisit all the hacks here and remove them via more elegant methods like
// GN flags, BUILDFLAGS etc.

// ../../third_party/libsync/src/include/sync/sync.h:27:1: error: unknown type
// name '__BEGIN_DECLS'
// ../../third_party/libsync/src/include/sync/sync.h:161:1: error: unknown type
// name '__END_DECLS'
#if defined(__cplusplus)
#define __BEGIN_EXTERN_C extern "C" {
#define __END_EXTERN_C }
#else
#define __BEGIN_EXTERN_C
#define __END_EXTERN_C
#endif

#define __BEGIN_DECLS __BEGIN_EXTERN_C
#define __END_DECLS __END_EXTERN_C

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_HACKS_SYS_CDEFS_H_
