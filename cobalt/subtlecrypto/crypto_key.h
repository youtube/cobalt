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

#ifndef COBALT_SUBTLECRYPTO_CRYPTO_KEY_H_
#define COBALT_SUBTLECRYPTO_CRYPTO_KEY_H_

#include <string>
#include <vector>

#include "cobalt/script/union_type.h"
#include "cobalt/script/value_handle.h"

#include "cobalt/subtlecrypto/algorithm.h"
#include "cobalt/subtlecrypto/key_type.h"
#include "cobalt/subtlecrypto/key_usage.h"

namespace cobalt {
namespace subtlecrypto {

class CryptoKey : public base::SupportsWeakPtr<CryptoKey>,
                  public script::Wrappable {
 public:
  using AlgorithmIdentifier = script::UnionType2<Algorithm, std::string>;
  using KeyData = std::vector<uint8_t>;

  explicit CryptoKey(const std::string& algorithm) : algorithm_(algorithm) {}

  // Hardcoded to secret key, as RSA/DSA keys are not supported
  KeyType type() const { return KeyType::kKeyTypeSecret; }
  bool extractable() const { return false; }

  const AlgorithmIdentifier algorithm() const {
    return AlgorithmIdentifier(algorithm_);
  }
  const std::string& get_algorithm() const { return algorithm_; }

  void set_key(const KeyData& data) { key_ = data; }
  const KeyData& get_key() const { return key_; }

  void set_hash(const std::string& hash) { hash_ = hash; }
  const std::string& get_hash() const { return hash_; }

  const script::Sequence<KeyUsage>& usages();
  bool set_usages(const script::Sequence<KeyUsage>& usages);
  bool usage(const KeyUsage& usage) const;

  DEFINE_WRAPPABLE_TYPE(CryptoKey);

 private:
  KeyData key_;
  script::Sequence<KeyUsage> usages_;
  std::string algorithm_;
  std::string hash_;
};

}  // namespace subtlecrypto
}  // namespace cobalt

#endif  // COBALT_SUBTLECRYPTO_CRYPTO_KEY_H_
