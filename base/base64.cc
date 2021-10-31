// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"

#include "starboard/types.h"
#include "third_party/modp_b64/modp_b64.h"

namespace base {

bool Base64Encode(const StringPiece& input, std::string* output) {
  std::string temp;
  temp.resize(modp_b64_encode_len(
      input.size()));  // makes room for null byte

#if defined(STARBOARD)
                       // null terminates result since result is base64 text!
  int input_size = static_cast<int>(input.size());

  // modp_b64_encode_len() returns at least 1, so temp[0] is safe to use.
  int output_size = modp_b64_encode(&(temp[0]), input.data(), input_size);
  if (output_size < 0)
    return false;
#else
  // modp_b64_encode_len() returns at least 1, so temp[0] is safe to use.
  size_t output_size = modp_b64_encode(&(temp[0]), input.data(), input.size());
#endif

  temp.resize(output_size);  // strips off null byte
  output->swap(temp);
  return true;
}

#if defined(STARBOARD)
// Cobalt uses different C++ types for different corresponding JavaScript types
// and we want to have the flexibility to base64 decode string into types like
// std::vector<uint8_t>.
template <class Container>
bool Base64DecodeInternal(const StringPiece& input, Container* output) {
  Container temp;
  temp.resize(modp_b64_decode_len(input.size()));

  int input_size = static_cast<int>(input.size());
  // When using this template for a new type, make sure its content is unsigned
  // char or char.
  static_assert(sizeof(typename Container::value_type) == 1,
                "Input type should be char or equivalent.");
  int output_size = modp_b64_decode(reinterpret_cast<char*>(&(temp[0])),
                                    input.data(), input_size);
  if (output_size < 0)
    return false;

  temp.resize(output_size);
  output->swap(temp);
  return true;
}

bool Base64Decode(const StringPiece& input, std::string* output) {
  return Base64DecodeInternal(input, output);
}

bool Base64Decode(const StringPiece& input, std::vector<uint8_t>* output) {
  return Base64DecodeInternal(input, output);
}
#else
bool Base64Decode(const StringPiece& input, std::string* output) {
  std::string temp;
  temp.resize(modp_b64_decode_len(input.size()));

  // does not null terminate result since result is binary data!
  size_t input_size = input.size();
  int output_size = modp_b64_decode(&(temp[0]), input.data(), input_size);
  if (output_size == MODP_B64_ERROR)
    return false;

  temp.resize(output_size);
  output->swap(temp);
  return true;
}
#endif  // STARBOARD

}  // namespace base
