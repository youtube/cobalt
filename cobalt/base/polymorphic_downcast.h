// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_POLYMORPHIC_DOWNCAST_H_
#define COBALT_BASE_POLYMORPHIC_DOWNCAST_H_

#include "base/logging.h"

namespace base {

// This function is designed to model the behavior of polymorphic_downcast from
// http://www.boost.org/doc/libs/1_56_0/libs/conversion/cast.htm

template <typename Derived, typename Base>
Derived polymorphic_downcast(Base base) {
#if defined(COBALT_BUILD_TYPE_DEBUG) || defined(COBALT_BUILD_TYPE_DEVEL)
  DCHECK(dynamic_cast<Derived>(base) == base);
#endif
  return static_cast<Derived>(base);
}

}  // namespace base

#endif  // COBALT_BASE_POLYMORPHIC_DOWNCAST_H_
