// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#if SB_API_VERSION >= 12

namespace starboard {
namespace sabi {
namespace {

typedef enum GenericEnumType {
  kOnlyTag,
} GenericEnumType;

SB_COMPILE_ASSERT((static_cast<GenericEnumType>(-1) < 0) == SB_HAS_SIGNED_ENUM,
                  SB_HAS_SIGNED_ENUM_is_inconsistent_with_sign_of_enum);

SB_COMPILE_ASSERT(sizeof(GenericEnumType) == SB_SIZE_OF_ENUM,
                  SB_SIZE_OF_ENUM_is_inconsistent_with_sizeof_enum);

}  // namespace
}  // namespace sabi
}  // namespace starboard

#endif  // SB_API_VERSION >= 12
