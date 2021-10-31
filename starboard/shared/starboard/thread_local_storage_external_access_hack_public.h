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

#ifndef STARBOARD_SHARED_STARBOARD_THREAD_LOCAL_STORAGE_EXTERNAL_ACCESS_HACK_PUBLIC_H_
#define STARBOARD_SHARED_STARBOARD_THREAD_LOCAL_STORAGE_EXTERNAL_ACCESS_HACK_PUBLIC_H_

// This file defines "backdoor" functions that allow external libraries access
// to internal functions.  In particular, this is currently being used by Cobalt
// on some non-Starboard platforms to gain access to thread local storage thread
// initialization and shutdown functions since some platforms are only "kinda"
// using Starboard right now, via glimp.  Since most of non-Starboard Cobalt
// creates threads through pthreads, not Starboard, we need to call these
// functions on newly created pthread-created threads.

// TODO: This file should ABSOLUTELY be removed as soon as all platforms are
//       fully up and running on Starboard.

#ifdef __cplusplus
extern "C" {
#endif

void StarboardSharedTLSKeyManagerInitializeTLSForThread();
void StarboardSharedTLSKeyManagerShutdownTLSForThread();

#ifdef __cplusplus
}
#endif

#endif  // STARBOARD_SHARED_STARBOARD_THREAD_LOCAL_STORAGE_EXTERNAL_ACCESS_HACK_PUBLIC_H_
