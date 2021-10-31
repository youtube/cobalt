// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"

#if SB_API_VERSION < 12

#include "starboard/cryptography.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const int kBlockSizeBits = 128;
const int kBlockSizeBytes = kBlockSizeBits / 8;

TEST(SbCryptographyCreateTransformer, SunnyDay) {
  char initialization_vector[kBlockSizeBytes] = {0};
  char key[kBlockSizeBytes] = {0};

  // Try to create a transformer for the most likely algorithm to be supported:
  // AES-128-CBC
  SbCryptographyTransformer transformer = SbCryptographyCreateTransformer(
      kSbCryptographyAlgorithmAes, kBlockSizeBits,
      kSbCryptographyDirectionDecode, kSbCryptographyBlockCipherModeCbc,
      initialization_vector, kBlockSizeBytes, key, kBlockSizeBytes);

  if (!SbCryptographyIsTransformerValid(transformer)) {
    // Test over if there's no implementation.
    return;
  }

  // TODO: Validate implementation.
  SbCryptographyDestroyTransformer(transformer);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 12
