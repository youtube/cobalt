// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"

#include "starboard/types.h"
#include "third_party/modp_b64/modp_b64.h"

namespace base {

void Base64Encode(const StringPiece& input, std::string* output) {
  std::string temp;
  temp.resize(modp_b64_encode_len(input.size()));  // makes room for null byte

  // modp_b64_encode_len() returns at least 1, so temp[0] is safe to use.
  size_t output_size = modp_b64_encode(&(temp[0]), input.data(), input.size());

  temp.resize(output_size);  // strips off null byte
  output->swap(temp);
}

bool Base64Decode(const StringPiece& input, std::string* output) {
  std::string temp;
  temp.resize(modp_b64_decode_len(input.size()));

  // does not null terminate result since result is binary data!
  size_t input_size = input.size();
  int output_size = modp_b64_decode(&(temp[0]), input.data(), input_size);
#if defined(STARBOARD)
  // Cobalt has a slightly different modp_b64 version.
  if (output_size < 0)
#else
  if (output_size == MODP_B64_ERROR)
#endif
    return false;

  temp.resize(output_size);
  output->swap(temp);
  return true;
}

}  // namespace base
