// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_ENCRYPTION_PATTERN_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_ENCRYPTION_PATTERN_MOJOM_TRAITS_H_

#include "third_party/chromium/media/base/encryption_pattern.h"
#include "third_party/chromium/media/base/ipc/media_param_traits.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::EncryptionPatternDataView,
                    media_m96::EncryptionPattern> {
  static uint32_t crypt_byte_block(const media_m96::EncryptionPattern& input) {
    return input.crypt_byte_block();
  }

  static uint32_t skip_byte_block(const media_m96::EncryptionPattern& input) {
    return input.skip_byte_block();
  }

  static bool Read(media_m96::mojom::EncryptionPatternDataView input,
                   media_m96::EncryptionPattern* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_ENCRYPTION_PATTERN_MOJOM_TRAITS_H_
