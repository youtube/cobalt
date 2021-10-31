// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// SbDirectoryClose is well-covered in all the other tests, so just the
// leftovers are here.

#include <string>

#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSystemSignWithCertificationSecretKeyTest, DoesNotCrash) {
  const size_t kDigestSize = 32;
  uint8_t digest[kDigestSize];

  std::string message("test_message");
  SbSystemSignWithCertificationSecretKey(
      reinterpret_cast<const uint8_t*>(message.data()), message.size(), digest,
      kDigestSize);
}

// Since SbSystemSignWithCertificationSecretKey() uses the SHA256 algorithm,
// the output digest size must be at least 256 bits, or 32 bytes.
TEST(SbSystemSignWithCertificationSecretKeyTest,
     RainyDayFailsOnLessThan32Bytes) {
  const size_t kDigestSize = 32;
  uint8_t digest[kDigestSize];

  std::string message("test_message");
  EXPECT_FALSE(SbSystemSignWithCertificationSecretKey(
      reinterpret_cast<const uint8_t*>(message.data()), message.size(), digest,
      31));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
