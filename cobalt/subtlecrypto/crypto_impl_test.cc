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

#include "cobalt/subtlecrypto/crypto_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace subtlecrypto {
namespace {

ByteVector empty_ref{218, 57,  163, 238, 94, 107, 75,  13,  50, 85,
                     191, 239, 149, 96,  24, 144, 175, 216, 7,  9};

using testing::ContainerEq;

TEST(HashImplTest, CreateByName) {
  EXPECT_TRUE(Hash::CreateByName("SHA-1"));
  EXPECT_TRUE(Hash::CreateByName("sha-1"));
  EXPECT_TRUE(Hash::CreateByName("SHA-256"));
  EXPECT_TRUE(Hash::CreateByName("sha-512"));
  EXPECT_FALSE(Hash::CreateByName("md5"));
  EXPECT_FALSE(Hash::CreateByName("SHA1"));
  EXPECT_FALSE(Hash::CreateByName("BlowFish"));
}

TEST(HashImplTest, NullHash) {
  auto hash = Hash::CreateByName("SHA-1");
  auto out = hash->Finish();
  EXPECT_THAT(out, ContainerEq(empty_ref));
}

TEST(HashImplTest, EmptyHash) {
  auto hash = Hash::CreateByName("SHA-1");
  hash->Update({});
  auto out = hash->Finish();
  EXPECT_THAT(out, ContainerEq(empty_ref));
}

TEST(HashImplTest, BasicHash) {
  auto hash = Hash::CreateByName("SHA-1");
  hash->Update({1, 2, 3});
  auto out = hash->Finish();
  EXPECT_THAT(out, testing::ContainerEq(ByteVector{
                       112, 55, 128, 113, 152, 194, 42,  125, 43,  8,
                       7,   55, 29,  118, 55,  121, 168, 79,  223, 207}));
}

TEST(HashImplTest, LongHash) {
  auto hash = Hash::CreateByName("SHA-512");
  hash->Update({'c', 'o', 'b', 'a', 'l', 't'});
  auto out = hash->Finish();
  EXPECT_THAT(
      out, ContainerEq(ByteVector{
               151, 169, 180, 36,  213, 102, 227, 224, 165, 138, 100, 210, 91,
               158, 228, 19,  41,  40,  161, 153, 173, 66,  171, 83,  20,  245,
               230, 110, 197, 4,   211, 110, 253, 105, 233, 138, 42,  70,  242,
               210, 45,  70,  247, 103, 5,   145, 228, 142, 197, 56,  73,  160,
               210, 95,  81,  144, 51,  173, 207, 21,  183, 234, 15,  226}));
}

TEST(HMACImplTest, SimpleHmac) {
  auto hash = Hash::CreateByName("SHA-1");
  auto out = hash->CalculateHMAC({1, 2, 3}, {3, 4, 5});
  EXPECT_THAT(out, ContainerEq(ByteVector{253, 95,  161, 139, 138, 122, 106,
                                          143, 19,  9,   188, 104, 215, 248,
                                          12,  112, 14,  38,  175, 198}));
}

TEST(HMACImplTest, LongHmac) {
  auto hash = Hash::CreateByName("SHA-256");
  auto out = hash->CalculateHMAC({1, 2, 3}, {3, 4, 5});
  EXPECT_THAT(out, ContainerEq(ByteVector{
                       12,  25,  8,   57,  149, 183, 80,  251, 83, 218, 165,
                       55,  132, 139, 243, 70,  107, 18,  185, 64, 103, 13,
                       251, 57,  248, 156, 237, 253, 103, 133, 26, 39}));
}

TEST(HMACImplTest, EmptyHmac) {
  auto hash = Hash::CreateByName("SHA-1");
  auto out = hash->CalculateHMAC({}, {3, 4, 5});
  EXPECT_THAT(out, ContainerEq(ByteVector{137, 104, 209, 15,  163, 43,  226,
                                          236, 10,  29,  196, 233, 152, 219,
                                          179, 217, 186, 247, 111, 43}));
}

TEST(HMACImplTest, EmptyHmacKey) {
  auto hash = Hash::CreateByName("SHA-1");
  auto out = hash->CalculateHMAC({1, 2, 3}, {});
  EXPECT_THAT(out, ContainerEq(ByteVector{104, 1,   10,  108, 24,  33,  145,
                                          220, 224, 70,  243, 58,  196, 6,
                                          109, 163, 178, 181, 185, 37}));
}

ByteVector key_128 = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};
ByteVector key_192 = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3,
                      4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};
ByteVector key_256 = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7,
                      0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};

ByteVector iv_128 = {7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7};

TEST(AESImplTest, Simple) {
  auto out = CalculateAES_CTR({1, 2}, key_128, iv_128);
  EXPECT_THAT(out, ContainerEq(ByteVector{76, 8}));

  auto modified_key{key_128};
  modified_key[0] = 42;
  out = CalculateAES_CTR({1, 2}, modified_key, iv_128);
  EXPECT_THAT(out, ContainerEq(ByteVector{185, 59}));

  auto modified_iv{key_128};
  modified_iv[0] = 42;
  out = CalculateAES_CTR({1, 2}, key_128, modified_iv);
  EXPECT_THAT(out, ContainerEq(ByteVector{249, 68}));

  out = CalculateAES_CTR({1, 2}, key_192, iv_128);
  EXPECT_THAT(out, ContainerEq(ByteVector{225, 207}));

  out = CalculateAES_CTR({1, 2}, key_256, iv_128);
  EXPECT_THAT(out, ContainerEq(ByteVector{18, 233}));
}

TEST(AESImplTest, Long) {
  auto sizes = {1, 2, 16, 17, 31, 32, 33, 1025, 16385, 1028 * 1024 + 1};
  for (auto& size : sizes) {
    ByteVector data(size, 1);
    auto out = CalculateAES_CTR(data, key_128, iv_128);
    EXPECT_EQ(out.size(), size);
  }
}

TEST(AESImplTest, InvalidParams) {
  auto out = CalculateAES_CTR({1, 2}, {1}, iv_128);
  EXPECT_TRUE(out.empty());

  out = CalculateAES_CTR({1, 2}, key_128, {1, 2});
  EXPECT_TRUE(out.empty());
}

}  // namespace
}  // namespace subtlecrypto
}  // namespace cobalt
