//
// Copyright 2013 Google Inc. All Rights Reserved.
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
//

#include "media/player/crypto/key_systems.h"

#include "base/string_util.h"
#include "media/crypto/shell_decryptor_factory.h"
#include "media/player/mime_util.h"

namespace media {

bool IsSupportedKeySystem(const std::string& key_system) {
  return ShellDecryptorFactory::Supports(key_system);
}

bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  if (ShellDecryptorFactory::Supports(key_system)) {
    if (mime_type == "video/mp4" || mime_type == "video/webm" ||
        mime_type == "audio/mp4") {
      return AreSupportedMediaCodecs(codecs);
    }
  }
  return false;
}

// Used for logging only.
std::string KeySystemNameForUMA(const std::string& key_system) {
  return key_system;
}

bool CanUseAesDecryptor(const std::string& key_system) {
  return false;
}

std::string GetPluginType(const std::string& key_system) {
  return std::string();
}

}  // namespace media
