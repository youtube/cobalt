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

#include "cobalt/subtlecrypto/crypto_key.h"
#include <algorithm>

namespace cobalt {
namespace subtlecrypto {

const script::Sequence<KeyUsage>& CryptoKey::usages() { return usages_; }

bool CryptoKey::set_usages(const script::Sequence<KeyUsage>& usages) {
  usages_.clear();
  // TODO: Key usage checking should actually be done per algorithm.
  for (auto& usage : usages) {
    switch (usage) {
      case KeyUsage::kKeyUsageEncrypt:
      case KeyUsage::kKeyUsageDecrypt:
      case KeyUsage::kKeyUsageSign:
      case KeyUsage::kKeyUsageVerify:
        usages_.push_back(usage);
      default:
        break;
    }
  }
  // If all elements were copied, all were valid.
  return usages_.size() == usages.size();
}

bool CryptoKey::usage(const KeyUsage& usage) const {
  return std::count(usages_.begin(), usages_.end(), usage) > 0;
}

}  // namespace subtlecrypto

}  // namespace cobalt
