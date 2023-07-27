// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <Mfobjects.h>
#include <wrl/client.h>

#include <string>

#ifndef STARBOARD_SHARED_WIN32_MEDIA_FOUNDATION_UTILS_H_
#define STARBOARD_SHARED_WIN32_MEDIA_FOUNDATION_UTILS_H_

namespace starboard {
namespace shared {
namespace win32 {

std::ostream& operator<<(std::ostream& os, const IMFMediaType& media_type);

std::string ToString(IMFAttributes* type);

void CopyProperties(IMFMediaType* source, IMFMediaType* destination);

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_MEDIA_FOUNDATION_UTILS_H_
