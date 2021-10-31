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

#if defined(COMPONENT_BUILD)

#if defined(COBALT_IMPLEMENTATION)
#define COBALT_EXPORT __declspec(dllexport)
#define COBALT_EXPORT_PRIVATE __declspec(dllexport)
#else
#define COBALT_EXPORT __declspec(dllimport)
#define COBALT_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(COBALT_IMPLEMENTATION)

#else  // defined(COMPONENT_BUILD)

#define COBALT_EXPORT
#define COBALT_EXPORT_PRIVATE
#endif

// Macro for importing data, either via extern or dllimport.
#if defined(COMPONENT_BUILD)
#define COBALT_EXTERN COBALT_EXPORT extern
#else
#define COBALT_EXTERN extern
#endif
#endif  // COBALT_EXPORT_H_
