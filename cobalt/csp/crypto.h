// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSP_CRYPTO_H_
#define COBALT_CSP_CRYPTO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace cobalt {
namespace csp {

enum HashAlgorithm {
  kHashAlgorithmNone = 0,
  kHashAlgorithmSha1 = 1 << 1,
  kHashAlgorithmSha256 = 1 << 2,
  kHashAlgorithmSha384 = 1 << 3,
  kHashAlgorithmSha512 = 1 << 4,
};

static const int kMaxDigestSize = 64;
typedef std::vector<uint8> DigestValue;

bool ComputeDigest(HashAlgorithm algorithm, const char* digestable,
                   size_t length, DigestValue* digest_result);

}  // namespace csp
}  // namespace cobalt

#endif  // COBALT_CSP_CRYPTO_H_
