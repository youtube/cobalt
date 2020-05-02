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

#ifndef STARBOARD_CLIENT_PORTING_ICU_INIT_ICU_INIT_H_
#define STARBOARD_CLIENT_PORTING_ICU_INIT_ICU_INIT_H_

#if defined(STARBOARD)

#ifdef __cplusplus
extern "C" {
#endif

// Initializes ICU using a standard path based on the Starboard content path.
// This function must be threadsafe and idempotent. Applications that wish to
// initialize ICU differently may define their own SbIcuInit function rather
// than using the implementation here.
void SbIcuInit();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_ICU_INIT_ICU_INIT_H_
