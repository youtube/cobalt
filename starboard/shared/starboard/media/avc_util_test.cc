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

#include "starboard/shared/starboard/media/avc_util.h"

#include <vector>

#include "starboard/common/optional.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

using ::starboard::shared::starboard::media::ConvertAnnexBToAvcc;

const auto kAnnexB = AvcParameterSets::kAnnexB;
const auto kHeadless = AvcParameterSets::kHeadless;
const auto kAnnexBHeaderSizeInBytes =
    AvcParameterSets::kAnnexBHeaderSizeInBytes;
const uint8_t kSliceStartCode = 0x61;
const uint8_t kIdrStartCode = AvcParameterSets::kIdrStartCode;
const uint8_t kSpsStartCode = AvcParameterSets::kSpsStartCode;
const uint8_t kPpsStartCode = AvcParameterSets::kPpsStartCode;

const std::vector<uint8_t> kRawSlice = {kSliceStartCode, 0, 0, 1, 0, 0, 0};
const std::vector<uint8_t> kRawIdr = {kIdrStartCode, 1, 2, 3, 4};
const std::vector<uint8_t> kRawSps = {kSpsStartCode, 10, 11};
const std::vector<uint8_t> kRawPps = {kPpsStartCode, 20};

const std::vector<uint8_t> kNaluHeaderOnlyInAnnexB = {0, 0, 0, 1};
const std::vector<uint8_t> kSpsInAnnexB = {0, 0, 0, 1, kSpsStartCode, 10, 11};
const std::vector<uint8_t> kPpsInAnnexB = {0, 0, 0, 1, kPpsStartCode, 20};
const std::vector<uint8_t> kIdrInAnnexB = {0, 0, 0, 1, kIdrStartCode,
                                           1, 2, 3, 4};
const std::vector<uint8_t> kSliceInAnnexB = {0, 0, 0, 1, kSliceStartCode, 0, 0,
                                             1, 0, 0, 0};

std::vector<uint8_t> operator+(const std::vector<uint8_t>& left,
                               const std::vector<uint8_t>& right) {
  std::vector<uint8_t> result(left);
  result.insert(result.end(), right.begin(), right.end());
  return result;
}

std::vector<uint8_t> ToAnnexB(const std::vector<uint8_t>& nalu_body) {
  return std::vector<uint8_t>({0, 0, 0, 1}) + nalu_body;
}

std::vector<uint8_t> ToAvcc(const std::vector<uint8_t>& nalu_body) {
  std::vector<uint8_t> size(4);
  size[0] = static_cast<uint8_t>((nalu_body.size() & 0xff000000) >> 24);
  size[1] = static_cast<uint8_t>((nalu_body.size() & 0xff0000) >> 16);
  size[2] = static_cast<uint8_t>((nalu_body.size() & 0xff00) >> 8);
  size[3] = static_cast<uint8_t>(nalu_body.size() & 0xff);
  return size + nalu_body;
}

// Return a nalu that is guaranteed to be different than the input parameter.
std::vector<uint8_t> Mutate(const std::vector<uint8_t>& nalu_in_annex_b) {
  return nalu_in_annex_b + std::vector<uint8_t>({123});
}

std::vector<uint8_t> ConvertAnnexBToAvcc(
    const std::vector<uint8_t>& nalus_in_annex_b) {
  std::vector<uint8_t> nalus_in_avcc(nalus_in_annex_b.size());
  SB_CHECK(ConvertAnnexBToAvcc(nalus_in_annex_b.data(), nalus_in_annex_b.size(),
                               nalus_in_avcc.data()));
  return nalus_in_avcc;
}

void VerifyConvertTo(const AvcParameterSets& parameter_sets_in_annex_b) {
  auto parameter_sets_headless = parameter_sets_in_annex_b.ConvertTo(kHeadless);

  ASSERT_TRUE(parameter_sets_headless.is_valid());
  ASSERT_EQ(parameter_sets_headless.format(), kHeadless);
  ASSERT_EQ(parameter_sets_headless.has_sps_and_pps(),
            parameter_sets_in_annex_b.has_sps_and_pps());
  if (parameter_sets_in_annex_b.has_sps_and_pps()) {
    ASSERT_EQ(ToAnnexB(parameter_sets_headless.first_sps()),
              parameter_sets_in_annex_b.first_sps());
    ASSERT_EQ(ToAnnexB(parameter_sets_headless.first_pps()),
              parameter_sets_in_annex_b.first_pps());
  }
  ASSERT_EQ(parameter_sets_headless.GetAddresses().size(),
            parameter_sets_in_annex_b.GetAddresses().size());
  ASSERT_EQ(parameter_sets_headless.GetSizesInBytes().size(),
            parameter_sets_in_annex_b.GetSizesInBytes().size());
  for (size_t i = 0; i < parameter_sets_headless.GetAddresses().size(); ++i) {
    auto nalu_headless =
        std::vector<uint8_t>(parameter_sets_headless.GetAddresses()[i],
                             parameter_sets_headless.GetAddresses()[i] +
                                 parameter_sets_headless.GetSizesInBytes()[i]);
    auto nalu_in_annex_b = std::vector<uint8_t>(
        parameter_sets_in_annex_b.GetAddresses()[i],
        parameter_sets_in_annex_b.GetAddresses()[i] +
            parameter_sets_in_annex_b.GetSizesInBytes()[i]);
    ASSERT_EQ(ToAnnexB(nalu_headless), nalu_in_annex_b);
  }
  ASSERT_EQ(parameter_sets_headless.combined_size_in_bytes() +
                kAnnexBHeaderSizeInBytes *
                    parameter_sets_headless.GetAddresses().size(),
            parameter_sets_in_annex_b.combined_size_in_bytes());
}

void VerifyAnnexB(const std::vector<uint8_t>& nalus_in_annex_b,
                  const std::vector<uint8_t>& first_sps_in_annex_b,
                  const std::vector<uint8_t>& first_pps_in_annex_b,
                  const std::vector<uint8_t>& parameter_sets_in_annex_b) {
  AvcParameterSets parameter_sets(kAnnexB, nalus_in_annex_b.data(),
                                  nalus_in_annex_b.size());

  ASSERT_TRUE(parameter_sets.is_valid());
  ASSERT_EQ(parameter_sets.format(), kAnnexB);
  ASSERT_TRUE(parameter_sets.has_sps_and_pps());
  ASSERT_EQ(parameter_sets.first_sps(), first_sps_in_annex_b);
  ASSERT_EQ(parameter_sets.first_pps(), first_pps_in_annex_b);
  ASSERT_EQ(parameter_sets.combined_size_in_bytes(),
            parameter_sets_in_annex_b.size());

  ASSERT_TRUE(parameter_sets == parameter_sets);
  ASSERT_FALSE(parameter_sets != parameter_sets);

  const auto& addresses = parameter_sets.GetAddresses();
  const auto& sizes = parameter_sets.GetSizesInBytes();
  ASSERT_EQ(addresses.size(), sizes.size());

  std::vector<uint8_t> accumulated_parameter_sets;
  for (size_t i = 0; i < addresses.size(); ++i) {
    accumulated_parameter_sets =
        accumulated_parameter_sets +
        std::vector<uint8_t>(addresses[i], addresses[i] + sizes[i]);
  }
  ASSERT_EQ(accumulated_parameter_sets, parameter_sets_in_annex_b);

  VerifyConvertTo(parameter_sets);
}

void VerifyAllEmpty(const std::vector<uint8_t>& nalus_in_annex_b) {
  AvcParameterSets parameter_sets(kAnnexB, nalus_in_annex_b.data(),
                                  nalus_in_annex_b.size());

  ASSERT_EQ(parameter_sets.format(), kAnnexB);

  for (int i = 0; i < 2; ++i) {
    if (i == 1) {
      parameter_sets = parameter_sets.ConvertTo(kHeadless);
      ASSERT_EQ(parameter_sets.format(), kHeadless);
    }
    ASSERT_TRUE(parameter_sets.is_valid());
    ASSERT_FALSE(parameter_sets.has_sps_and_pps());
    ASSERT_TRUE(parameter_sets.GetAddresses().empty());
    ASSERT_TRUE(parameter_sets.GetSizesInBytes().empty());
    ASSERT_EQ(parameter_sets.combined_size_in_bytes(), 0);
  }

  VerifyConvertTo(parameter_sets);
}

bool HasEqualParameterSets(const std::vector<uint8_t>& nalus_in_annex_b_1,
                           const std::vector<uint8_t>& nalus_in_annex_b_2) {
  AvcParameterSets parameter_sets_1(kAnnexB, nalus_in_annex_b_1.data(),
                                    nalus_in_annex_b_1.size());
  AvcParameterSets parameter_sets_2(kAnnexB, nalus_in_annex_b_2.data(),
                                    nalus_in_annex_b_2.size());

  SB_CHECK((parameter_sets_1 == parameter_sets_2) !=
           (parameter_sets_1 != parameter_sets_2));

  return parameter_sets_1 == parameter_sets_2;
}

TEST(AvcParameterSetsTest, Ctor) {
  AvcParameterSets parameter_sets_1(kAnnexB, nullptr, 0);
  AvcParameterSets parameter_sets_2(kAnnexB, kSpsInAnnexB.data(),
                                    kSpsInAnnexB.size());
  AvcParameterSets parameter_sets_3(kAnnexB, kPpsInAnnexB.data(),
                                    kPpsInAnnexB.size());
  AvcParameterSets parameter_sets_4(kAnnexB, kIdrInAnnexB.data(),
                                    kIdrInAnnexB.size());

  auto nalus_in_annex_b = kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;
  AvcParameterSets parameter_sets_5(kAnnexB, nalus_in_annex_b.data(),
                                    nalus_in_annex_b.size());
}

TEST(AvcParameterSetsTest, SingleSpsAndPps) {
  auto sps_before_pps = kSpsInAnnexB + kPpsInAnnexB + kIdrInAnnexB;
  auto pps_before_sps = kPpsInAnnexB + kSpsInAnnexB + kIdrInAnnexB;

  VerifyAnnexB(sps_before_pps, kSpsInAnnexB, kPpsInAnnexB,
               kSpsInAnnexB + kPpsInAnnexB);
  VerifyAnnexB(pps_before_sps, kSpsInAnnexB, kPpsInAnnexB,
               kPpsInAnnexB + kSpsInAnnexB);

  // Change sps and pps position are treated as unequal.
  ASSERT_FALSE(HasEqualParameterSets(sps_before_pps, pps_before_sps));
}

TEST(AvcParameterSetsTest, MultipleSpsAndPps) {
  for (int i = 0; i < 4; ++i) {
    std::vector<uint8_t> parameter_sets_in_annex_b;
    switch (i) {
      case 0:
        parameter_sets_in_annex_b = kSpsInAnnexB + Mutate(kSpsInAnnexB) +
                                    kPpsInAnnexB + Mutate(kPpsInAnnexB);
        break;
      case 1:
        parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB +
                                    Mutate(kSpsInAnnexB) + Mutate(kPpsInAnnexB);
        break;
      case 2:
        parameter_sets_in_annex_b = kPpsInAnnexB + kSpsInAnnexB +
                                    Mutate(kSpsInAnnexB) + Mutate(kPpsInAnnexB);
        break;
      case 3:
        parameter_sets_in_annex_b = kPpsInAnnexB + Mutate(kPpsInAnnexB) +
                                    kSpsInAnnexB + Mutate(kSpsInAnnexB);
        break;
    }
    auto nalus_in_annex_b =
        parameter_sets_in_annex_b + kIdrInAnnexB + kSliceInAnnexB;

    VerifyAnnexB(nalus_in_annex_b, kSpsInAnnexB, kPpsInAnnexB,
                 parameter_sets_in_annex_b);

    ASSERT_FALSE(HasEqualParameterSets(kSpsInAnnexB + kPpsInAnnexB,
                                       parameter_sets_in_annex_b));
  }
}

TEST(AvcParameterSetsTest, SpsWithoutPps) {
  auto leading_sps_nalus = kSpsInAnnexB;
  for (int i = 0; i < 5; ++i) {
    auto nalus_in_annex_b = leading_sps_nalus + kIdrInAnnexB;

    AvcParameterSets parameter_sets(kAnnexB, nalus_in_annex_b.data(),
                                    nalus_in_annex_b.size());

    ASSERT_TRUE(parameter_sets.is_valid());
    ASSERT_FALSE(parameter_sets.has_sps_and_pps());
    ASSERT_EQ(kSpsInAnnexB, parameter_sets.first_sps());
    ASSERT_EQ(parameter_sets.combined_size_in_bytes(),
              leading_sps_nalus.size());

    ASSERT_EQ(parameter_sets.GetAddresses().size(), i + 1);
    ASSERT_EQ(parameter_sets.GetSizesInBytes().size(), i + 1);
    ASSERT_EQ(kSpsInAnnexB,
              std::vector<uint8_t>(parameter_sets.GetAddresses()[0],
                                   parameter_sets.GetAddresses()[0] +
                                       parameter_sets.GetSizesInBytes()[0]));

    leading_sps_nalus = leading_sps_nalus + Mutate(kSpsInAnnexB);
  }
}

TEST(AvcParameterSetsTest, PpsWithoutSps) {
  auto leading_pps_nalus = kPpsInAnnexB;
  for (int i = 0; i < 5; ++i) {
    auto nalus_in_annex_b = leading_pps_nalus + kIdrInAnnexB;

    AvcParameterSets parameter_sets(kAnnexB, nalus_in_annex_b.data(),
                                    nalus_in_annex_b.size());

    ASSERT_TRUE(parameter_sets.is_valid());
    ASSERT_FALSE(parameter_sets.has_sps_and_pps());
    ASSERT_EQ(kPpsInAnnexB, parameter_sets.first_pps());
    ASSERT_EQ(parameter_sets.combined_size_in_bytes(),
              leading_pps_nalus.size());

    ASSERT_EQ(parameter_sets.GetAddresses().size(), i + 1);
    ASSERT_EQ(parameter_sets.GetSizesInBytes().size(), i + 1);
    ASSERT_EQ(kPpsInAnnexB,
              std::vector<uint8_t>(parameter_sets.GetAddresses()[0],
                                   parameter_sets.GetAddresses()[0] +
                                       parameter_sets.GetSizesInBytes()[0]));

    leading_pps_nalus = leading_pps_nalus + Mutate(kPpsInAnnexB);
  }
}

TEST(AvcParameterSetsTest, MultipleSpsAndPpsWithoutPayload) {
  for (int i = 0; i < 4; ++i) {
    std::vector<uint8_t> parameter_sets_in_annex_b;
    switch (i) {
      case 0:
        parameter_sets_in_annex_b = kSpsInAnnexB + Mutate(kSpsInAnnexB) +
                                    kPpsInAnnexB + Mutate(kPpsInAnnexB);
        break;
      case 1:
        parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB +
                                    Mutate(kSpsInAnnexB) + Mutate(kPpsInAnnexB);
        break;
      case 2:
        parameter_sets_in_annex_b = kPpsInAnnexB + kSpsInAnnexB +
                                    Mutate(kSpsInAnnexB) + Mutate(kPpsInAnnexB);
        break;
      case 3:
        parameter_sets_in_annex_b = kPpsInAnnexB + Mutate(kPpsInAnnexB) +
                                    kSpsInAnnexB + Mutate(kSpsInAnnexB);
        break;
    }

    AvcParameterSets parameter_sets(kAnnexB, parameter_sets_in_annex_b.data(),
                                    parameter_sets_in_annex_b.size());

    VerifyAnnexB(parameter_sets_in_annex_b, kSpsInAnnexB, kPpsInAnnexB,
                 parameter_sets_in_annex_b);
  }
}

TEST(AvcParameterSetsTest, SpsAfterIdr) {
  auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
  auto nalus_in_annex_b =
      parameter_sets_in_annex_b + kIdrInAnnexB + kSpsInAnnexB;

  VerifyAnnexB(nalus_in_annex_b, kSpsInAnnexB, kPpsInAnnexB,
               parameter_sets_in_annex_b);
  ASSERT_TRUE(
      HasEqualParameterSets(parameter_sets_in_annex_b, nalus_in_annex_b));
}

TEST(AvcParameterSetsTest, PpsAfterIdr) {
  auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
  auto nalus_in_annex_b =
      parameter_sets_in_annex_b + kIdrInAnnexB + kPpsInAnnexB;

  VerifyAnnexB(nalus_in_annex_b, kSpsInAnnexB, kPpsInAnnexB,
               parameter_sets_in_annex_b);
  ASSERT_TRUE(
      HasEqualParameterSets(parameter_sets_in_annex_b, nalus_in_annex_b));
}

TEST(AvcParameterSetsTest, SpsAndPpsAfterIdrWithoutSpsAndPps) {
  auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
  auto nalus_in_annex_b =
      kIdrInAnnexB + kPpsInAnnexB + parameter_sets_in_annex_b;

  VerifyAllEmpty(nalus_in_annex_b);
}

TEST(AvcParameterSetsTest, Nullptr) {
  AvcParameterSets parameter_sets(kAnnexB, nullptr, 0);

  ASSERT_TRUE(parameter_sets.is_valid());
  ASSERT_EQ(parameter_sets.format(), kAnnexB);
  ASSERT_FALSE(parameter_sets.has_sps_and_pps());
  ASSERT_TRUE(parameter_sets.GetAddresses().empty());
  ASSERT_TRUE(parameter_sets.GetSizesInBytes().empty());
  ASSERT_EQ(parameter_sets.combined_size_in_bytes(), 0);
}

TEST(AvcParameterSetsTest, NaluHeaderWithoutType) {
  {
    AvcParameterSets parameter_sets(kAnnexB, kNaluHeaderOnlyInAnnexB.data(),
                                    kNaluHeaderOnlyInAnnexB.size());

    ASSERT_TRUE(parameter_sets.is_valid());
    ASSERT_EQ(parameter_sets.format(), kAnnexB);
    ASSERT_FALSE(parameter_sets.has_sps_and_pps());
    ASSERT_TRUE(parameter_sets.GetAddresses().empty());
    ASSERT_TRUE(parameter_sets.GetSizesInBytes().empty());
    ASSERT_EQ(parameter_sets.combined_size_in_bytes(), 0);
  }
  for (int i = 0; i < 2; ++i) {
    auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
    std::vector<uint8_t> nalus_in_annex_b;
    if (i == 0) {
      nalus_in_annex_b = parameter_sets_in_annex_b + kNaluHeaderOnlyInAnnexB;
    } else {
      nalus_in_annex_b =
          parameter_sets_in_annex_b + kIdrInAnnexB + kNaluHeaderOnlyInAnnexB;
    }

    VerifyAnnexB(nalus_in_annex_b, kSpsInAnnexB, kPpsInAnnexB,
                 parameter_sets_in_annex_b);
  }
}

TEST(AvcParameterSetsTest, InvalidNaluHeader) {
  { VerifyAllEmpty(kNaluHeaderOnlyInAnnexB); }
  {
    auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
    auto nalus_in_annex_b = parameter_sets_in_annex_b + kNaluHeaderOnlyInAnnexB;

    VerifyAnnexB(nalus_in_annex_b, kSpsInAnnexB, kPpsInAnnexB,
                 parameter_sets_in_annex_b);
  }
  {
    auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
    auto nalus_in_annex_b =
        parameter_sets_in_annex_b + kIdrInAnnexB + kNaluHeaderOnlyInAnnexB;

    VerifyAnnexB(nalus_in_annex_b, kSpsInAnnexB, kPpsInAnnexB,
                 parameter_sets_in_annex_b);
  }
  {
    auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
    auto nalus_in_annex_b =
        parameter_sets_in_annex_b + kIdrInAnnexB + kNaluHeaderOnlyInAnnexB;
    nalus_in_annex_b.erase(nalus_in_annex_b.begin());  // One less 0

    AvcParameterSets parameter_sets(kAnnexB, nalus_in_annex_b.data(),
                                    nalus_in_annex_b.size());
    ASSERT_FALSE(parameter_sets.is_valid());
  }
  {
    auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
    auto nalus_in_annex_b =
        parameter_sets_in_annex_b + kIdrInAnnexB + kNaluHeaderOnlyInAnnexB;
    nalus_in_annex_b.insert(nalus_in_annex_b.begin(), 0);  // One extra 0

    AvcParameterSets parameter_sets(kAnnexB, nalus_in_annex_b.data(),
                                    nalus_in_annex_b.size());
    ASSERT_FALSE(parameter_sets.is_valid());
  }
}

TEST(AvcParameterSetsTest, MultiNalusWithoutSpsPps) {
  std::vector<uint8_t> nalus_in_annex_b = kIdrInAnnexB + kSliceInAnnexB;

  for (int i = 0; i < 3; ++i) {
    VerifyAllEmpty(nalus_in_annex_b);

    nalus_in_annex_b = nalus_in_annex_b + nalus_in_annex_b;

    ASSERT_TRUE(
        HasEqualParameterSets(nalus_in_annex_b, std::vector<uint8_t>()));
  }
}

TEST(AvcParameterSetsTest, ConvertAnnexBToAvcc) {
  {
    std::vector<uint8_t> raw_nalus[] = {kRawSlice, kRawIdr, kRawSps, kRawPps};
    std::vector<uint8_t> nalus_in_annex_b;
    std::vector<uint8_t> nalus_in_avcc;

    for (int i = 0; i < 20; ++i) {
      nalus_in_annex_b =
          nalus_in_annex_b + ToAnnexB(raw_nalus[i % SB_ARRAY_SIZE(raw_nalus)]);
      nalus_in_avcc =
          nalus_in_avcc + ToAvcc(raw_nalus[i % SB_ARRAY_SIZE(raw_nalus)]);

      ASSERT_EQ(ConvertAnnexBToAvcc(nalus_in_annex_b), nalus_in_avcc);
    }
  }
  {
    std::vector<uint8_t> raw_nalus[] = {kRawSps, kRawPps, kRawSlice, kRawIdr};
    std::vector<uint8_t> nalus_in_annex_b;
    std::vector<uint8_t> nalus_in_avcc;

    for (int i = 0; i < 20; ++i) {
      nalus_in_annex_b =
          nalus_in_annex_b + ToAnnexB(raw_nalus[i % SB_ARRAY_SIZE(raw_nalus)]);
      nalus_in_avcc =
          nalus_in_avcc + ToAvcc(raw_nalus[i % SB_ARRAY_SIZE(raw_nalus)]);

      ASSERT_EQ(ConvertAnnexBToAvcc(nalus_in_annex_b), nalus_in_avcc);
    }
  }
}

TEST(AvcParameterSetsTest, ConvertAnnexBToAvccEmptyNalus) {
  const std::vector<uint8_t> kEmpty = {};
  const std::vector<uint8_t> kRawNalu = {1, 2, 3, 4, 5};

  std::vector<uint8_t> nalus_in_annex_b;
  std::vector<uint8_t> nalus_in_avcc;

  for (int i = 0; i < 3; ++i) {
    nalus_in_annex_b = nalus_in_annex_b + ToAnnexB(kEmpty);
    nalus_in_avcc = nalus_in_avcc + ToAvcc(kEmpty);

    ASSERT_EQ(ConvertAnnexBToAvcc(nalus_in_annex_b), nalus_in_avcc);
  }

  ASSERT_EQ(ConvertAnnexBToAvcc(ToAnnexB(kEmpty) + ToAnnexB(kRawNalu)),
            ToAvcc(kEmpty) + ToAvcc(kRawNalu));
  ASSERT_EQ(ConvertAnnexBToAvcc(ToAnnexB(kRawNalu) + ToAnnexB(kEmpty)),
            ToAvcc(kRawNalu) + ToAvcc(kEmpty));
}

TEST(AvcParameterSetsTest, ConvertAnnexBToAvccInvalidNalus) {
  {
    auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
    auto nalus_in_annex_b =
        parameter_sets_in_annex_b + kIdrInAnnexB + kNaluHeaderOnlyInAnnexB;
    nalus_in_annex_b.erase(nalus_in_annex_b.begin());  // One less 0
    auto nalus_in_avcc = nalus_in_annex_b;
    ASSERT_FALSE(ConvertAnnexBToAvcc(nalus_in_annex_b.data(),
                                     nalus_in_annex_b.size(),
                                     nalus_in_avcc.data()));
  }
  {
    auto parameter_sets_in_annex_b = kSpsInAnnexB + kPpsInAnnexB;
    auto nalus_in_annex_b =
        parameter_sets_in_annex_b + kIdrInAnnexB + kNaluHeaderOnlyInAnnexB;
    nalus_in_annex_b.insert(nalus_in_annex_b.begin(), 0);  // One extra 0
    auto nalus_in_avcc = nalus_in_annex_b;
    ASSERT_FALSE(ConvertAnnexBToAvcc(nalus_in_annex_b.data(),
                                     nalus_in_annex_b.size(),
                                     nalus_in_avcc.data()));
  }
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
