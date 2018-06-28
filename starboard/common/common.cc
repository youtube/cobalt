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

#include "starboard/configuration.h"

// These audits are here so they are only displayed once every build.

#if SB_API_VERSION == SB_EXPERIMENTAL_API_VERSION
#pragma message( \
    "Your platform's SB_API_VERSION == SB_EXPERIMENTAL_API_VERSION. " \
    "You are implementing the Experimental version of Starboard at your " \
    "own risk!  We don't recommend this for third parties.")
#endif

#if defined(SB_RELEASE_CANDIDATE_API_VERSION) && \
    SB_API_VERSION >= SB_RELEASE_CANDIDATE_API_VERSION && \
    SB_API_VERSION < SB_EXPERIMENTAL_API_VERSION
#pragma message( \
    "Your platform's SB_API_VERSION >= SB_RELEASE_CANDIDATE_API_VERSION. " \
    "Note that the RC version of Starboard is still subject to change.")
#endif
