// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_store.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/featured/fake_featured_client.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace variations {
namespace {

using ::base::test::EqualsProto;

// The sentinel value that may be stored as the latest variations seed value in
// prefs to indicate that the latest seed is identical to the safe seed.
// Note: This constant is intentionally duplicated in the test because it is
// persisted to disk. In order to maintain backward-compatibility, it's
// important that code continue to correctly handle this specific constant, even
// if the constant used internally in the implementation changes.
constexpr char kIdenticalToSafeSeedSentinel[] = "safe_seed_content";

// File used by SeedReaderWriter to store a latest seed.
const base::FilePath::CharType kSeedFilename[] = FILE_PATH_LITERAL("TestSeed");

class TestVariationsSeedStore : public VariationsSeedStore {
 public:
  explicit TestVariationsSeedStore(
      PrefService* local_state,
      version_info::Channel channel = version_info::Channel::UNKNOWN,
      base::FilePath seed_file_dir = base::FilePath(),
      bool signature_verification_needed = false,
      std::unique_ptr<SeedResponse> initial_seed = nullptr,
      bool use_first_run_prefs = true)
      : VariationsSeedStore(
            local_state,
            std::move(initial_seed),
            signature_verification_needed,
            std::make_unique<VariationsSafeSeedStoreLocalState>(local_state,
                                                                channel,
                                                                seed_file_dir),
            channel,
            seed_file_dir,
            use_first_run_prefs) {}
  ~TestVariationsSeedStore() override = default;
};

// Creates a base::Time object from the corresponding raw value. The specific
// implementation is not important; it's only important that distinct inputs map
// to distinct outputs.
base::Time WrapTime(int64_t time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(time));
}

// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
// probability weight 100. |seed|'s study field will be cleared before adding
// the new study.
VariationsSeed CreateTestSeed() {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("test");
  study->set_default_experiment_name("abc");
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name("abc");
  experiment->set_probability_weight(100);
  seed.set_serial_number("123");
  return seed;
}

// Returns a ClientFilterableState with all fields set to "interesting" values
// for testing.
std::unique_ptr<ClientFilterableState> CreateTestClientFilterableState() {
  std::unique_ptr<ClientFilterableState> client_state =
      std::make_unique<ClientFilterableState>(
          base::BindOnce([] { return false; }),
          base::BindOnce([] { return base::flat_set<uint64_t>(); }));
  client_state->locale = "es-MX";
  client_state->reference_date = WrapTime(1234554321);
  client_state->version = base::Version("1.2.3.4");
  client_state->channel = Study::CANARY;
  client_state->form_factor = Study::PHONE;
  client_state->platform = Study::PLATFORM_MAC;
  client_state->hardware_class = "mario";
  client_state->is_low_end_device = true;
  client_state->session_consistency_country = "mx";
  client_state->permanent_consistency_country = "br";
  return client_state;
}

// Serializes |seed| to protobuf binary format.
std::string SerializeSeed(const VariationsSeed& seed) {
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}

// Compresses |data| using Gzip compression and returns the result.
std::string Gzip(const std::string& data) {
  std::string compressed;
  EXPECT_TRUE(compression::GzipCompress(data, &compressed));
  return compressed;
}

// Gzips |data| and then base64-encodes it.
std::string GzipAndBase64Encode(const std::string& data) {
  return base::Base64Encode(Gzip(data));
}

// Serializes |seed| to gzipped base64-encoded protobuf binary format.
std::string SerializeSeedBase64(const VariationsSeed& seed) {
  return GzipAndBase64Encode(SerializeSeed(seed));
}

// Wrapper over base::Base64Decode() that returns the result.
std::string Base64DecodeData(const std::string& data) {
  std::string decoded;
  EXPECT_TRUE(base::Base64Decode(data, &decoded));
  return decoded;
}

// Sample seeds and the server produced delta between them to verify that the
// client code is able to decode the deltas produced by the server.
struct {
  const std::string base64_initial_seed_data =
      "CigxN2E4ZGJiOTI4ODI0ZGU3ZDU2MGUyODRlODY1ZDllYzg2NzU1MTE0ElgKDFVNQVN0YWJp"
      "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
      "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEkQKIFVNQS1Vbmlmb3JtaXR5LVRyaWFsLTEwMC1Q"
      "ZXJjZW50GIDjhcAFOAFCCGdyb3VwXzAxSgwKCGdyb3VwXzAxEAFgARJPCh9VTUEtVW5pZm9y"
      "bWl0eS1UcmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDAoIZ3JvdXBfMDEQAUoL"
      "CgdkZWZhdWx0EAFgAQ==";
  const std::string base64_new_seed_data =
      "CigyNGQzYTM3ZTAxYmViOWYwNWYzMjM4YjUzNWY3MDg1ZmZlZWI4NzQwElgKDFVNQVN0YWJp"
      "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
      "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEpIBCh9VTUEtVW5pZm9ybWl0eS1UcmlhbC0yMC1Q"
      "ZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKEQoIZ3JvdXBfMDEQARijtskBShEKCGdyb3VwXzAy"
      "EAEYpLbJAUoRCghncm91cF8wMxABGKW2yQFKEQoIZ3JvdXBfMDQQARimtskBShAKB2RlZmF1"
      "bHQQARiitskBYAESWAofVU1BLVVuaWZvcm1pdHktVHJpYWwtNTAtUGVyY2VudBiA44XABTgB"
      "QgdkZWZhdWx0Sg8KC25vbl9kZWZhdWx0EAFKCwoHZGVmYXVsdBABUgQoACgBYAE=";
  const std::string base64_delta_data =
      "KgooMjRkM2EzN2UwMWJlYjlmMDVmMzIzOGI1MzVmNzA4NWZmZWViODc0MAAqW+4BkgEKH1VN"
      "QS1Vbmlmb3JtaXR5LVRyaWFsLTIwLVBlcmNlbnQYgOOFwAU4AUIHZGVmYXVsdEoRCghncm91"
      "cF8wMRABGKO2yQFKEQoIZ3JvdXBfMDIQARiktskBShEKCGdyb3VwXzAzEAEYpbbJAUoRCghn"
      "cm91cF8wNBABGKa2yQFKEAoHZGVmYXVsdBABGKK2yQFgARJYCh9VTUEtVW5pZm9ybWl0eS1U"
      "cmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDwoLbm9uX2RlZmF1bHQQAUoLCgdk"
      "ZWZhdWx0EAFSBCgAKAFgAQ==";

  std::string GetInitialSeedData() {
    return Base64DecodeData(base64_initial_seed_data);
  }

  std::string GetInitialSeedDataAsPrefValue() {
    return GzipAndBase64Encode(GetInitialSeedData());
  }

  std::string GetNewSeedData() {
    return Base64DecodeData(base64_new_seed_data);
  }

  std::string GetDeltaData() { return Base64DecodeData(base64_delta_data); }

} kSeedDeltaTestData;

// Sets all seed-related prefs to non-default values. Used to verify whether
// pref values were cleared.
void SetAllSeedPrefsToNonDefaultValues(PrefService* prefs) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::Days(1);

  // Regular seed prefs:
  prefs->SetString(prefs::kVariationsCompressedSeed, "coffee");
  prefs->SetTime(prefs::kVariationsLastFetchTime, now);
  prefs->SetTime(prefs::kVariationsSeedDate, now - delta * 1);
  prefs->SetString(prefs::kVariationsSeedSignature, "tea");

  // Safe seed prefs:
  prefs->SetString(prefs::kVariationsSafeCompressedSeed, "ketchup");
  prefs->SetTime(prefs::kVariationsSafeSeedDate, now - delta * 2);
  prefs->SetTime(prefs::kVariationsSafeSeedFetchTime, now - delta * 3);
  prefs->SetString(prefs::kVariationsSafeSeedLocale, "en-MX");
  prefs->SetInteger(prefs::kVariationsSafeSeedMilestone, 90);
  prefs->SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry, "mx");
  prefs->SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry, "gt");
  prefs->SetString(prefs::kVariationsSafeSeedSignature, "mustard");
}

// Checks whether the given pref has its default value in |prefs|.
bool PrefHasDefaultValue(const TestingPrefServiceSimple& prefs,
                         const char* pref_name) {
  return prefs.FindPreference(pref_name)->IsDefaultValue();
}

void CheckRegularSeedPrefsAreSet(const TestingPrefServiceSimple& prefs) {
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsLastFetchTime));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));
}

void CheckRegularSeedPrefsAreCleared(const TestingPrefServiceSimple& prefs) {
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsLastFetchTime));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedDate));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSeedSignature));
}

void CheckSafeSeedPrefsAreSet(const TestingPrefServiceSimple& prefs) {
  EXPECT_FALSE(
      PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedMilestone));
  EXPECT_FALSE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_FALSE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));
}

void CheckSafeSeedPrefsAreCleared(const TestingPrefServiceSimple& prefs) {
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedDate));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedFetchTime));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedLocale));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedMilestone));
  EXPECT_TRUE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedPermanentConsistencyCountry));
  EXPECT_TRUE(PrefHasDefaultValue(
      prefs, prefs::kVariationsSafeSeedSessionConsistencyCountry));
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeSeedSignature));
}

}  // namespace

class VariationsSeedStoreTest : public ::testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(VariationsSeedStoreTest, LoadSeed_ValidSeed) {
  // Store good seed data to test if loading from prefs works.
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a test signature, ignored.";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  // Check that loading a seed works correctly.
  ASSERT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &loaded_base64_seed_signature));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(SerializeSeed(seed), loaded_seed_data);
  EXPECT_EQ(base64_seed_signature, loaded_base64_seed_signature);
  // Make sure the pref hasn't been changed.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_EQ(base64_seed, prefs.GetString(prefs::kVariationsCompressedSeed));
}

TEST_F(VariationsSeedStoreTest, LoadSeed_InvalidSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed, "this should fail");

  // Loading an invalid seed should return false.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kCorruptBase64, 1);
  CheckRegularSeedPrefsAreCleared(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST_F(VariationsSeedStoreTest, LoadSeed_InvalidSignature) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  SerializeSeedBase64(CreateTestSeed()));
  prefs.SetString(prefs::kVariationsSeedSignature,
                  "a deeply compromised signature.");

  // Loading a valid seed with an invalid signature should return false and
  // clear all associated prefs when signature verification is enabled.
  TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                     /*seed_file_dir=*/base::FilePath(),
                                     /*signature_verification_needed=*/true);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckRegularSeedPrefsAreCleared(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST_F(VariationsSeedStoreTest, LoadSeed_InvalidProto) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  GzipAndBase64Encode("Not a proto"));

  // Loading a valid seed with an invalid signature should return false and
  // clear all associated prefs when signature verification is enabled.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kCorruptProtobuf, 1);
  CheckRegularSeedPrefsAreCleared(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST_F(VariationsSeedStoreTest, LoadSeed_RejectEmptySignature) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  SerializeSeedBase64(CreateTestSeed()));
  prefs.SetString(prefs::kVariationsSeedSignature, "");

  // Loading a valid seed with an empty signature should fail and clear all
  // associated prefs when signature verification is enabled.
  TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                     /*seed_file_dir=*/base::FilePath(),
                                     /*signature_verification_needed=*/true);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckRegularSeedPrefsAreCleared(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST_F(VariationsSeedStoreTest, LoadSeed_AcceptEmptySignature) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  SerializeSeedBase64(CreateTestSeed()));
  prefs.SetString(prefs::kVariationsSeedSignature, "");

  // Loading a valid seed with an empty signature should succeed iff
  // switches::kAcceptEmptySeedSignatureForTesting is on the command line.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAcceptEmptySeedSignatureForTesting);

  TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                     /*seed_file_dir=*/base::FilePath(),
                                     /*signature_verification_needed=*/true);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &loaded_base64_seed_signature));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);
  CheckRegularSeedPrefsAreSet(prefs);
  CheckSafeSeedPrefsAreSet(prefs);
}

TEST_F(VariationsSeedStoreTest, LoadSeed_EmptySeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  ASSERT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));

  // Loading an empty seed should return false.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  ASSERT_FALSE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                   &loaded_base64_seed_signature));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kEmpty, 1);
}

TEST_F(VariationsSeedStoreTest, LoadSeed_IdenticalToSafeSeed) {
  // Store good seed data to the safe seed prefs, and store a sentinel value to
  // the latest seed pref, to verify that loading via the alias works.
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const std::string base64_seed_signature = "a test signature, ignored.";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  kIdenticalToSafeSeedSentinel);
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);

  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string loaded_base64_seed_signature;
  // Check that loading the seed works correctly.
  ASSERT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &loaded_base64_seed_signature));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SeedLoadResult",
                                      LoadSeedResult::kSuccess, 1);

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(SerializeSeed(seed), loaded_seed_data);
  EXPECT_EQ(base64_seed_signature, loaded_base64_seed_signature);
}

TEST_F(VariationsSeedStoreTest, ApplyDeltaPatch) {
  std::string output;
  ASSERT_TRUE(VariationsSeedStore::ApplyDeltaPatch(
      kSeedDeltaTestData.GetInitialSeedData(),
      kSeedDeltaTestData.GetDeltaData(), &output));
  EXPECT_EQ(kSeedDeltaTestData.GetNewSeedData(), output);
}

struct StoreSeedDataTestParams {
  using TupleT = std::tuple<bool, version_info::Channel>;
  const bool require_synchronous_stores;
  const version_info::Channel channel;

  explicit StoreSeedDataTestParams(const TupleT& t)
      : require_synchronous_stores(std::get<0>(t)), channel(std::get<1>(t)) {}
};

class StoreSeedTestBase : public ::testing::Test {
 public:
  StoreSeedTestBase(std::string_view seed_pref, version_info::Channel channel)
      : file_writer_thread_("SeedReaderWriter Test thread") {
    file_writer_thread_.Start();
    CHECK(temp_dir_.CreateUniqueTempDir());
    temp_seed_file_path_ = temp_dir_.GetPath().Append(kSeedFilename);

    VariationsSeedStore::RegisterPrefs(prefs_.registry());

    // Initialize |seed_reader_writer_| with test thread and timer.
    seed_reader_writer_ = std::make_unique<SeedReaderWriter>(
        &prefs_, temp_dir_.GetPath(), kSeedFilename, channel, seed_pref,
        file_writer_thread_.task_runner());
    seed_reader_writer_->SetTimerForTesting(&timer_);
  }

  ~StoreSeedTestBase() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  base::Thread file_writer_thread_;
  base::ScopedTempDir temp_dir_;
  base::MockOneShotTimer timer_;
  base::FilePath temp_seed_file_path_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<SeedReaderWriter> seed_reader_writer_;
};

class StoreSeedDataChannelTest
    : public StoreSeedTestBase,
      public ::testing::WithParamInterface<StoreSeedDataTestParams> {
 public:
  StoreSeedDataChannelTest()
      : StoreSeedTestBase(prefs::kVariationsCompressedSeed,
                          GetParam().channel) {}
  ~StoreSeedDataChannelTest() override = default;

  bool RequireSynchronousStores() const {
    return GetParam().require_synchronous_stores;
  }

  struct Params {
    std::string country_code;
    bool is_delta_compressed;
    bool is_gzip_compressed;
  };

  // Wrapper for VariationsSeedStore::StoreSeedData() exposing a more convenient
  // API. Invokes either the underlying function either in sync or async mode,
  // but if async, it blocks on its completion.
  bool StoreSeedData(VariationsSeedStore& seed_store,
                     const std::string& seed_data,
                     const Params& params = {}) {
    base::RunLoop run_loop;
    seed_store.StoreSeedData(
        seed_data, /*base64_seed_signature=*/std::string(), params.country_code,
        base::Time::Now(), params.is_delta_compressed,
        params.is_gzip_compressed,
        base::BindOnce(&StoreSeedDataChannelTest::OnSeedStoreResult,
                       base::Unretained(this), run_loop.QuitClosure()),
        RequireSynchronousStores());
    // If we're testing synchronous stores, we shouldn't issue a Run() call so
    // that the test verifies that the operation completed synchronously.
    if (!RequireSynchronousStores())
      run_loop.Run();
    return store_success_;
  }

 protected:
  bool store_success_ = false;
  VariationsSeed stored_seed_;

 private:
  void OnSeedStoreResult(base::RepeatingClosure quit_closure,
                         bool store_success,
                         VariationsSeed seed) {
    store_success_ = store_success;
    stored_seed_.Swap(&seed);
    quit_closure.Run();
  }
};

class StoreSeedDataPreStableTest : public StoreSeedDataChannelTest {};

class StoreSeedDataStableAndUnknownTest : public StoreSeedDataChannelTest {};

class StoreSeedDataAllChannelsTest : public StoreSeedDataChannelTest {};

INSTANTIATE_TEST_SUITE_P(
    StoreSeedData,
    StoreSeedDataPreStableTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(version_info::Channel::CANARY,
                                             version_info::Channel::DEV,
                                             version_info::Channel::BETA))));

// Verifies that pre-stable clients write latest seeds to local state prefs and
// a seed file.
TEST_P(StoreSeedDataPreStableTest, StoreSeedData) {
  // Initialize SeedStore with test local state prefs and SeedReaderWriter.
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());
  seed_store.SetSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  // Store seed and force write for SeedReaderWriter.
  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Make sure seed in local state prefs matches the one created.
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsCompressedSeed),
            GzipAndBase64Encode(serialized_seed));

  // Make sure seed in seed file matches the one created.
  std::string seed_file_data;
  ASSERT_TRUE(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
  EXPECT_EQ(seed_file_data, Gzip(serialized_seed));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSeedDataStableAndUnknownTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(version_info::Channel::UNKNOWN,
                                             version_info::Channel::STABLE))));

// Verifies that stable and unknown channel clients write latest seeds only to
// local state prefs.
TEST_P(StoreSeedDataStableAndUnknownTest, StoreSeedData) {
  // Initialize SeedStore with test local state prefs and SeedReaderWriter.
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());
  seed_store.SetSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));

  // Make sure seed in local state prefs matches the one created.
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsCompressedSeed),
            GzipAndBase64Encode(serialized_seed));

  // Check there's no pending write to a seed file and that it was not created.
  EXPECT_FALSE(timer_.IsRunning());
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSeedDataAllChannelsTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(version_info::Channel::UNKNOWN,
                                             version_info::Channel::CANARY,
                                             version_info::Channel::DEV,
                                             version_info::Channel::BETA,
                                             version_info::Channel::STABLE))));

// Verifies that invalid latest seeds are not stored.
TEST_P(StoreSeedDataAllChannelsTest, StoreSeedData_InvalidSeed) {
  // Initialize SeedStore with test local state prefs and SeedReaderWriter.
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());
  seed_store.SetSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  // Check if trying to store a bad seed leaves the local state prefs unchanged.
  ASSERT_FALSE(StoreSeedData(seed_store, "should fail"));
  EXPECT_TRUE(PrefHasDefaultValue(prefs_, prefs::kVariationsCompressedSeed));

  // Check there's no pending write to a seed file and that it was not created.
  EXPECT_FALSE(timer_.IsRunning());
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));
}

TEST_P(StoreSeedDataAllChannelsTest, ParsedSeed) {
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());

  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));
  EXPECT_EQ(serialized_seed, SerializeSeed(stored_seed_));
}

TEST_P(StoreSeedDataAllChannelsTest, CountryCode) {
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());

  // Test with a valid header value.
  std::string seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(
      StoreSeedData(seed_store, seed, {.country_code = "test_country"}));
  EXPECT_EQ("test_country", prefs_.GetString(prefs::kVariationsCountry));

  // Test with no country code specified - which should preserve the old value.
  ASSERT_TRUE(StoreSeedData(seed_store, seed));
  EXPECT_EQ("test_country", prefs_.GetString(prefs::kVariationsCountry));
}

TEST_P(StoreSeedDataAllChannelsTest, GzippedSeed) {
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());

  const std::string serialized_seed = SerializeSeed(CreateTestSeed());
  ASSERT_TRUE(StoreSeedData(seed_store, Gzip(serialized_seed),
                            {.is_gzip_compressed = true}));
  EXPECT_EQ(serialized_seed, SerializeSeed(stored_seed_));
}

TEST_P(StoreSeedDataAllChannelsTest, GzippedEmptySeed) {
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());

  store_success_ = true;
  EXPECT_FALSE(StoreSeedData(seed_store, Gzip(/*data=*/std::string()),
                             {.is_gzip_compressed = true}));
}

TEST_P(StoreSeedDataAllChannelsTest, DeltaCompressed) {
  prefs_.SetString(prefs::kVariationsCompressedSeed,
                   kSeedDeltaTestData.GetInitialSeedDataAsPrefValue());
  prefs_.SetString(prefs::kVariationsSeedSignature, "ignored signature");

  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());

  ASSERT_TRUE(StoreSeedData(seed_store, kSeedDeltaTestData.GetDeltaData(),
                            {.is_delta_compressed = true}));
  EXPECT_EQ(kSeedDeltaTestData.GetNewSeedData(), SerializeSeed(stored_seed_));
}

TEST_P(StoreSeedDataAllChannelsTest, DeltaCompressedGzipped) {
  prefs_.SetString(prefs::kVariationsCompressedSeed,
                   kSeedDeltaTestData.GetInitialSeedDataAsPrefValue());
  prefs_.SetString(prefs::kVariationsSeedSignature, "ignored signature");

  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());

  ASSERT_TRUE(StoreSeedData(seed_store, Gzip(kSeedDeltaTestData.GetDeltaData()),
                            {
                                .is_delta_compressed = true,
                                .is_gzip_compressed = true,
                            }));
  EXPECT_EQ(kSeedDeltaTestData.GetNewSeedData(), SerializeSeed(stored_seed_));
}

TEST_P(StoreSeedDataAllChannelsTest, DeltaButNoInitialSeed) {
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());

  store_success_ = true;
  EXPECT_FALSE(StoreSeedData(seed_store,
                             Gzip(kSeedDeltaTestData.GetDeltaData()),
                             {
                                 .is_delta_compressed = true,
                                 .is_gzip_compressed = true,
                             }));
}

TEST_P(StoreSeedDataAllChannelsTest, BadDelta) {
  prefs_.SetString(prefs::kVariationsCompressedSeed,
                   kSeedDeltaTestData.GetInitialSeedDataAsPrefValue());
  prefs_.SetString(prefs::kVariationsSeedSignature, "ignored signature");

  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());

  store_success_ = true;
  // Provide a gzipped delta, when gzip is not expected.
  EXPECT_FALSE(StoreSeedData(seed_store,
                             Gzip(kSeedDeltaTestData.GetDeltaData()),
                             {.is_delta_compressed = true}));
}

TEST_P(StoreSeedDataAllChannelsTest, IdenticalToSafeSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);

  prefs_.SetString(prefs::kVariationsSafeCompressedSeed,
                   SerializeSeedBase64(seed));

  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());
  ASSERT_TRUE(StoreSeedData(seed_store, serialized_seed));

  // Verify that the pref has a sentinel value, rather than the full string.
  EXPECT_EQ(kIdenticalToSafeSeedSentinel,
            prefs_.GetString(prefs::kVariationsCompressedSeed));

  // Verify that loading the stored seed returns the original seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &unused_loaded_base64_seed_signature));

  EXPECT_EQ(serialized_seed, SerializeSeed(loaded_seed));
  EXPECT_EQ(serialized_seed, loaded_seed_data);
}

// Verifies that the cached serial number is correctly updated when a new seed
// is saved.
TEST_P(StoreSeedDataAllChannelsTest,
       GetLatestSerialNumber_UpdatedWithNewStoredSeed) {
  // Store good seed data initially.
  prefs_.SetString(prefs::kVariationsCompressedSeed,
                   SerializeSeedBase64(CreateTestSeed()));
  prefs_.SetString(prefs::kVariationsSeedSignature,
                   "a completely ignored signature");

  // Call GetLatestSerialNumber() once to prime the cached value.
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());
  EXPECT_EQ("123", seed_store.GetLatestSerialNumber());

  VariationsSeed new_seed = CreateTestSeed();
  new_seed.set_serial_number("456");
  ASSERT_TRUE(StoreSeedData(seed_store, SerializeSeed(new_seed)));
  EXPECT_EQ("456", seed_store.GetLatestSerialNumber());
}

TEST_F(VariationsSeedStoreTest, LoadSafeSeed_ValidSeed) {
  // Store good seed data to test if loading from prefs works.
  const VariationsSeed seed = CreateTestSeed();
  const std::string base64_seed = SerializeSeedBase64(seed);
  const base::Time reference_date = base::Time::Now();
  const std::string locale = "en-US";
  const std::string permanent_consistency_country = "us";
  const std::string session_consistency_country = "ca";

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, base64_seed);
  prefs.SetString(prefs::kVariationsSafeSeedSignature,
                  "a test signature, ignored.");
  prefs.SetTime(prefs::kVariationsSafeSeedDate, reference_date);
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime,
                reference_date - base::Days(3));
  prefs.SetString(prefs::kVariationsSafeSeedLocale, locale);
  prefs.SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry,
                  permanent_consistency_country);
  prefs.SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry,
                  session_consistency_country);

  // Attempt to load a valid safe seed.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_TRUE(seed_store.LoadSafeSeed(&loaded_seed, client_state.get()));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kSuccess, 1);

  // Check that the loaded data is the same as the original.
  EXPECT_EQ(SerializeSeed(seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(locale, client_state->locale);
  EXPECT_EQ(reference_date, client_state->reference_date);
  EXPECT_EQ(permanent_consistency_country,
            client_state->permanent_consistency_country);
  EXPECT_EQ(session_consistency_country,
            client_state->session_consistency_country);

  // Make sure that other data in the |client_state| hasn't been changed.
  std::unique_ptr<ClientFilterableState> original_state =
      CreateTestClientFilterableState();
  EXPECT_EQ(original_state->version, client_state->version);
  EXPECT_EQ(original_state->channel, client_state->channel);
  EXPECT_EQ(original_state->form_factor, client_state->form_factor);
  EXPECT_EQ(original_state->platform, client_state->platform);
  EXPECT_EQ(original_state->hardware_class, client_state->hardware_class);
  EXPECT_EQ(original_state->is_low_end_device, client_state->is_low_end_device);

  // Make sure the pref hasn't been changed.
  ASSERT_FALSE(
      PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));
  EXPECT_EQ(base64_seed, prefs.GetString(prefs::kVariationsSafeCompressedSeed));
}

TEST_F(VariationsSeedStoreTest, LoadSafeSeed_CorruptSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "this should fail");

  // Attempt to load a corrupted safe seed.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeed(&loaded_seed, client_state.get()));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kCorruptBase64, 1);
  CheckSafeSeedPrefsAreCleared(prefs);
  CheckRegularSeedPrefsAreSet(prefs);

  // Moreover, loading an invalid seed should leave the |client_state|
  // unmodified.
  std::unique_ptr<ClientFilterableState> original_state =
      CreateTestClientFilterableState();
  EXPECT_EQ(original_state->locale, client_state->locale);
  EXPECT_EQ(original_state->reference_date, client_state->reference_date);
  EXPECT_EQ(original_state->session_consistency_country,
            client_state->session_consistency_country);
  EXPECT_EQ(original_state->permanent_consistency_country,
            client_state->permanent_consistency_country);
}

TEST_F(VariationsSeedStoreTest, LoadSafeSeed_InvalidSignature) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  SetAllSeedPrefsToNonDefaultValues(&prefs);
  prefs.SetString(prefs::kVariationsSafeCompressedSeed,
                  SerializeSeedBase64(CreateTestSeed()));
  prefs.SetString(prefs::kVariationsSafeSeedSignature,
                  "a deeply compromised signature.");

  // Attempt to load a valid safe seed with an invalid signature while signature
  // verification is enabled.
  TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                     /*seed_file_dir=*/base::FilePath(),
                                     /*signature_verification_needed=*/true);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeed(&loaded_seed, client_state.get()));

  // Verify metrics and prefs.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kInvalidSignature, 1);
  CheckSafeSeedPrefsAreCleared(prefs);
  CheckRegularSeedPrefsAreSet(prefs);

  // Moreover, the passed-in |client_state| should remain unmodified.
  std::unique_ptr<ClientFilterableState> original_state =
      CreateTestClientFilterableState();
  EXPECT_EQ(original_state->locale, client_state->locale);
  EXPECT_EQ(original_state->reference_date, client_state->reference_date);
  EXPECT_EQ(original_state->session_consistency_country,
            client_state->session_consistency_country);
  EXPECT_EQ(original_state->permanent_consistency_country,
            client_state->permanent_consistency_country);
}

TEST_F(VariationsSeedStoreTest, LoadSafeSeed_EmptySeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  ASSERT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsSafeCompressedSeed));

  // Attempt to load an empty safe seed.
  TestVariationsSeedStore seed_store(&prefs);
  base::HistogramTester histogram_tester;
  VariationsSeed loaded_seed;
  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();
  ASSERT_FALSE(seed_store.LoadSafeSeed(&loaded_seed, client_state.get()));

  // Verify metrics.
  histogram_tester.ExpectUniqueSample("Variations.SafeMode.LoadSafeSeed.Result",
                                      LoadSeedResult::kEmpty, 1);
}

struct InvalidSafeSeedTestParams {
  const std::string test_name;
  const std::string seed;
  const std::string signature;
  StoreSeedResult store_seed_result;
  std::optional<VerifySignatureResult> verify_signature_result = std::nullopt;
};

struct StoreInvalidSafeSeedTestParams {
  using TupleT = std::tuple<InvalidSafeSeedTestParams, version_info::Channel>;

  InvalidSafeSeedTestParams invalid_params;
  version_info::Channel channel;

  explicit StoreInvalidSafeSeedTestParams(const TupleT& t)
      : invalid_params(std::get<0>(t)), channel(std::get<1>(t)) {}
};

class StoreInvalidSafeSeedTest
    : public StoreSeedTestBase,
      public ::testing::WithParamInterface<StoreInvalidSafeSeedTestParams> {
 public:
  StoreInvalidSafeSeedTest()
      : StoreSeedTestBase(prefs::kVariationsSafeCompressedSeed,
                          GetParam().channel) {}
  ~StoreInvalidSafeSeedTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreInvalidSafeSeedTest,
    ::testing::ConvertGenerator<
        StoreInvalidSafeSeedTestParams::TupleT>(::testing::Combine(
        ::testing::Values(
            InvalidSafeSeedTestParams{
                .test_name = "EmptySeed",
                .seed = "",
                .signature = "unused signature",
                .store_seed_result = StoreSeedResult::kFailedEmptyGzipContents},
            InvalidSafeSeedTestParams{
                .test_name = "InvalidSeed",
                .seed = "invalid seed",
                .signature = "unused signature",
                .store_seed_result = StoreSeedResult::kFailedParse},
            InvalidSafeSeedTestParams{
                .test_name = "InvalidSignature",
                .seed = SerializeSeed(CreateTestSeed()),
                // A well-formed signature that does not correspond to the seed.
                .signature = kTestSeedData.base64_signature,
                .store_seed_result = StoreSeedResult::kFailedSignature,
                .verify_signature_result =
                    VerifySignatureResult::INVALID_SEED}),
        ::testing::Values(version_info::Channel::CANARY,
                          version_info::Channel::DEV,
                          version_info::Channel::BETA,
                          version_info::Channel::STABLE,
                          version_info::Channel::UNKNOWN))),
    [](const ::testing::TestParamInfo<StoreInvalidSafeSeedTestParams>& params) {
      switch (params.param.channel) {
        case version_info::Channel::CANARY:
          return params.param.invalid_params.test_name + "_" + "CANARY";
        case version_info::Channel::DEV:
          return params.param.invalid_params.test_name + "_" + "DEV";
        case version_info::Channel::BETA:
          return params.param.invalid_params.test_name + "_" + "BETA";
        case version_info::Channel::STABLE:
          return params.param.invalid_params.test_name + "_" + "STABLE";
        default:
          return params.param.invalid_params.test_name + "_" + "UNKNOWN";
      }
    });

// Verify that attempting to store an invalid safe seed fails and does not
// modify Local State's safe-seed-related prefs or a seed file.
TEST_P(StoreInvalidSafeSeedTest, StoreSafeSeed) {
  // Set a safe seed in the seed file and local state prefs.
  const std::string expected_seed = "a seed";
  InvalidSafeSeedTestParams params = GetParam().invalid_params;
  const std::string seed_to_store = params.seed;
  prefs_.SetString(prefs::kVariationsSafeCompressedSeed, expected_seed);
  ASSERT_TRUE(base::WriteFile(temp_seed_file_path_, expected_seed));

  // Set associated safe seed local state prefs to their expected values.
  const std::string expected_signature = "a signature";
  prefs_.SetString(prefs::kVariationsSafeSeedSignature, expected_signature);

  const int expected_milestone = 90;
  prefs_.SetInteger(prefs::kVariationsSafeSeedMilestone, expected_milestone);

  const base::Time now = base::Time::Now();
  const base::Time expected_fetch_time = now - base::Hours(3);
  prefs_.SetTime(prefs::kVariationsSafeSeedFetchTime, expected_fetch_time);

  std::unique_ptr<ClientFilterableState> client_state =
      CreateTestClientFilterableState();

  const std::string expected_locale = "en-US";
  client_state->locale = "pt-PT";
  prefs_.SetString(prefs::kVariationsSafeSeedLocale, expected_locale);

  const std::string expected_permanent_consistency_country = "US";
  client_state->permanent_consistency_country = "CA";
  prefs_.SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry,
                   expected_permanent_consistency_country);

  const std::string expected_session_consistency_country = "BR";
  client_state->session_consistency_country = "PT";
  prefs_.SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry,
                   expected_session_consistency_country);

  const base::Time expected_date = now - base::Days(2);
  client_state->reference_date = now - base::Days(1);
  prefs_.SetTime(prefs::kVariationsSafeSeedDate, expected_date);

  TestVariationsSeedStore seed_store(&prefs_, version_info::Channel::UNKNOWN,
                                     /*seed_file_dir=*/base::FilePath(),
                                     /*signature_verification_needed=*/true);
  seed_store.SetSafeSeedReaderWriterForTesting(std::move(seed_reader_writer_));
  base::HistogramTester histogram_tester;

  // Verify that attempting to store an invalid seed fails.
  ASSERT_FALSE(
      seed_store.StoreSafeSeed(params.seed, params.signature,
                               /*seed_milestone=*/91, *client_state,
                               /*seed_fetch_time=*/now - base::Hours(1)));

  // Verify that the seed file has no pending writes and was not overwritten.
  ASSERT_FALSE(timer_.IsRunning());
  std::string seed_file_data;
  ASSERT_TRUE(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
  EXPECT_EQ(seed_file_data, expected_seed);

  // Verify that none of the safe seed prefs were overwritten.
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeCompressedSeed),
            expected_seed);
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedSignature),
            expected_signature);
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedLocale),
            expected_locale);
  EXPECT_EQ(prefs_.GetInteger(prefs::kVariationsSafeSeedMilestone),
            expected_milestone);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry),
      expected_permanent_consistency_country);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedSessionConsistencyCountry),
      expected_session_consistency_country);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedDate), expected_date);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedFetchTime),
            expected_fetch_time);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", params.store_seed_result, 1);
  if (params.verify_signature_result.has_value()) {
    histogram_tester.ExpectUniqueSample(
        "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
        params.verify_signature_result.value(), 1);
  }
}

class StoreSafeSeedDataChannelTest
    : public StoreSeedTestBase,
      public ::testing::WithParamInterface<StoreSeedDataTestParams> {
 public:
  StoreSafeSeedDataChannelTest()
      : StoreSeedTestBase(prefs::kVariationsSafeCompressedSeed,
                          GetParam().channel) {}
  ~StoreSafeSeedDataChannelTest() override = default;
};

class StoreSafeSeedDataPreStableTest : public StoreSafeSeedDataChannelTest {};
class StoreSafeSeedDataStableAndUnknownTest
    : public StoreSafeSeedDataChannelTest {};
class StoreSafeSeedDataAllChannelsTest : public StoreSafeSeedDataChannelTest {};

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSafeSeedDataPreStableTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(version_info::Channel::CANARY,
                                             version_info::Channel::DEV,
                                             version_info::Channel::BETA))));

TEST_P(StoreSafeSeedDataPreStableTest, StoreSafeSeed_ValidSignature) {
  auto client_state = CreateDummyClientFilterableState();
  const std::string expected_locale = "en-US";
  client_state->locale = expected_locale;
  const base::Time now = base::Time::Now();
  const base::Time expected_date = now - base::Days(1);
  client_state->reference_date = expected_date;
  const std::string expected_permanent_consistency_country = "US";
  client_state->permanent_consistency_country =
      expected_permanent_consistency_country;
  const std::string expected_session_consistency_country = "CA";
  client_state->session_consistency_country =
      expected_session_consistency_country;

  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  base::HistogramTester histogram_tester;
  seed_store.SetSafeSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  std::string expected_seed;
  ASSERT_TRUE(base::Base64Decode(kTestSeedData.base64_uncompressed_data,
                                 &expected_seed));
  const std::string expected_signature = kTestSeedData.base64_signature;
  const int expected_seed_milestone = 92;
  const base::Time expected_fetch_time = now - base::Hours(6);

  // Verify that storing the safe seed succeeded.
  ASSERT_TRUE(seed_store.StoreSafeSeed(expected_seed, expected_signature,
                                       expected_seed_milestone, *client_state,
                                       expected_fetch_time));
  // Force write for SeedReaderWriter.
  timer_.Fire();
  file_writer_thread_.FlushForTesting();

  // Make sure the seed was successfully stored in the seed file.
  std::string seed_file_data;
  EXPECT_TRUE(base::ReadFileToString(temp_seed_file_path_, &seed_file_data));
  EXPECT_EQ(seed_file_data, Gzip(expected_seed));

  // Verify that safe-seed-related prefs were successfully stored.
  std::string decoded_compressed_seed;
  ASSERT_TRUE(
      base::Base64Decode(prefs_.GetString(prefs::kVariationsSafeCompressedSeed),
                         &decoded_compressed_seed));
  EXPECT_EQ(Gzip(expected_seed), decoded_compressed_seed);
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedSignature),
            expected_signature);
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedLocale),
            expected_locale);
  EXPECT_EQ(prefs_.GetInteger(prefs::kVariationsSafeSeedMilestone),
            expected_seed_milestone);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry),
      expected_permanent_consistency_country);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedSessionConsistencyCountry),
      expected_session_consistency_country);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedDate), expected_date);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedFetchTime),
            expected_fetch_time);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
      VerifySignatureResult::VALID_SIGNATURE, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSafeSeedDataStableAndUnknownTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(version_info::Channel::UNKNOWN,
                                             version_info::Channel::STABLE))));

TEST_P(StoreSafeSeedDataStableAndUnknownTest, StoreSafeSeed_ValidSignature) {
  std::string expected_seed;
  ASSERT_TRUE(base::Base64Decode(kTestSeedData.base64_uncompressed_data,
                                 &expected_seed));
  const std::string expected_signature = kTestSeedData.base64_signature;
  const int expected_seed_milestone = 92;

  auto client_state = CreateDummyClientFilterableState();
  const std::string expected_locale = "en-US";
  client_state->locale = expected_locale;
  const base::Time now = base::Time::Now();
  const base::Time expected_date = now - base::Days(1);
  client_state->reference_date = expected_date;
  const std::string expected_permanent_consistency_country = "US";
  client_state->permanent_consistency_country =
      expected_permanent_consistency_country;
  const std::string expected_session_consistency_country = "CA";
  client_state->session_consistency_country =
      expected_session_consistency_country;
  const base::Time expected_fetch_time = now - base::Hours(6);

  // Initialize SeedStore with test prefs and SeedReaderWriter.
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);
  base::HistogramTester histogram_tester;
  seed_store.SetSafeSeedReaderWriterForTesting(std::move(seed_reader_writer_));

  // Verify that storing the safe seed succeeded.
  ASSERT_TRUE(seed_store.StoreSafeSeed(expected_seed, expected_signature,
                                       expected_seed_milestone, *client_state,
                                       expected_fetch_time));

  // Verify that the seed file has no pending or executed writes
  ASSERT_FALSE(timer_.IsRunning());
  EXPECT_FALSE(base::PathExists(temp_seed_file_path_));

  // Verify that safe-seed-related prefs were successfully stored.
  const std::string safe_seed =
      prefs_.GetString(prefs::kVariationsSafeCompressedSeed);
  std::string decoded_compressed_seed;
  ASSERT_TRUE(
      base::Base64Decode(prefs_.GetString(prefs::kVariationsSafeCompressedSeed),
                         &decoded_compressed_seed));
  EXPECT_EQ(Gzip(expected_seed), decoded_compressed_seed);
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedSignature),
            expected_signature);
  EXPECT_EQ(prefs_.GetString(prefs::kVariationsSafeSeedLocale),
            expected_locale);
  EXPECT_EQ(prefs_.GetInteger(prefs::kVariationsSafeSeedMilestone),
            expected_seed_milestone);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry),
      expected_permanent_consistency_country);
  EXPECT_EQ(
      prefs_.GetString(prefs::kVariationsSafeSeedSessionConsistencyCountry),
      expected_session_consistency_country);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedDate), expected_date);
  EXPECT_EQ(prefs_.GetTime(prefs::kVariationsSafeSeedFetchTime),
            expected_fetch_time);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
      VerifySignatureResult::VALID_SIGNATURE, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StoreSafeSeedDataAllChannelsTest,
    ::testing::ConvertGenerator<StoreSeedDataTestParams::TupleT>(
        ::testing::Combine(::testing::Bool(),
                           ::testing::Values(version_info::Channel::UNKNOWN,
                                             version_info::Channel::CANARY,
                                             version_info::Channel::DEV,
                                             version_info::Channel::BETA,
                                             version_info::Channel::STABLE))));

TEST_P(StoreSafeSeedDataAllChannelsTest, StoreSafeSeed_IdenticalToLatestSeed) {
  const VariationsSeed seed = CreateTestSeed();
  const std::string serialized_seed = SerializeSeed(seed);
  const std::string base64_seed = SerializeSeedBase64(seed);
  auto unused_client_state = CreateDummyClientFilterableState();

  prefs_.SetString(prefs::kVariationsCompressedSeed, base64_seed);
  const base::Time last_fetch_time = WrapTime(99999);
  prefs_.SetTime(prefs::kVariationsLastFetchTime, last_fetch_time);

  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(seed_store.StoreSafeSeed(
      serialized_seed, "a completely ignored signature", /*seed_milestone=*/92,
      *unused_client_state, /*seed_fetch_time=*/WrapTime(12345)));

  // Verify the latest seed value was migrated to a sentinel value, rather than
  // the full string.
  EXPECT_EQ(kIdenticalToSafeSeedSentinel,
            prefs_.GetString(prefs::kVariationsCompressedSeed));

  // Verify that loading the stored seed returns the original seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &unused_loaded_base64_seed_signature));

  EXPECT_EQ(serialized_seed, SerializeSeed(loaded_seed));
  EXPECT_EQ(serialized_seed, loaded_seed_data);

  // Verify that the safe seed prefs indeed contain the serialized seed value
  // and that the last fetch time was copied from the latest seed.
  EXPECT_EQ(base64_seed,
            prefs_.GetString(prefs::kVariationsSafeCompressedSeed));
  VariationsSeed loaded_safe_seed;
  EXPECT_TRUE(
      seed_store.LoadSafeSeed(&loaded_safe_seed, unused_client_state.get()));
  EXPECT_EQ(serialized_seed, SerializeSeed(loaded_safe_seed));
  EXPECT_EQ(last_fetch_time, seed_store.GetSafeSeedFetchTime());

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
}

TEST_P(StoreSafeSeedDataAllChannelsTest,
       StoreSafeSeed_PreviouslyIdenticalToLatestSeed) {
  // Create two distinct seeds: an old one saved as both the safe and the latest
  // seed value, and a new one that should overwrite only the stored safe seed
  // value.
  const VariationsSeed old_seed = CreateTestSeed();
  VariationsSeed new_seed = CreateTestSeed();
  new_seed.set_serial_number("12345678");
  ASSERT_NE(SerializeSeed(old_seed), SerializeSeed(new_seed));

  const std::string base64_old_seed = SerializeSeedBase64(old_seed);
  const base::Time fetch_time = WrapTime(12345);
  auto unused_client_state = CreateDummyClientFilterableState();

  prefs_.SetString(prefs::kVariationsSafeCompressedSeed, base64_old_seed);
  prefs_.SetString(prefs::kVariationsCompressedSeed,
                   kIdenticalToSafeSeedSentinel);

  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath());
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(seed_store.StoreSafeSeed(
      SerializeSeed(new_seed), "a completely ignored signature",
      /*seed_milestone=*/92, *unused_client_state, fetch_time));

  // Verify the latest seed value was copied before the safe seed was
  // overwritten.
  EXPECT_EQ(base64_old_seed,
            prefs_.GetString(prefs::kVariationsCompressedSeed));

  // Verify that loading the stored seed returns the old seed value.
  VariationsSeed loaded_seed;
  std::string loaded_seed_data;
  std::string unused_loaded_base64_seed_signature;
  ASSERT_TRUE(seed_store.LoadSeed(&loaded_seed, &loaded_seed_data,
                                  &unused_loaded_base64_seed_signature));

  EXPECT_EQ(SerializeSeed(old_seed), SerializeSeed(loaded_seed));
  EXPECT_EQ(SerializeSeed(old_seed), loaded_seed_data);

  // Verify that the safe seed prefs indeed contain the new seed's serialized
  // value.
  EXPECT_EQ(SerializeSeedBase64(new_seed),
            prefs_.GetString(prefs::kVariationsSafeCompressedSeed));
  VariationsSeed loaded_safe_seed;
  ASSERT_TRUE(
      seed_store.LoadSafeSeed(&loaded_safe_seed, unused_client_state.get()));
  EXPECT_EQ(SerializeSeed(new_seed), SerializeSeed(loaded_safe_seed));
  EXPECT_EQ(fetch_time, seed_store.GetSafeSeedFetchTime());

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Variations.SafeMode.StoreSafeSeed.Result", StoreSeedResult::kSuccess, 1);
}

TEST_F(VariationsSeedStoreTest, VerifySeedSignature) {
  // A valid seed and signature pair generated using the server's private key.
  const std::string uncompressed_base64_seed_data =
      kTestSeedData.base64_uncompressed_data;
  const std::string base64_seed_signature = kTestSeedData.base64_signature;

  std::string base64_seed_data;
  {
    std::string seed_data;
    ASSERT_TRUE(base::Base64Decode(uncompressed_base64_seed_data, &seed_data));
    VariationsSeed seed;
    ASSERT_TRUE(seed.ParseFromString(seed_data));
    base64_seed_data = SerializeSeedBase64(seed);
  }

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  // The above inputs should be valid.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);
    TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                       /*seed_file_dir=*/base::FilePath(),
                                       /*signature_verification_needed=*/true);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_data;
    std::string seed_signature;
    ASSERT_TRUE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::VALID_SIGNATURE),
        1);
  }

  // If there's no signature, the corresponding result should be returned.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, std::string());
    TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                       /*seed_file_dir=*/base::FilePath(),
                                       /*signature_verification_needed=*/true);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_data;
    std::string seed_signature;
    ASSERT_FALSE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::MISSING_SIGNATURE),
        1);
  }

  // Using non-base64 encoded value as signature should fail.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature,
                    "not a base64-encoded string");
    TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                       /*seed_file_dir=*/base::FilePath(),
                                       /*signature_verification_needed=*/true);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_data;
    std::string seed_signature;
    ASSERT_FALSE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::DECODE_FAILED),
        1);
  }

  // Using a different signature (e.g. the base64 seed data) should fail.
  // OpenSSL doesn't distinguish signature decode failure from the
  // signature not matching.
  {
    prefs.SetString(prefs::kVariationsCompressedSeed, base64_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_data);
    TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                       /*seed_file_dir=*/base::FilePath(),
                                       /*signature_verification_needed=*/true);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_data;
    std::string seed_signature;
    ASSERT_FALSE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::INVALID_SEED),
        1);
  }

  // Using a different seed should not match the signature.
  {
    std::string seed_data;
    ASSERT_TRUE(base::Base64Decode(uncompressed_base64_seed_data, &seed_data));
    VariationsSeed wrong_seed;
    ASSERT_TRUE(wrong_seed.ParseFromString(seed_data));
    (*wrong_seed.mutable_study(0)->mutable_name())[0] = 'x';
    std::string base64_wrong_seed_data = SerializeSeedBase64(wrong_seed);

    prefs.SetString(prefs::kVariationsCompressedSeed, base64_wrong_seed_data);
    prefs.SetString(prefs::kVariationsSeedSignature, base64_seed_signature);
    TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                       /*seed_file_dir=*/base::FilePath(),
                                       /*signature_verification_needed=*/true);

    base::HistogramTester histogram_tester;
    VariationsSeed seed;
    std::string seed_signature;
    ASSERT_FALSE(seed_store.LoadSeed(&seed, &seed_data, &seed_signature));
    histogram_tester.ExpectUniqueSample(
        "Variations.LoadSeedSignature",
        static_cast<base::HistogramBase::Sample>(
            VerifySignatureResult::INVALID_SEED),
        1);
  }
}

TEST_F(VariationsSeedStoreTest, LastFetchTime_DistinctSeeds) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, "one");
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "not one");
  prefs.SetTime(prefs::kVariationsLastFetchTime, WrapTime(1));
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime, WrapTime(0));

  base::Time start_time = WrapTime(10);
  TestVariationsSeedStore seed_store(&prefs);
  seed_store.RecordLastFetchTime(WrapTime(11));

  // Verify that the last fetch time was updated.
  const base::Time last_fetch_time =
      prefs.GetTime(prefs::kVariationsLastFetchTime);
  EXPECT_EQ(WrapTime(11), last_fetch_time);
  EXPECT_GE(last_fetch_time, start_time);

  // Verify that the safe seed's fetch time was *not* updated.
  EXPECT_EQ(WrapTime(0), prefs.GetTime(prefs::kVariationsSafeSeedFetchTime));
}

TEST_F(VariationsSeedStoreTest, LastFetchTime_IdenticalSeeds) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsSafeCompressedSeed, "some seed");
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  kIdenticalToSafeSeedSentinel);
  prefs.SetTime(prefs::kVariationsLastFetchTime, WrapTime(1));
  prefs.SetTime(prefs::kVariationsSafeSeedFetchTime, WrapTime(0));

  base::Time start_time = WrapTime(10);
  TestVariationsSeedStore seed_store(&prefs);
  seed_store.RecordLastFetchTime(WrapTime(11));

  // Verify that the last fetch time was updated.
  const base::Time last_fetch_time =
      prefs.GetTime(prefs::kVariationsLastFetchTime);
  EXPECT_EQ(WrapTime(11), last_fetch_time);
  EXPECT_GE(last_fetch_time, start_time);

  // Verify that the safe seed's fetch time *was* also updated.
  EXPECT_EQ(last_fetch_time,
            prefs.GetTime(prefs::kVariationsSafeSeedFetchTime));
}

TEST_F(VariationsSeedStoreTest, GetLatestSerialNumber_LoadsInitialValue) {
  // Store good seed data to test if loading from prefs works.
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed,
                  SerializeSeedBase64(CreateTestSeed()));
  prefs.SetString(prefs::kVariationsSeedSignature,
                  "a completely ignored signature");

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ("123", seed_store.GetLatestSerialNumber());
}

TEST_F(VariationsSeedStoreTest, GetLatestSerialNumber_EmptyWhenNoSeedIsSaved) {
  // Start with empty prefs.
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(std::string(), seed_store.GetLatestSerialNumber());
}

TEST_F(VariationsSeedStoreTest, GetLatestSerialNumber_ClearsPrefsOnFailure) {
  // Store corrupted seed data to test that prefs are cleared when loading
  // fails.
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetString(prefs::kVariationsCompressedSeed, "complete garbage");
  prefs.SetString(prefs::kVariationsSeedSignature, "an unused signature");

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(std::string(), seed_store.GetLatestSerialNumber());
  EXPECT_TRUE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
}

// Verifies that GetTimeForStudyDateChecks() returns the server timestamp for
// when the regular seed was fetched,|kVariationsSeedDate|, when the time is
// more recent than the build time.
TEST_F(VariationsSeedStoreTest, RegularSeedTimeReturned) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  const base::Time seed_fetch_time = base::GetBuildTime() + base::Days(4);
  prefs.SetTime(prefs::kVariationsSeedDate, seed_fetch_time);

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/false),
            seed_fetch_time);
}

// Verifies that GetTimeForStudyDateChecks() returns the server timestamp for
// when the safe seed was fetched, |kVariationsSafeSeedDate|, when the time is
// more recent than the build time.
TEST_F(VariationsSeedStoreTest, SafeSeedTimeReturned) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  const base::Time safe_seed_fetch_time = base::GetBuildTime() + base::Days(7);
  prefs.SetTime(prefs::kVariationsSafeSeedDate, safe_seed_fetch_time);

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/true),
            safe_seed_fetch_time);
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when it is
// more recent than |kVariationsSeedDate|.
TEST_F(VariationsSeedStoreTest, BuildTimeReturnedForRegularSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetTime(prefs::kVariationsSeedDate,
                base::GetBuildTime() - base::Days(2));

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/false),
            base::GetBuildTime());
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when it is
// more recent than |kVariationsSafeSeedDate|.
TEST_F(VariationsSeedStoreTest, BuildTimeReturnedForSafeSeed) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  prefs.SetTime(prefs::kVariationsSeedDate,
                base::GetBuildTime() - base::Days(3));

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/true),
            base::GetBuildTime());
}

// Verifies that GetTimeForStudyDateChecks() returns the build time when the
// seed time is null.
TEST_F(VariationsSeedStoreTest, BuildTimeReturnedForNullSeedTimes) {
  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  ASSERT_TRUE(prefs.GetTime(prefs::kVariationsSeedDate).is_null());

  TestVariationsSeedStore seed_store(&prefs);
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/false),
            base::GetBuildTime());

  ASSERT_TRUE(prefs.GetTime(prefs::kVariationsSafeSeedDate).is_null());
  EXPECT_EQ(seed_store.GetTimeForStudyDateChecks(/*is_safe_seed=*/true),
            base::GetBuildTime());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(VariationsSeedStoreTest, ImportFirstRunJavaSeed) {
  const std::string test_seed_data = "raw_seed_data_test";
  const std::string test_seed_signature = "seed_signature_test";
  const std::string test_seed_country = "seed_country_code_test";
  const int64_t test_response_date = 1234567890;
  const bool test_is_gzip_compressed = true;
  android::SetJavaFirstRunPrefsForTesting(test_seed_data, test_seed_signature,
                                          test_seed_country, test_response_date,
                                          test_is_gzip_compressed);

  auto seed = android::GetVariationsFirstRunSeed();
  EXPECT_EQ(test_seed_data, seed->data);
  EXPECT_EQ(test_seed_signature, seed->signature);
  EXPECT_EQ(test_seed_country, seed->country);
  EXPECT_EQ(test_response_date, seed->date.InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(test_is_gzip_compressed, seed->is_gzip_compressed);

  android::ClearJavaFirstRunPrefs();
  seed = android::GetVariationsFirstRunSeed();
  EXPECT_EQ("", seed->data);
  EXPECT_EQ("", seed->signature);
  EXPECT_EQ("", seed->country);
  EXPECT_EQ(0, seed->date.InMillisecondsSinceUnixEpoch());
  EXPECT_FALSE(seed->is_gzip_compressed);
}

class VariationsSeedStoreFirstRunPrefsTest
    : public ::testing::TestWithParam<bool> {
 private:
  base::test::TaskEnvironment task_environment_;
};

INSTANTIATE_TEST_SUITE_P(VariationsSeedStoreTest,
                         VariationsSeedStoreFirstRunPrefsTest,
                         ::testing::Bool());

TEST_P(VariationsSeedStoreFirstRunPrefsTest, FirstRunPrefsAllowed) {
  bool use_first_run_prefs = GetParam();

  const std::string test_seed_data = "raw_seed_data_test";
  const std::string test_seed_signature = "seed_signature_test";
  const std::string test_seed_country = "seed_country_code_test";
  const int64_t test_response_date = 1234567890;
  const bool test_is_gzip_compressed = true;
  android::SetJavaFirstRunPrefsForTesting(test_seed_data, test_seed_signature,
                                          test_seed_country, test_response_date,
                                          test_is_gzip_compressed);

  const VariationsSeed test_seed = CreateTestSeed();
  const std::string seed_data = SerializeSeed(test_seed);
  auto seed = std::make_unique<SeedResponse>();
  seed->data = seed_data;
  seed->signature = "java_seed_signature";
  seed->country = "java_seed_country";
  seed->date = base::Time::FromMillisecondsSinceUnixEpoch(test_response_date) +
               base::Days(1);
  seed->is_gzip_compressed = false;

  TestingPrefServiceSimple prefs;
  VariationsSeedStore::RegisterPrefs(prefs.registry());
  TestVariationsSeedStore seed_store(&prefs, version_info::Channel::UNKNOWN,
                                     /*seed_file_dir=*/base::FilePath(),
                                     /*signature_verification_needed=*/false,
                                     /*initial_seed=*/std::move(seed),
                                     use_first_run_prefs);

  seed = android::GetVariationsFirstRunSeed();

  // VariationsSeedStore must not modify Java prefs at all.
  EXPECT_EQ(test_seed_data, seed->data);
  EXPECT_EQ(test_seed_signature, seed->signature);
  EXPECT_EQ(test_seed_country, seed->country);
  EXPECT_EQ(test_response_date, seed->date.InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(test_is_gzip_compressed, seed->is_gzip_compressed);
  if (use_first_run_prefs) {
    EXPECT_TRUE(android::HasMarkedPrefsForTesting());
  } else {
    EXPECT_FALSE(android::HasMarkedPrefsForTesting());
  }

  // Seed should be stored in prefs.
  EXPECT_FALSE(PrefHasDefaultValue(prefs, prefs::kVariationsCompressedSeed));
  EXPECT_EQ(SerializeSeedBase64(test_seed),
            prefs.GetString(prefs::kVariationsCompressedSeed));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const featured::SeedDetails CreateDummySafeSeed(
    ClientFilterableState* client_state,
    base::Time fetch_time_to_store) {
  featured::SeedDetails expected_seed;
  expected_seed.set_b64_compressed_data(kTestSeedData.base64_compressed_data);
  expected_seed.set_signature(kTestSeedData.base64_signature);
  expected_seed.set_milestone(92);
  expected_seed.set_locale(client_state->locale);
  expected_seed.set_date(
      client_state->reference_date.ToDeltaSinceWindowsEpoch().InMilliseconds());
  expected_seed.set_permanent_consistency_country(
      client_state->permanent_consistency_country);
  expected_seed.set_session_consistency_country(
      client_state->session_consistency_country);
  expected_seed.set_fetch_time(
      fetch_time_to_store.ToDeltaSinceWindowsEpoch().InMilliseconds());
  return expected_seed;
}

// Checks that |platform_data| and |expected_data| deserialize to the same
// VariationsSeed proto.
// |platform_data| and |expected_data| are base64_compressed forms of seed data.
void ExpectSeedData(const std::string& platform_data,
                    const std::string& expected_data) {
  std::string decoded_platform_data;
  EXPECT_TRUE(base::Base64Decode(platform_data, &decoded_platform_data));
  std::string uncompressed_decoded_platform_data;
  EXPECT_TRUE(compression::GzipUncompress(decoded_platform_data,
                                          &uncompressed_decoded_platform_data));
  VariationsSeed platform_seed;
  EXPECT_TRUE(
      platform_seed.ParseFromString(uncompressed_decoded_platform_data));

  std::string decoded_expected_data;
  EXPECT_TRUE(base::Base64Decode(expected_data, &decoded_expected_data));
  std::string uncompressed_decoded_expected_data;
  EXPECT_TRUE(compression::GzipUncompress(decoded_expected_data,
                                          &uncompressed_decoded_expected_data));
  VariationsSeed expected_seed;
  EXPECT_TRUE(
      expected_seed.ParseFromString(uncompressed_decoded_expected_data));

  EXPECT_THAT(platform_seed, EqualsProto(expected_seed));
}

// Manually verifying each field in featured::SeedDetails rather than using
// EqualsProto is necessary because the
// featured::SeedDetails::b64_compressed_data field may be different between
// |platform| and |expected| even if the data unserializes to the same
// VariationsSeed. This could be caused by implementation differences between
// different versions of compression::GzipCompress.
//
// To accurately compare two featured::SeedDetails protos, the
// `b64_compressed_data` should be deserialized into a VariationsSeed proto and
// the two VariationsSeed protos should be compared.
void ExpectSafeSeed(const featured::SeedDetails& platform,
                    const featured::SeedDetails expected) {
  ExpectSeedData(platform.b64_compressed_data(),
                 expected.b64_compressed_data());
  EXPECT_EQ(platform.locale(), expected.locale());
  EXPECT_EQ(platform.milestone(), expected.milestone());
  EXPECT_EQ(platform.permanent_consistency_country(),
            expected.permanent_consistency_country());
  EXPECT_EQ(platform.session_consistency_country(),
            expected.session_consistency_country());
  EXPECT_EQ(platform.signature(), expected.signature());
  EXPECT_EQ(platform.date(), expected.date());
  EXPECT_EQ(platform.fetch_time(), expected.fetch_time());
}

TEST_P(StoreSafeSeedDataAllChannelsTest,
       SendSafeSeedToPlatform_SucceedFirstAttempt) {
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);

  ash::featured::FeaturedClient::InitializeFake();
  ash::featured::FakeFeaturedClient* client =
      ash::featured::FakeFeaturedClient::Get();
  client->AddResponse(true);

  std::unique_ptr<ClientFilterableState> client_state =
      CreateDummyClientFilterableState();
  const base::Time fetch_time_to_store = base::Time::Now() - base::Hours(1);
  featured::SeedDetails expected_platform_seed =
      CreateDummySafeSeed(client_state.get(), fetch_time_to_store);
  std::string expected_seed_data;
  ASSERT_TRUE(base::Base64Decode(kTestSeedData.base64_uncompressed_data,
                                 &expected_seed_data));

  // Verify that storing the safe seed succeeded.
  ASSERT_TRUE(seed_store.StoreSafeSeed(
      expected_seed_data, expected_platform_seed.signature(),
      expected_platform_seed.milestone(), *client_state, fetch_time_to_store));

  // Verify that the validated safe seed was received on Platform.
  ExpectSafeSeed(client->latest_safe_seed(), expected_platform_seed);
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 1);

  ash::featured::FeaturedClient::Shutdown();
}

TEST_P(StoreSafeSeedDataAllChannelsTest,
       SendSafeSeedToPlatform_FailFirstAttempt) {
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);

  ash::featured::FeaturedClient::InitializeFake();
  ash::featured::FakeFeaturedClient* client =
      ash::featured::FakeFeaturedClient::Get();
  client->AddResponse(false);
  client->AddResponse(true);

  std::unique_ptr<ClientFilterableState> client_state =
      CreateDummyClientFilterableState();
  const base::Time fetch_time_to_store = base::Time::Now() - base::Hours(1);
  featured::SeedDetails expected_platform_seed =
      CreateDummySafeSeed(client_state.get(), fetch_time_to_store);
  std::string expected_seed_data;
  ASSERT_TRUE(base::Base64Decode(kTestSeedData.base64_uncompressed_data,
                                 &expected_seed_data));

  // Verify that storing the safe seed succeeded.
  ASSERT_TRUE(seed_store.StoreSafeSeed(
      expected_seed_data, expected_platform_seed.signature(),
      expected_platform_seed.milestone(), *client_state, fetch_time_to_store));

  // Verify that the validated safe seed was received on Platform.
  ExpectSafeSeed(client->latest_safe_seed(), expected_platform_seed);
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 2);

  ash::featured::FeaturedClient::Shutdown();
}

TEST_P(StoreSafeSeedDataAllChannelsTest,
       SendSafeSeedToPlatform_FailTwoAttempts) {
  TestVariationsSeedStore seed_store(&prefs_, GetParam().channel,
                                     temp_dir_.GetPath(),
                                     /*signature_verification_needed=*/true);

  ash::featured::FeaturedClient::InitializeFake();
  ash::featured::FakeFeaturedClient* client =
      ash::featured::FakeFeaturedClient::Get();
  client->AddResponse(false);
  client->AddResponse(false);

  std::unique_ptr<ClientFilterableState> client_state =
      CreateDummyClientFilterableState();
  const base::Time fetch_time_to_store = base::Time::Now() - base::Hours(1);
  featured::SeedDetails seed =
      CreateDummySafeSeed(client_state.get(), fetch_time_to_store);
  std::string seed_data;
  ASSERT_TRUE(
      base::Base64Decode(kTestSeedData.base64_uncompressed_data, &seed_data));

  // Verify that storing the safe seed succeeded.
  ASSERT_TRUE(seed_store.StoreSafeSeed(seed_data, seed.signature(),
                                       seed.milestone(), *client_state,
                                       fetch_time_to_store));

  // Verify that the validated safe seed was not received on Platform.
  featured::SeedDetails empty_seed;
  EXPECT_THAT(client->latest_safe_seed(), EqualsProto(empty_seed));
  EXPECT_EQ(client->handle_seed_fetched_attempts(), 2);

  ash::featured::FeaturedClient::Shutdown();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace variations
