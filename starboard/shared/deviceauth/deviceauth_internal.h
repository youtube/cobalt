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

#ifndef STARBOARD_SHARED_DEVICEAUTH_DEVICEAUTH_INTERNAL_H_
#define STARBOARD_SHARED_DEVICEAUTH_DEVICEAUTH_INTERNAL_H_

#include <cstddef>
#include <cstdint>

namespace starboard {
namespace shared {
namespace deviceauth {

bool SignWithCertificationSecretKey(const char* secret_key,
                                    const uint8_t* message,
                                    size_t message_size_in_bytes,
                                    uint8_t* digest,
                                    size_t digest_size_in_bytes);

}  // namespace deviceauth
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_DEVICEAUTH_DEVICEAUTH_INTERNAL_H_
