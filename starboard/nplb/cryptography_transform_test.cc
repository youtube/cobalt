// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/cryptography.h"

#include "starboard/log.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 4

namespace starboard {
namespace nplb {
namespace {

const int kBlockSizeBits = 128;
const int kBlockSizeBytes = kBlockSizeBits / 8;

TEST(SbCryptographyTransform, SunnyDay) {
  char initialization_vector[kBlockSizeBytes + 1] = "0123456789ABCDEF";
  char key[kBlockSizeBytes + 1] = "RijndaelRijndael";
  const char kClearText[] =
      "This test text is designed to be a multiple of "
      "128 bits, huzzah.";

  // Try to create a transformer for the most likely algorithm to be supported:
  // AES-128-CBC
  SbCryptographyTransformer transformer = SbCryptographyCreateTransformer(
      kSbCryptographyAlgorithmAes, kBlockSizeBits,
      kSbCryptographyDirectionEncode, kSbCryptographyBlockCipherModeCbc,
      initialization_vector, kBlockSizeBytes, key, kBlockSizeBytes);

  if (!SbCryptographyIsTransformerValid(transformer)) {
    // Test over if there's no implementation.
    return;
  }

  const int kBufferSize = sizeof(kClearText) - 1;
  char* cipher_text = new char[kBufferSize];
  SbMemorySet(cipher_text, 0, kBufferSize);
  int count = SbCryptographyTransform(transformer, kClearText, kBufferSize,
                                      cipher_text);
  EXPECT_EQ(kBufferSize, count);
  EXPECT_NE(0, SbStringCompare(kClearText, cipher_text, kBufferSize));

  SbCryptographyTransformer decode_transformer =
      SbCryptographyCreateTransformer(
          kSbCryptographyAlgorithmAes, kBlockSizeBits,
          kSbCryptographyDirectionDecode, kSbCryptographyBlockCipherModeCbc,
          initialization_vector, kBlockSizeBytes, key, kBlockSizeBytes);

  ASSERT_TRUE(SbCryptographyIsTransformerValid(decode_transformer));

  char* decrypted_text = new char[kBufferSize];
  SbMemorySet(decrypted_text, 0, kBufferSize);
  count =
      SbCryptographyTransform(decode_transformer, cipher_text, kBufferSize,
                              decrypted_text);

  EXPECT_EQ(kBufferSize, count);
  EXPECT_EQ(kBufferSize, SbStringGetLength(decrypted_text));
  EXPECT_STREQ(kClearText, decrypted_text);

  delete[] decrypted_text;
  delete[] cipher_text;
  SbCryptographyDestroyTransformer(decode_transformer);
  SbCryptographyDestroyTransformer(transformer);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION >= 4
