// Copyright 2013 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_EXPORT_H_
#define COBALT_EXPORT_H_
#include "starboard/configuration.h"

#if defined(COMPONENT_BUILD) || SB_IS(MODULAR)
#if defined(WIN32)
#if defined(COBALT_IMPLEMENTATION)
#define COBALT_EXPORT __declspec(dllexport)
#else
#define COBALT_EXPORT __declspec(dllimport)
#endif  // defined(COBALT_IMPLEMENTATION)

#else
#if defined(COBALT_IMPLEMENTATION)
#define COBALT_EXPORT __attribute__((visibility("default")))
#else
#define COBALT_EXPORT
#endif  // defined(COBALT_IMPLEMENTATION)
#endif  // defined(WIN32)

#else
#define COBALT_EXPORT
#endif  // #if defined(COMPONENT_BUILD) || SB_IS(MODULAR)


#endif  // COBALT_EXPORT_H_
