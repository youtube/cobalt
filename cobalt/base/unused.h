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

#ifndef COBALT_BASE_UNUSED_H_
#define COBALT_BASE_UNUSED_H_

namespace base {

// This struct is available for cases where a type is required, but will not be
// used, and causes the type to use 0 bytes. A current use case for this is with
// base::small_map. Because base::SmallSet does not exist, a map must sometimes
// be used in cases where the desired functionality is that of a set. Providing
// Unused as the map's value type allows the value to not add additional memory
// overhead.
struct Unused {};

}  // namespace base

#endif  // COBALT_BASE_UNUSED_H_
