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

#include "net/base/platform_mime_util.h"

#include "net/base/network_interfaces.h"

namespace net {

bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const base::FilePath::StringType& ext,
    std::string* result) const {
  return false;
}

bool PlatformMimeUtil::GetPlatformPreferredExtensionForMimeType(
    const std::string& mime_type,
    base::FilePath::StringType* ext) const {
  return false;
}

void PlatformMimeUtil::GetPlatformExtensionsForMimeType(
    const std::string& mime_type,
    std::unordered_set<base::FilePath::StringType>* extensions) const {
}

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  return false;
}

}
