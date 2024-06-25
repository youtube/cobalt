// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include <iostream>

// Provide a workaround alias for newer libcxx that places iostreams in
// a namespace.
namespace std {
// This refers to a macro that should normally resolve to __1, __2 or be
// defined by LIBCXX_ABI_NAMESPACE or LIBCXX_ABI_VERSION
namespace _LIBCPP_ABI_NAMESPACE {
__attribute__((visibility("default"))) std::istream& cin = ::std::cin;
}
}  // namespace std
