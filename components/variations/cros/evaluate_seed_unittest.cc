// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros/evaluate_seed.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "build/branding_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/cros/featured.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations::evaluate_seed {

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

TEST(VariationsCrosEvaluateSeed, GetClientFilterable_Enrolled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"evaluate_seed", "--enterprise-enrolled"});
  std::unique_ptr<ClientFilterableState> state = GetClientFilterableState();
  EXPECT_TRUE(state->IsEnterprise());
}

TEST(VariationsCrosEvaluateSeed, GetClientFilterable_NotEnrolled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  std::unique_ptr<ClientFilterableState> state = GetClientFilterableState();
  EXPECT_FALSE(state->IsEnterprise());
}

struct Param {
  std::string test_name;
  std::string channel_name;
  Study::Channel channel;
};

class VariationsCrosEvaluateSeedGetChannel
    : public ::testing::TestWithParam<Param> {
 protected:
  VariationsCrosEvaluateSeedGetChannel() = default;
};

TEST_P(VariationsCrosEvaluateSeedGetChannel,
       GetClientFilterableState_Channel_Override) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"evaluate_seed",
       base::StrCat({"--fake-variations-channel=", GetParam().channel_name})});
  std::unique_ptr<ClientFilterableState> state = GetClientFilterableState();
  EXPECT_EQ(GetParam().channel, state->channel);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Verify GetClientFilterableState gets the channel from lsb-release on branded
// builds.
TEST_P(VariationsCrosEvaluateSeedGetChannel,
       GetClientFilterableState_Channel_Branded) {
  std::string lsb_release = base::StrCat(
      {"CHROMEOS_RELEASE_TRACK=", GetParam().channel_name, "-channel"});
  const base::Time lsb_release_time(base::Time::FromDoubleT(12345.6));
  base::test::ScopedChromeOSVersionInfo lsb_info(lsb_release, lsb_release_time);

  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  std::unique_ptr<ClientFilterableState> state = GetClientFilterableState();
  EXPECT_EQ(GetParam().channel, state->channel);
}

#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Verify that we use unknown channel on non-branded builds.
TEST(VariationsCrosEvaluateSeed, GetClientFilterableState_Channel_NotBranded) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  std::unique_ptr<ClientFilterableState> state = GetClientFilterableState();
  EXPECT_EQ(Study::UNKNOWN, state->channel);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
INSTANTIATE_TEST_SUITE_P(
    VariationsCrosEvaluateSeedGetChannel,
    VariationsCrosEvaluateSeedGetChannel,
    ::testing::ValuesIn<Param>({{"Stable", "stable", Study::STABLE},
                                {"Beta", "beta", Study::BETA},
                                {"Dev", "dev", Study::DEV},
                                {"Canary", "canary", Study::CANARY},
                                {"Unknown", "testimage", Study::UNKNOWN}}),
    [](const testing::TestParamInfo<
        VariationsCrosEvaluateSeedGetChannel::ParamType>& info) {
      return info.param.test_name;
    });

#if BUILDFLAG(PLATFORM_CFM)
TEST(VariationsCrosEvaluateSeed, GetClientFilterableState_FormFactor) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  std::unique_ptr<ClientFilterableState> state = GetClientFilterableState();
  EXPECT_EQ(Study::MEET_DEVICE, state->form_factor);
}
#else   // BUILDFLAG(PLATFORM_CFM)
TEST(VariationsCrosEvaluateSeed, GetClientFilterableState_FormFactor) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  std::unique_ptr<ClientFilterableState> state = GetClientFilterableState();
  EXPECT_EQ(Study::DESKTOP, state->form_factor);
}
#endif  // BUILDFLAG(PLATFORM_CFM)

// Should ignore data if flag is off.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_Off) {
  featured::SeedDetails safe_seed;
  safe_seed.set_compressed_data("some text");
  std::string text;
  safe_seed.SerializeToString(&text);
  FILE* stream = fmemopen(text.data(), text.size(), "r");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  auto data = GetSafeSeedData(stream);
  featured::SeedDetails empty_seed;
  ASSERT_TRUE(data.has_value());
  EXPECT_FALSE(data.value().use_safe_seed);
  EXPECT_THAT(data.value().seed_data, EqualsProto(empty_seed));
}

// Should return specified data via stream if flag is on.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On) {
  featured::SeedDetails safe_seed;
  safe_seed.set_compressed_data("some text");
  std::string text;
  safe_seed.SerializeToString(&text);
  FILE* stream = fmemopen(text.data(), text.size(), "r");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"evaluate_seed", "--use-safe-seed"});
  auto data = GetSafeSeedData(stream);
  ASSERT_TRUE(data.has_value());
  EXPECT_TRUE(data.value().use_safe_seed);
  EXPECT_THAT(data.value().seed_data, EqualsProto(safe_seed));
}

// Should not attempt to read stream if flag is not on.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_Off_FailRead) {
  featured::SeedDetails safe_seed;
  safe_seed.set_compressed_data("some text");
  std::string text;
  safe_seed.SerializeToString(&text);
  FILE* stream = fmemopen(text.data(), text.size(), "w");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  auto data = GetSafeSeedData(stream);
  featured::SeedDetails empty_seed;
  ASSERT_TRUE(data.has_value());
  EXPECT_FALSE(data.value().use_safe_seed);
  EXPECT_THAT(data.value().seed_data, EqualsProto(empty_seed));
}

// If flag is on and reading fails, should return nullopt.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On_FailRead) {
  featured::SeedDetails safe_seed;
  safe_seed.set_compressed_data("some text");
  std::string text;
  safe_seed.SerializeToString(&text);
  FILE* stream = fmemopen(text.data(), text.size(), "w");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"evaluate_seed", "--use-safe-seed"});
  auto data = GetSafeSeedData(stream);
  EXPECT_FALSE(data.has_value());
}

// If flag is on and parsing input fails, should return nullopt.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On_FailParse) {
  std::string text("not a serialized proto");
  FILE* stream = fmemopen(text.data(), text.size(), "r");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"evaluate_seed", "--use-safe-seed"});
  auto data = GetSafeSeedData(stream);
  ASSERT_FALSE(data.has_value());
}

// If flag is on and reading fails, should return nullopt.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On_FailRead_Null) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"evaluate_seed", "--use-safe-seed"});
  auto data = GetSafeSeedData(nullptr);
  EXPECT_FALSE(data.has_value());
}

TEST(VariationsCrosEvaluateSeed, Main_NoFlag) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  EXPECT_EQ(0, EvaluateSeedMain(nullptr));
}

TEST(VariationsCrosEvaluateSeed, Main_NoStdin) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"evaluate_seed", "--use-safe-seed"});
  EXPECT_EQ(1, EvaluateSeedMain(nullptr));
}

}  // namespace variations::evaluate_seed
