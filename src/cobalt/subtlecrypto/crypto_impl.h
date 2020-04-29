// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SUBTLECRYPTO_CRYPTO_IMPL_H_
#define COBALT_SUBTLECRYPTO_CRYPTO_IMPL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cobalt {
namespace subtlecrypto {

using ByteVector = std::vector<uint8_t>;

class Hash {
 public:
  virtual ~Hash() {}
  // Hash input data
  virtual void Update(const ByteVector &data) = 0;
  // Finish hash, output is hash-length vector
  virtual ByteVector Finish() = 0;
  // Calculate HMAC with given key
  virtual ByteVector CalculateHMAC(const ByteVector &data,
                                   const ByteVector &key) = 0;
  static std::unique_ptr<Hash> CreateByName(const std::string &name);
};

// Calculate AES-CTR, selecting the correct algorithm based on |key| length.
// Returns an empty vector in case of failure.
ByteVector CalculateAES_CTR(const ByteVector &data, const ByteVector &key,
                            const ByteVector &iv);

}  // namespace subtlecrypto
}  // namespace cobalt

#endif  // COBALT_SUBTLECRYPTO_CRYPTO_IMPL_H_
