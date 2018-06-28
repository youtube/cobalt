// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_ENABLE_IF_H_
#define COBALT_BASE_ENABLE_IF_H_

namespace base {

// These classes are used to implement std::enable_if, which is available
// only in C++11.
// See http://en.cppreference.com/w/cpp/types/enable_if for documentation.
template <bool B, class T = void>
struct enable_if {};

template <class T>
struct enable_if<true, T> {
  typedef T type;
};

}  // namespace base

#endif  // COBALT_BASE_ENABLE_IF_H_
