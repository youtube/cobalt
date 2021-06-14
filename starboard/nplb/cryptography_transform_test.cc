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

#include "starboard/common/log.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/nplb/cryptography_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class Aes : public ::testing::TestWithParam<SbCryptographyBlockCipherMode> {
 protected:
  SbCryptographyBlockCipherMode GetMode() { return GetParam(); }

  bool ShouldSetIvAtInitialization() {
    return GetIvMode() == kIvModeSetAtInitialization ||
           GetIvMode() == kIvModeSetAnyTime;
  }

  bool ShouldSetIvAfterInitialization() {
    return GetIvMode() == kIvModeSetAfterInitialization ||
           GetIvMode() == kIvModeSetAnyTime;
  }

  bool ShouldSetAuthenticatedData() {
    switch (GetMode()) {
      case kSbCryptographyBlockCipherModeGcm:
        return true;
      case kSbCryptographyBlockCipherModeEcb:
      case kSbCryptographyBlockCipherModeCtr:
      case kSbCryptographyBlockCipherModeCbc:
      case kSbCryptographyBlockCipherModeCfb:
      case kSbCryptographyBlockCipherModeOfb:
        return false;
      default:
        ADD_FAILURE() << "Unrecognized SbCryptographyBlockCipherMode "
                      << GetMode();
        return false;
    }
  }

  const char* GetExpectedEncryptedValueHex() {
    switch (GetMode()) {
      case kSbCryptographyBlockCipherModeGcm:
        return "6a7b6e871f851db21a29f6aef579cfb77238d34b2f099404f20bb44c6b2d4d2"
               "0a2788f8ffc0a36eba4cd8e69ee95e973b5eb52d1e218ee991ccc0ee1ba2fa7"
               "d1dc83a5d2e3c317e5637b67c2524bd073ec6fe547edf6044111e2e16c239ed"
               "e845d4b7b235f24fbdb2673dbdd1c5d5c46d7b69059ff4b1566b01bec4f4ca6"
               "a4c500";

      case kSbCryptographyBlockCipherModeEcb:
        return "36aa40499dccc9f25bb3c1abcf4b73cf5993dc6127c534e7491a31a54f09bc4"
               "8e9fc157e380ca1457dedac9147e158be1c8eba2ec679d6bdc54cb2878cd2ab"
               "6736aa40499dccc9f25bb3c1abcf4b73cf5993dc6127c534e7491a31a54f09b"
               "c48e9fc157e380ca1457dedac9147e158beae602e043e0572ed2f71acf5438c"
               "e78200";

      case kSbCryptographyBlockCipherModeCtr:
        return "f32f8a813cea30e0f4c6e2efeab52ab92976cc06fa5f69409c3358ef2f3782d"
               "daff201f56acd9b61fea4c3e4739deab176ad764da16d1248d59337962c7ab0"
               "f2cd4c292ed34ae4125d8fb142344b2d2fc4ff533b80f5e8e311cde700ceaed"
               "a613c6f27f72a0602e6e4891e6b4f5a0a64e795998a40544d88363d124d5294"
               "c91700";

      case kSbCryptographyBlockCipherModeCbc:
        return "eaa929d637cde55115d43257e920ff8d39d1dec8240255a5db79a0cf79501a1"
               "4bc62c4d16fc45dd7fa9d15b3346a74301260dfeb96e22259787344d7e47047"
               "3eb4187eeac16a74afd1c5fdc67fb145cc494667ea4b16dccc83cc1cc1b2c36"
               "2f2a03e90d6c3a9adbf53c4cb8e4987719928ef7c47c4403f0cfe5a0ff77325"
               "6eaa00";

      case kSbCryptographyBlockCipherModeCfb:
      case kSbCryptographyBlockCipherModeOfb:
        ADD_FAILURE() << "Unsupported SbCryptographyBlockCipherMode "
                      << GetMode();
        return "";
      default:
        ADD_FAILURE() << "Unrecognized SbCryptographyBlockCipherMode "
                      << GetMode();
        return "";
    }
  }

 private:
  enum IvMode {
    kIvModeNotUsed,
    kIvModeSetAtInitialization,
    kIvModeSetAfterInitialization,
    kIvModeSetAnyTime,
  };

  IvMode GetIvMode() {
    switch (GetMode()) {
      case kSbCryptographyBlockCipherModeEcb:
        return kIvModeNotUsed;
      case kSbCryptographyBlockCipherModeGcm:
        return kIvModeSetAfterInitialization;
      case kSbCryptographyBlockCipherModeCtr:
        return kIvModeSetAnyTime;
      case kSbCryptographyBlockCipherModeCbc:
      case kSbCryptographyBlockCipherModeCfb:
      case kSbCryptographyBlockCipherModeOfb:
        return kIvModeSetAtInitialization;
      default:
        ADD_FAILURE() << "Unrecognized SbCryptographyBlockCipherMode "
                      << GetMode();
        return kIvModeNotUsed;
    }
  }
};

const int kBlockSizeBits = 128;
const int kBlockSizeBytes = kBlockSizeBits / 8;

const char kClearText[] =
    "This test text is designed to be a multiple of 128 bits, huzzah-"
    "This test text is designed to be a multiple of 128 bits, huzzah!";
const char kAdditionalDataString[] = "000000000000000000000000";
const char kInitializationVector[kBlockSizeBytes + 1] = "0123456789ABCDEF";
const char kKey[kBlockSizeBytes + 1] = "Rijndael";

TEST_P(Aes, SunnyDayIdentity) {
  SbCryptographyBlockCipherMode mode = GetMode();

  SbCryptographyTransformer encrypter = SbCryptographyCreateTransformer(
      kSbCryptographyAlgorithmAes, kBlockSizeBits,
      kSbCryptographyDirectionEncode, mode,
      ShouldSetIvAtInitialization() ? kInitializationVector : NULL,
      ShouldSetIvAtInitialization() ? kBlockSizeBytes : 0, kKey,
      kBlockSizeBytes);

  if (!SbCryptographyIsTransformerValid(encrypter)) {
    SB_LOG(WARNING) << "Skipping test, as there is no implementation.";
    // Test over if there's no implementation.
    return;
  }

  if (ShouldSetIvAfterInitialization()) {
    SbCryptographySetInitializationVector(encrypter, kInitializationVector,
                                          kBlockSizeBytes);
  }

  if (ShouldSetAuthenticatedData()) {
    scoped_array<uint8_t> aad;
    int aad_len = 0;
    DecodeHex(&aad, &aad_len, kAdditionalDataString, GetMode(), "aad");
    SbCryptographySetAuthenticatedData(encrypter, aad.get(), aad_len);
  }

  const int kInputSize = static_cast<int>(strlen(kClearText));
  const int kBufferSize = static_cast<int>(sizeof(kClearText));
  char* cipher_text = new char[kBufferSize];
  memset(cipher_text, 0, kBufferSize);
  int count =
      SbCryptographyTransform(encrypter, kClearText, kInputSize, cipher_text);
  EXPECT_EQ(kInputSize, count);
  EXPECT_NE(0, strncmp(kClearText, cipher_text, kBufferSize));

  EXPECT_STREQ(GetExpectedEncryptedValueHex(),
               HexDump(cipher_text, kBufferSize).c_str());

  SbCryptographyTransformer decrypter = SbCryptographyCreateTransformer(
      kSbCryptographyAlgorithmAes, kBlockSizeBits,
      kSbCryptographyDirectionDecode, mode,
      ShouldSetIvAtInitialization() ? kInitializationVector : NULL,
      ShouldSetIvAtInitialization() ? kBlockSizeBytes : 0, kKey,
      kBlockSizeBytes);

  ASSERT_TRUE(SbCryptographyIsTransformerValid(decrypter))
      << "Cryptographic support for a set of parameters must be symmetrical.";

  if (ShouldSetIvAfterInitialization()) {
    SbCryptographySetInitializationVector(decrypter, kInitializationVector,
                                          kBlockSizeBytes);
  }

  if (ShouldSetAuthenticatedData()) {
    scoped_array<uint8_t> aad;
    int aad_len = 0;
    DecodeHex(&aad, &aad_len, kAdditionalDataString, GetMode(), "aad");
    SbCryptographySetAuthenticatedData(decrypter, aad.get(), aad_len);
  }

  char* decrypted_text = new char[kBufferSize];
  memset(decrypted_text, 0, kBufferSize);
  count = SbCryptographyTransform(decrypter, cipher_text, kInputSize,
                                  decrypted_text);

  EXPECT_EQ(kInputSize, count);
  EXPECT_EQ(kInputSize, strlen(decrypted_text));
  EXPECT_STREQ(kClearText, decrypted_text);

  delete[] decrypted_text;
  delete[] cipher_text;
  SbCryptographyDestroyTransformer(decrypter);
  SbCryptographyDestroyTransformer(encrypter);
}

INSTANTIATE_TEST_CASE_P(SbCryptographyTransform,
                        Aes,
                        ::testing::Values(kSbCryptographyBlockCipherModeCbc,
                                          kSbCryptographyBlockCipherModeCfb,
                                          kSbCryptographyBlockCipherModeCtr,
                                          kSbCryptographyBlockCipherModeEcb,
                                          kSbCryptographyBlockCipherModeOfb,
                                          kSbCryptographyBlockCipherModeGcm));

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 12
