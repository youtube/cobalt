// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/mojom/encryption_pattern_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media_m96::mojom::EncryptionPatternDataView,
                  media_m96::EncryptionPattern>::
    Read(media_m96::mojom::EncryptionPatternDataView input,
         media_m96::EncryptionPattern* output) {
  *output = media_m96::EncryptionPattern(input.crypt_byte_block(),
                                     input.skip_byte_block());
  return true;
}

}  // namespace mojo
