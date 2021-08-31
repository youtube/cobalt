// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
#ifndef STARBOARD_CONTRIB_STADIA_CLIENTS_VENDOR_PUBLIC_STADIA_EXPORT_H_
#define STARBOARD_CONTRIB_STADIA_CLIENTS_VENDOR_PUBLIC_STADIA_EXPORT_H_

// Generates a forward declaration, a string with the function name, and a
// typedef for the given return_type, function_name, and parameters. This macro
// ensures consistency between the symbol that can be loaded dynamincally for
// this function and the underlying implementation. e.g.
// STADIA_EXPORT_FUNCTION(bool, GreaterThan, (int a, int b)) will produce the
// following statements:
//     * bool GreaterThan(int a, int b);
//     * constexpr const char* kGreaterThan = "_GreaterThan";
//     * typedef bool (*GreaterThanFunction)(int a, int b);
#define STADIA_EXPORT_FUNCTION(return_type, name, parameters) \
  return_type name parameters;                                \
  constexpr const char* k##name = #name;                      \
  typedef return_type(*name##Function) parameters
#endif  // STARBOARD_CONTRIB_STADIA_CLIENTS_VENDOR_PUBLIC_STADIA_EXPORT_H_
