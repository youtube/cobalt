// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PLAYER_CRYPTO_KEY_SYSTEMS_H_
#define MEDIA_PLAYER_CRYPTO_KEY_SYSTEMS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

namespace media {

// Returns whether |key_sytem| is supported at all.
// Call IsSupportedKeySystemWithMediaMimeType() to determine whether a
// |key_system| supports a specific type of media.
bool IsSupportedKeySystem(const std::string& key_system);

// Returns whether |key_sytem| supports the specified media type and codec(s).
bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system);

// Returns a name for |key_system| suitable to UMA logging.
std::string KeySystemNameForUMA(const std::string& key_system);

// Returns whether AesDecryptor can be used for the given |key_system|.
bool CanUseAesDecryptor(const std::string& key_system);

// Returns the plugin type given a |key_system|.
// Returns an empty string if no plugin type is found for |key_system|.
std::string GetPluginType(const std::string& key_system);

}  // namespace media

#endif  // MEDIA_PLAYER_CRYPTO_KEY_SYSTEMS_H_
