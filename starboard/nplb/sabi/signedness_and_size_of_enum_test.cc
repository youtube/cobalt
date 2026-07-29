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

#include <type_traits>

#include "starboard/configuration.h"

namespace nplb {
namespace {

// Unscoped enumeration without an explicit underlying type and with only
// non-negative enumerators. This allows us to verify the compiler's default
// ABI signedness and size rules for standard enumeration types.
typedef enum GenericEnumType {
  kOnlyTag = 0,
} GenericEnumType;

// Verify that the compiler's default underlying integral type for standard
// enumerations matches the Starboard ABI configuration specification.
SB_COMPILE_ASSERT(
    std::is_signed<std::underlying_type<GenericEnumType>::type>::value ==
        SB_HAS_SIGNED_ENUM,
    SB_HAS_SIGNED_ENUM_is_inconsistent_with_sign_of_enum);

// Verify that the size of standard enumerations matches the ABI specification.
SB_COMPILE_ASSERT(sizeof(GenericEnumType) == SB_SIZE_OF_ENUM,
                  SB_SIZE_OF_ENUM_is_inconsistent_with_sizeof_enum);

}  // namespace
}  // namespace nplb
