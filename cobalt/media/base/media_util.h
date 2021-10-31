// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_MEDIA_UTIL_H_
#define COBALT_MEDIA_BASE_MEDIA_UTIL_H_

#include <vector>

#include "cobalt/media/base/encryption_scheme.h"
#include "cobalt/media/base/media_export.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// Simply returns an empty vector. {Audio|Video}DecoderConfig are often
// constructed with empty extra data.
MEDIA_EXPORT std::vector<uint8_t> EmptyExtraData();

// The following helper functions return new instances of EncryptionScheme that
// indicate widely used settings.
MEDIA_EXPORT EncryptionScheme Unencrypted();
MEDIA_EXPORT EncryptionScheme AesCtrEncryptionScheme();

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_MEDIA_UTIL_H_
