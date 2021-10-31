/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ==================================================================== */

// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// This test is adapted from BoringSSL's crypto/fipsmodule/modes/gcm_test.cc

#include "starboard/configuration.h"

#if SB_API_VERSION < 12

#include "starboard/cryptography.h"

#include "starboard/common/log.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/nplb/cryptography_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NotNull;

namespace starboard {
namespace nplb {
namespace {

const int kBlockSizeBits = 128;
const int kBlockSizeBytes = kBlockSizeBits / 8;

struct test_case {
  const char *key;
  const char *plaintext;
  const char *additional_data;
  const char *nonce;
  const char *ciphertext;
  const char *tag;
};

const struct test_case test_cases[] = {
  {
    "00000000000000000000000000000000",
    NULL,
    NULL,
    "000000000000000000000000",
    NULL,
    "58e2fccefa7e3061367f1d57a4e7455a",
  },
  {
    "00000000000000000000000000000000",
    "00000000000000000000000000000000",
    NULL,
    "000000000000000000000000",
    "0388dace60b6a392f328c2b971b2fe78",
    "ab6e47d42cec13bdf53a67b21257bddf",
  },
  {
    "feffe9928665731c6d6a8f9467308308",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255",
    NULL,
    "cafebabefacedbaddecaf888",
    "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b254"
    "66931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985",
    "4d5c2af327cd64a62cf35abd2ba6fab4",
  },
  {
    "feffe9928665731c6d6a8f9467308308",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "cafebabefacedbaddecaf888",
    "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b254"
    "66931c7d8f6a5aac84aa051ba30b396a0aac973d58e091",
    "5bc94fbc3221a5db94fae95ae7121a47",
  },
  {
    "feffe9928665731c6d6a8f9467308308",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "cafebabefacedbad",
    "61353b4c2806934a777ff51fa22a4755699b2a714fcdc6f83766e5f97b6c742373806900e4"
    "9f24b22b097544d4896b424989b5e1ebac0f07c23f4598",
    "3612d2e79e3b0785561be14aaca2fccb",
  },
  {
    "feffe9928665731c6d6a8f9467308308",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728c3c0c95156"
    "809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b",
    "8ce24998625615b603a033aca13fb894be9112a5c3a211a8ba262a3cca7e2ca701e4a9a4fb"
    "a43c90ccdcb281d48c7c6fd62875d2aca417034c34aee5",
    "619cc5aefffe0bfa462af43c1699d050",
  },
  {
    "000000000000000000000000000000000000000000000000",
    NULL,
    NULL,
    "000000000000000000000000",
    NULL,
    "cd33b28ac773f74ba00ed1f312572435",
  },
  {
    "000000000000000000000000000000000000000000000000",
    "00000000000000000000000000000000",
    NULL,
    "000000000000000000000000",
    "98e7247c07f0fe411c267e4384b0f600",
    "2ff58d80033927ab8ef4d4587514f0fb",
  },
  {
    "feffe9928665731c6d6a8f9467308308feffe9928665731c",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255",
    NULL,
    "cafebabefacedbaddecaf888",
    "3980ca0b3c00e841eb06fac4872a2757859e1ceaa6efd984628593b40ca1e19c7d773d00c1"
    "44c525ac619d18c84a3f4718e2448b2fe324d9ccda2710acade256",
    "9924a7c8587336bfb118024db8674a14",
  },
  {
    "feffe9928665731c6d6a8f9467308308feffe9928665731c",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "cafebabefacedbaddecaf888",
    "3980ca0b3c00e841eb06fac4872a2757859e1ceaa6efd984628593b40ca1e19c7d773d00c1"
    "44c525ac619d18c84a3f4718e2448b2fe324d9ccda2710",
    "2519498e80f1478f37ba55bd6d27618c",
  },
  {
    "feffe9928665731c6d6a8f9467308308feffe9928665731c",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "cafebabefacedbad",
    "0f10f599ae14a154ed24b36e25324db8c566632ef2bbb34f8347280fc4507057fddc29df9a"
    "471f75c66541d4d4dad1c9e93a19a58e8b473fa0f062f7",
    "65dcc57fcf623a24094fcca40d3533f8",
  },
  {
    "feffe9928665731c6d6a8f9467308308feffe9928665731c",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "cafebabefacedbad",
    "0f10f599ae14a154ed24b36e25324db8c566632ef2bbb34f8347280fc4507057fddc29df9a"
    "471f75c66541d4d4dad1c9e93a19a58e8b473fa0f062f7",
    "65dcc57fcf623a24094fcca40d3533f8",
  },
  {
    "feffe9928665731c6d6a8f9467308308feffe9928665731c",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728c3c0c95156"
    "809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b",
    "d27e88681ce3243c4830165a8fdcf9ff1de9a1d8e6b447ef6ef7b79828666e4581e79012af"
    "34ddd9e2f037589b292db3e67c036745fa22e7e9b7373b",
    "dcf566ff291c25bbb8568fc3d376a6d9",
  },
  {
    "0000000000000000000000000000000000000000000000000000000000000000",
    NULL,
    NULL,
    "000000000000000000000000",
    NULL,
    "530f8afbc74536b9a963b4f1c4cb738b",
  },
  {
    "0000000000000000000000000000000000000000000000000000000000000000",
    "00000000000000000000000000000000",
    NULL,
    "000000000000000000000000",
    "cea7403d4d606b6e074ec5d3baf39d18",
    "d0d1c8a799996bf0265b98b5d48ab919",
  },
  {
    "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255",
    NULL,
    "cafebabefacedbaddecaf888",
    "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e4859"
    "0dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015ad",
    "b094dac5d93471bdec1a502270e3cc6c",
  },
  {
    "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "cafebabefacedbaddecaf888",
    "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e4859"
    "0dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662",
    "76fc6ece0f4e1768cddf8853bb2d551b",
  },
  {
    "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "cafebabefacedbad",
    "c3762df1ca787d32ae47c13bf19844cbaf1ae14d0b976afac52ff7d79bba9de0feb582d339"
    "34a4f0954cc2363bc73f7862ac430e64abe499f47c9b1f",
    "3a337dbf46a792c45e454913fe2ea8f2",
  },
  {
    "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
    "feedfacedeadbeeffeedfacedeadbeefabaddad2",
    "9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728c3c0c95156"
    "809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b",
    "5a8def2f0c9e53f1f75d7853659e2a20eeb2b22aafde6419a058ab4f6f746bf40fc0c3b780"
    "f244452da3ebf1c5d82cdea2418997200ef82e44ae7e3f",
    "a44a8266ee1c8eb0c8b5d4cf5ae9f19a",
  },
  {
    "00000000000000000000000000000000",
    NULL,
    "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c9595"
    "6809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255522dc1f099567d07f47f"
    "37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b10568288"
    "38c5f61e6393ba7a0abcc9f662898015ad",
    "000000000000000000000000",
    NULL,
    "5fea793a2d6f974d37e68e0cb8ff9492",
  },
  {
    "00000000000000000000000000000000",
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000",
    NULL,
    /* This nonce results in 0xfff in counter LSB. */
    "ffffffff000000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000000000",
    "56b3373ca9ef6e4a2b64fe1e9a17b61425f10d47a75a5fce13efc6bc784af24f4141bdd48c"
    "f7c770887afd573cca5418a9aeffcd7c5ceddfc6a78397b9a85b499da558257267caab2ad0"
    "b23ca476a53cb17fb41c4b8b475cb4f3f7165094c229c9e8c4dc0a2a5ff1903e5015112213"
    "76a1cdb8364c5061a20cae74bc4acd76ceb0abc9fd3217ef9f8c90be402ddf6d8697f4f880"
    "dff15bfb7a6b28241ec8fe183c2d59e3f9dfff653c7126f0acb9e64211f42bae12af462b10"
    "70bef1ab5e3606872ca10dee15b3249b1a1b958f23134c4bccb7d03200bce420a2f8eb66dc"
    "f3644d1423c1b5699003c13ecef4bf38a3b60eedc34033bac1902783dc6d89e2e774188a43"
    "9c7ebcc0672dbda4ddcfb2794613b0be41315ef778708a70ee7d75165c",
    "8b307f6b33286d0ab026a9ed3fe1e85f",
  },
};

class Gcm
    : public ::testing::TestWithParam<int> {
 public:
  int GetTestCase() { return GetParam(); }
};

TEST_P(Gcm, TestCase) {
  int test_num = GetTestCase();
  const struct test_case *test = &test_cases[test_num];

  int key_len;
  scoped_array<uint8_t> key;
  int plaintext_len;
  scoped_array<uint8_t> plaintext;
  int additional_data_len;
  scoped_array<uint8_t> additional_data;
  int nonce_len;
  scoped_array<uint8_t> nonce;
  int ciphertext_len;
  scoped_array<uint8_t> ciphertext;
  int tag_len;
  scoped_array<uint8_t> tag;
  scoped_array<uint8_t> out;

  DecodeHex(&key, &key_len, test->key, test_num, "key");
  DecodeHex(&plaintext, &plaintext_len, test->plaintext, test_num,
             "plaintext");
  DecodeHex(&additional_data, &additional_data_len,
             test->additional_data, test_num, "additional_data");
  DecodeHex(&nonce, &nonce_len, test->nonce, test_num, "nonce");
  DecodeHex(&ciphertext, &ciphertext_len, test->ciphertext, test_num,
             "ciphertext");
  DecodeHex(&tag, &tag_len, test->tag, test_num, "tag");

  if (plaintext_len != ciphertext_len) {
    FAIL() << "Plaintext and ciphertext have differing lengths.";
  }

  if (key_len != 16 && key_len != 24 && key_len != 32) {
    FAIL() << "Bad key length.";
  }

  if (tag_len != 16) {
    FAIL() << "Bad tag length.";
  }

  out.reset(new uint8_t[plaintext_len]);
  if (plaintext_len != 0 && out == NULL) {
    FAIL() << "Out of memory.";
  }

  // Test encryption.

  // Try to create a transformer for GCM.
  SbCryptographyTransformer encrypter = SbCryptographyCreateTransformer(
      kSbCryptographyAlgorithmAes, kBlockSizeBits,
      kSbCryptographyDirectionEncode, kSbCryptographyBlockCipherModeGcm,
      NULL, 0, key.get(), key_len);

  if (!SbCryptographyIsTransformerValid(encrypter)) {
    // Test over (but no failure) if there's no implementation.
    return;
  }

  SbCryptographySetInitializationVector(encrypter, nonce.get(), nonce_len);
  memset(out.get(), 0, plaintext_len);
  if (additional_data) {
    SbCryptographySetAuthenticatedData(encrypter, additional_data.get(),
                                       additional_data_len);
  }

  if (plaintext) {
    EXPECT_EQ(plaintext_len,
              SbCryptographyTransform(encrypter, plaintext.get(), plaintext_len,
                                      out.get()));
  }

  scoped_array<uint8_t> actual_tag(new uint8_t[tag_len]);
  memset(actual_tag.get(), 0, tag_len);
  SbCryptographyGetTag(encrypter, actual_tag.get(), tag_len);
  if (tag) {
    EXPECT_STREQ(HexDump(tag.get(), tag_len).c_str(),
                 HexDump(actual_tag.get(), tag_len).c_str());
  }

  if (ciphertext) {
    EXPECT_STREQ(HexDump(ciphertext.get(), plaintext_len).c_str(),
                 HexDump(out.get(), plaintext_len).c_str());
  }

  SbCryptographyDestroyTransformer(encrypter);

  // Test decryption.

  SbCryptographyTransformer decrypter = SbCryptographyCreateTransformer(
      kSbCryptographyAlgorithmAes, kBlockSizeBits,
      kSbCryptographyDirectionDecode, kSbCryptographyBlockCipherModeGcm,
      NULL, 0, key.get(), key_len);
  ASSERT_THAT(decrypter, NotNull());

  SbCryptographySetInitializationVector(decrypter, nonce.get(), nonce_len);
  memset(out.get(), 0, plaintext_len);
  if (additional_data) {
    SbCryptographySetAuthenticatedData(decrypter, additional_data.get(),
                                       additional_data_len);
  }

  if (ciphertext) {
    EXPECT_EQ(plaintext_len,
              SbCryptographyTransform(decrypter, ciphertext.get(),
                                      plaintext_len, out.get()));
  }

  actual_tag.reset(new uint8_t[tag_len]);
  memset(actual_tag.get(), 0, tag_len);
  SbCryptographyGetTag(decrypter, actual_tag.get(), tag_len);
  if (tag) {
    EXPECT_STREQ(HexDump(tag.get(), tag_len).c_str(),
                 HexDump(actual_tag.get(), tag_len).c_str());
  }

  if (plaintext) {
    EXPECT_STREQ(HexDump(plaintext.get(), plaintext_len).c_str(),
                 HexDump(out.get(), plaintext_len).c_str());
  }
  SbCryptographyDestroyTransformer(decrypter);
}

INSTANTIATE_TEST_CASE_P(
    SbCryptographyTransform,
    Gcm,
    ::testing::Range(0, SB_ARRAY_SIZE_INT(test_cases)));

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 12
