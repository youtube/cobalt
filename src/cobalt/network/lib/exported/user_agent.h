// Copyright 2017 Google Inc. All Rights Reserved.
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

// This file provides a simple API for exposing Starboard as a
// library to another app.

#ifndef COBALT_NETWORK_LIB_EXPORTED_USER_AGENT_H_
#define COBALT_NETWORK_LIB_EXPORTED_USER_AGENT_H_

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Sets a string to be appended to the platform segment of the User Agent
// string that the Starboard application will report. The maximum length of
// |suffix| is 128 including null terminator (which is required). Returns false
// if the suffix could not be set, in which case nothing will be apended to the
// UA string.
SB_EXPORT_PLATFORM bool CbLibUserAgentSetPlatformNameSuffix(const char* suffix);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_NETWORK_LIB_EXPORTED_USER_AGENT_H_
