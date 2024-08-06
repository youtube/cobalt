// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/base/key_system_properties.h"

namespace media_m96 {

SupportedCodecs KeySystemProperties::GetSupportedHwSecureCodecs() const {
  return EME_CODEC_NONE;
}

bool KeySystemProperties::UseAesDecryptor() const {
  return false;
}

}  // namespace media_m96
