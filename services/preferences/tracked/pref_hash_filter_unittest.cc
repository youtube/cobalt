// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_filter.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/testing_pref_store.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/preferences/public/cpp/tracked/configuration.h"
#include "services/preferences/public/cpp/tracked/mock_validation_delegate.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"
#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/pref_hash_store.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using EnforcementLevel =
    prefs::mojom::TrackedPreferenceMetadata::EnforcementLevel;
using PrefTrackingStrategy =
    prefs::mojom::TrackedPreferenceMetadata::PrefTrackingStrategy;
using ValueType = prefs::mojom::TrackedPreferenceMetadata::ValueType;
using ValueState =
    prefs::mojom::TrackedPreferenceValidationDelegate::ValueState;

const char kAtomicPref[] = "atomic_pref";
const char kAtomicPref2[] = "atomic_pref2";
const char kAtomicPref3[] = "pref3";
const char kAtomicPref4[] = "pref4";
const char kDeprecatedTrackedDictionaryEntry[] = "dictionary.pref";
const char kDeprecatedUntrackedDictionary[] = "dictionary";
const char kReportOnlyPref[] = "report_only";
const char kReportOnlySplitPref[] = "report_only_split_pref";
const char kSplitPref[] = "split_pref";

const prefs::TrackedPreferenceMetadata kTestTrackedPrefs[] = {
    {0, kAtomicPref, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::PERSONAL},
    {1, kReportOnlyPref, EnforcementLevel::NO_ENFORCEMENT,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {2, kSplitPref, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::SPLIT, ValueType::IMPERSONAL},
    {3, kReportOnlySplitPref, EnforcementLevel::NO_ENFORCEMENT,
     PrefTrackingStrategy::SPLIT, ValueType::IMPERSONAL},
    {4, kAtomicPref2, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {5, kAtomicPref3, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {6, kAtomicPref4, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
};

}  // namespace

// A PrefHashStore that allows simulation of CheckValue results and captures
// checked and stored values.
class MockPrefHashStore : public PrefHashStore {
 public:
  typedef std::pair<const void*, PrefTrackingStrategy> ValuePtrStrategyPair;

  MockPrefHashStore()
      : stamp_super_mac_result_(false),
        is_super_mac_valid_result_(false),
        transactions_performed_(0),
        transaction_active_(false) {}

  MockPrefHashStore(const MockPrefHashStore&) = delete;
  MockPrefHashStore& operator=(const MockPrefHashStore&) = delete;

  ~MockPrefHashStore() override { EXPECT_FALSE(transaction_active_); }

  // Set the result that will be returned when |path| is passed to
  // |CheckValue/CheckSplitValue|.
  void SetCheckResult(const std::string& path, ValueState result);

  // Set the invalid_keys that will be returned when |path| is passed to
  // |CheckSplitValue|. SetCheckResult should already have been called for
  // |path| with |result == CHANGED| for this to make any sense.
  void SetInvalidKeysResult(
      const std::string& path,
      const std::vector<std::string>& invalid_keys_result);

  // Sets the value that will be returned from
  // PrefHashStoreTransaction::StampSuperMAC().
  void set_stamp_super_mac_result(bool result) {
    stamp_super_mac_result_ = result;
  }

  // Sets the value that will be returned from
  // PrefHashStoreTransaction::IsSuperMACValid().
  void set_is_super_mac_valid_result(bool result) {
    is_super_mac_valid_result_ = result;
  }

  // Returns the number of transactions that were performed.
  size_t transactions_performed() { return transactions_performed_; }

  // Returns the number of paths checked.
  size_t checked_paths_count() const { return checked_values_.size(); }

  // Returns the number of paths stored.
  size_t stored_paths_count() const { return stored_values_.size(); }

  // Returns the pointer value and strategy that was passed to
  // |CheckHash/CheckSplitHash| for |path|. The returned pointer could since
  // have been freed and is thus not safe to dereference.
  ValuePtrStrategyPair checked_value(const std::string& path) const {
    std::map<std::string, ValuePtrStrategyPair>::const_iterator value =
        checked_values_.find(path);
    if (value != checked_values_.end())
      return value->second;
    return std::make_pair(reinterpret_cast<void*>(0xBAD),
                          static_cast<PrefTrackingStrategy>(-1));
  }

  // Returns the pointer value that was passed to |StoreHash/StoreSplitHash| for
  // |path|. The returned pointer could since have been freed and is thus not
  // safe to dereference.
  ValuePtrStrategyPair stored_value(const std::string& path) const {
    std::map<std::string, ValuePtrStrategyPair>::const_iterator value =
        stored_values_.find(path);
    if (value != stored_values_.end())
      return value->second;
    return std::make_pair(reinterpret_cast<void*>(0xBAD),
                          static_cast<PrefTrackingStrategy>(-1));
  }

  // PrefHashStore implementation.
  std::unique_ptr<PrefHashStoreTransaction> BeginTransaction(
      HashStoreContents* storage) override;
  std::string ComputeMac(const std::string& path,
                         const base::Value* new_value) override;
  base::Value::Dict ComputeSplitMacs(
      const std::string& path,
      const base::Value::Dict* split_values) override;

 private:
  // A MockPrefHashStoreTransaction is handed to the caller on
  // MockPrefHashStore::BeginTransaction(). It then stores state in its
  // underlying MockPrefHashStore about calls it receives from that same caller
  // which can later be verified in tests.
  class MockPrefHashStoreTransaction : public PrefHashStoreTransaction {
   public:
    explicit MockPrefHashStoreTransaction(MockPrefHashStore* outer)
        : outer_(outer) {}

    MockPrefHashStoreTransaction(const MockPrefHashStoreTransaction&) = delete;
    MockPrefHashStoreTransaction& operator=(
        const MockPrefHashStoreTransaction&) = delete;

    ~MockPrefHashStoreTransaction() override {
      outer_->transaction_active_ = false;
      ++outer_->transactions_performed_;
    }

    // PrefHashStoreTransaction implementation.
    base::StringPiece GetStoreUMASuffix() const override;
    ValueState CheckValue(const std::string& path,
                          const base::Value* value) const override;
    void StoreHash(const std::string& path,
                   const base::Value* new_value) override;
    ValueState CheckSplitValue(
        const std::string& path,
        const base::Value::Dict* initial_split_value,
        std::vector<std::string>* invalid_keys) const override;
    void StoreSplitHash(const std::string& path,
                        const base::Value::Dict* split_value) override;
    bool HasHash(const std::string& path) const override;
    void ImportHash(const std::string& path, const base::Value* hash) override;
    void ClearHash(const std::string& path) override;
    bool IsSuperMACValid() const override;
    bool StampSuperMac() override;

   private:
    raw_ptr<MockPrefHashStore> outer_;
  };

  // Records a call to this mock's CheckValue/CheckSplitValue methods.
  ValueState RecordCheckValue(const std::string& path,
                              const void* value,
                              PrefTrackingStrategy strategy);

  // Records a call to this mock's StoreHash/StoreSplitHash methods.
  void RecordStoreHash(const std::string& path,
                       const void* new_value,
                       PrefTrackingStrategy strategy);
  void ClearStoreHash(const std::string& path);

  std::map<std::string, ValueState> check_results_;
  std::map<std::string, std::vector<std::string>> invalid_keys_results_;

  bool stamp_super_mac_result_;
  bool is_super_mac_valid_result_;

  std::map<std::string, ValuePtrStrategyPair> checked_values_;
  std::map<std::string, ValuePtrStrategyPair> stored_values_;

  // Number of transactions that were performed via this MockPrefHashStore.
  size_t transactions_performed_;

  // Whether a transaction is currently active (only one transaction should be
  // active at a time).
  bool transaction_active_;
};

void MockPrefHashStore::SetCheckResult(const std::string& path,
                                       ValueState result) {
  check_results_.insert(std::make_pair(path, result));
}

void MockPrefHashStore::SetInvalidKeysResult(
    const std::string& path,
    const std::vector<std::string>& invalid_keys_result) {
  // Ensure |check_results_| has a CHANGED entry for |path|.
  std::map<std::string, ValueState>::const_iterator result =
      check_results_.find(path);
  ASSERT_TRUE(result != check_results_.end());
  ASSERT_EQ(ValueState::CHANGED, result->second);

  invalid_keys_results_.insert(std::make_pair(path, invalid_keys_result));
}

std::unique_ptr<PrefHashStoreTransaction> MockPrefHashStore::BeginTransaction(
    HashStoreContents* storage) {
  EXPECT_FALSE(transaction_active_);
  return std::unique_ptr<PrefHashStoreTransaction>(
      new MockPrefHashStoreTransaction(this));
}

std::string MockPrefHashStore::ComputeMac(const std::string& path,
                                          const base::Value* new_value) {
  return "atomic mac for: " + path;
}

base::Value::Dict MockPrefHashStore::ComputeSplitMacs(
    const std::string& path,
    const base::Value::Dict* split_values) {
  base::Value::Dict macs_dict;
  if (!split_values)
    return macs_dict;
  for (const auto item : *split_values) {
    macs_dict.Set(item.first,
                  base::Value("split mac for: " + path + "/" + item.first));
  }
  return macs_dict;
}

ValueState MockPrefHashStore::RecordCheckValue(const std::string& path,
                                               const void* value,
                                               PrefTrackingStrategy strategy) {
  // Record that |path| was checked and validate that it wasn't previously
  // checked.
  EXPECT_TRUE(checked_values_
                  .insert(std::make_pair(path, std::make_pair(value, strategy)))
                  .second);
  std::map<std::string, ValueState>::const_iterator result =
      check_results_.find(path);
  if (result != check_results_.end())
    return result->second;
  return ValueState::UNCHANGED;
}

void MockPrefHashStore::RecordStoreHash(const std::string& path,
                                        const void* new_value,
                                        PrefTrackingStrategy strategy) {
  EXPECT_TRUE(
      stored_values_
          .insert(std::make_pair(path, std::make_pair(new_value, strategy)))
          .second);
}

void MockPrefHashStore::ClearStoreHash(const std::string& path) {
  stored_values_.erase(path);
}

base::StringPiece
MockPrefHashStore::MockPrefHashStoreTransaction ::GetStoreUMASuffix() const {
  return "unused";
}

ValueState MockPrefHashStore::MockPrefHashStoreTransaction::CheckValue(
    const std::string& path,
    const base::Value* value) const {
  return outer_->RecordCheckValue(path, value, PrefTrackingStrategy::ATOMIC);
}

void MockPrefHashStore::MockPrefHashStoreTransaction::StoreHash(
    const std::string& path,
    const base::Value* new_value) {
  outer_->RecordStoreHash(path, new_value, PrefTrackingStrategy::ATOMIC);
}

ValueState MockPrefHashStore::MockPrefHashStoreTransaction::CheckSplitValue(
    const std::string& path,
    const base::Value::Dict* initial_split_value,
    std::vector<std::string>* invalid_keys) const {
  EXPECT_TRUE(invalid_keys && invalid_keys->empty());

  std::map<std::string, std::vector<std::string>>::const_iterator
      invalid_keys_result = outer_->invalid_keys_results_.find(path);
  if (invalid_keys_result != outer_->invalid_keys_results_.end()) {
    invalid_keys->insert(invalid_keys->begin(),
                         invalid_keys_result->second.begin(),
                         invalid_keys_result->second.end());
  }

  return outer_->RecordCheckValue(path, initial_split_value,
                                  PrefTrackingStrategy::SPLIT);
}

void MockPrefHashStore::MockPrefHashStoreTransaction::StoreSplitHash(
    const std::string& path,
    const base::Value::Dict* new_value) {
  outer_->RecordStoreHash(path, new_value, PrefTrackingStrategy::SPLIT);
}

bool MockPrefHashStore::MockPrefHashStoreTransaction::HasHash(
    const std::string& path) const {
  ADD_FAILURE() << "Unexpected call.";
  return false;
}

void MockPrefHashStore::MockPrefHashStoreTransaction::ImportHash(
    const std::string& path,
    const base::Value* hash) {
  ADD_FAILURE() << "Unexpected call.";
}

void MockPrefHashStore::MockPrefHashStoreTransaction::ClearHash(
    const std::string& path) {
  // Allow this to be called by PrefHashFilter's deprecated tracked prefs
  // cleanup tasks.
  outer_->ClearStoreHash(path);
}

bool MockPrefHashStore::MockPrefHashStoreTransaction::IsSuperMACValid() const {
  return outer_->is_super_mac_valid_result_;
}

bool MockPrefHashStore::MockPrefHashStoreTransaction::StampSuperMac() {
  return outer_->stamp_super_mac_result_;
}

std::vector<prefs::mojom::TrackedPreferenceMetadataPtr> GetConfiguration(
    EnforcementLevel max_enforcement_level) {
  auto configuration = prefs::ConstructTrackedConfiguration(kTestTrackedPrefs);
  for (const auto& metadata : configuration) {
    if (metadata->enforcement_level > max_enforcement_level)
      metadata->enforcement_level = max_enforcement_level;
  }
  return configuration;
}

class MockHashStoreContents : public HashStoreContents {
 public:
  MockHashStoreContents() {}

  MockHashStoreContents(const MockHashStoreContents&) = delete;
  MockHashStoreContents& operator=(const MockHashStoreContents&) = delete;

  // Returns the number of hashes stored.
  size_t stored_hashes_count() const { return dictionary_.size(); }

  // Returns the number of paths cleared.
  size_t cleared_paths_count() const { return removed_entries_.size(); }

  // Returns the stored MAC for an Atomic preference.
  std::string GetStoredMac(const std::string& path) const;
  // Returns the stored MAC for a Split preference.
  std::string GetStoredSplitMac(const std::string& path,
                                const std::string& split_path) const;

  // HashStoreContents implementation.
  bool IsCopyable() const override;
  std::unique_ptr<HashStoreContents> MakeCopy() const override;
  base::StringPiece GetUMASuffix() const override;
  void Reset() override;
  bool GetMac(const std::string& path, std::string* out_value) override;
  bool GetSplitMacs(const std::string& path,
                    std::map<std::string, std::string>* split_macs) override;
  void SetMac(const std::string& path, const std::string& value) override;
  void SetSplitMac(const std::string& path,
                   const std::string& split_path,
                   const std::string& value) override;
  void ImportEntry(const std::string& path,
                   const base::Value* in_value) override;
  bool RemoveEntry(const std::string& path) override;
  const base::Value::Dict* GetContents() const override;
  std::string GetSuperMac() const override;
  void SetSuperMac(const std::string& super_mac) override;

 private:
  explicit MockHashStoreContents(MockHashStoreContents* origin_mock);

  // Records calls to this mock's SetMac/SetSplitMac methods.
  void RecordSetMac(const std::string& path, const std::string& mac) {
    dictionary_.Set(path, mac);
  }
  void RecordSetSplitMac(const std::string& path,
                         const std::string& split_path,
                         const std::string& mac) {
    dictionary_.SetByDottedPath(base::StrCat({path, ".", split_path}), mac);
  }

  // Records a call to this mock's RemoveEntry method.
  void RecordRemoveEntry(const std::string& path) {
    // Don't expect the same pref to be cleared more than once.
    EXPECT_EQ(removed_entries_.end(), removed_entries_.find(path));
    removed_entries_.insert(path);
  }

  base::Value::Dict dictionary_;
  std::set<std::string> removed_entries_;

  // The code being tested copies its HashStoreContents for use in a callback
  // which can be executed during shutdown. To be able to capture the behavior
  // of the copy, we make it forward calls to the mock it was created from.
  // Once set, |origin_mock_| must outlive this instance.
  raw_ptr<MockHashStoreContents> origin_mock_;
};

std::string MockHashStoreContents::GetStoredMac(const std::string& path) const {
  const base::Value* out_value = dictionary_.Find(path);
  if (out_value) {
    EXPECT_TRUE(out_value->is_string());

    return out_value->GetString();
  }

  return std::string();
}

std::string MockHashStoreContents::GetStoredSplitMac(
    const std::string& path,
    const std::string& split_path) const {
  const base::Value* out_value = dictionary_.Find(path);
  if (out_value) {
    EXPECT_TRUE(out_value->is_dict());

    out_value = dictionary_.Find(split_path);
    if (out_value) {
      EXPECT_TRUE(out_value->is_string());

      return out_value->GetString();
    }
  }

  return std::string();
}

MockHashStoreContents::MockHashStoreContents(MockHashStoreContents* origin_mock)
    : origin_mock_(origin_mock) {}

bool MockHashStoreContents::IsCopyable() const {
  return true;
}

std::unique_ptr<HashStoreContents> MockHashStoreContents::MakeCopy() const {
  // Return a new MockHashStoreContents which forwards all requests to this
  // mock instance.
  return std::unique_ptr<HashStoreContents>(
      new MockHashStoreContents(const_cast<MockHashStoreContents*>(this)));
}

base::StringPiece MockHashStoreContents::GetUMASuffix() const {
  return "Unused";
}

void MockHashStoreContents::Reset() {
  ADD_FAILURE() << "Unexpected call.";
}

bool MockHashStoreContents::GetMac(const std::string& path,
                                   std::string* out_value) {
  ADD_FAILURE() << "Unexpected call.";
  return false;
}

bool MockHashStoreContents::GetSplitMacs(
    const std::string& path,
    std::map<std::string, std::string>* split_macs) {
  ADD_FAILURE() << "Unexpected call.";
  return false;
}

void MockHashStoreContents::SetMac(const std::string& path,
                                   const std::string& value) {
  if (origin_mock_)
    origin_mock_->RecordSetMac(path, value);
  else
    RecordSetMac(path, value);
}

void MockHashStoreContents::SetSplitMac(const std::string& path,
                                        const std::string& split_path,
                                        const std::string& value) {
  if (origin_mock_)
    origin_mock_->RecordSetSplitMac(path, split_path, value);
  else
    RecordSetSplitMac(path, split_path, value);
}

void MockHashStoreContents::ImportEntry(const std::string& path,
                                        const base::Value* in_value) {
  ADD_FAILURE() << "Unexpected call.";
}

bool MockHashStoreContents::RemoveEntry(const std::string& path) {
  if (origin_mock_)
    origin_mock_->RecordRemoveEntry(path);
  else
    RecordRemoveEntry(path);
  return true;
}

const base::Value::Dict* MockHashStoreContents::GetContents() const {
  ADD_FAILURE() << "Unexpected call.";
  return nullptr;
}

std::string MockHashStoreContents::GetSuperMac() const {
  ADD_FAILURE() << "Unexpected call.";
  return std::string();
}

void MockHashStoreContents::SetSuperMac(const std::string& super_mac) {
  ADD_FAILURE() << "Unexpected call.";
}

class PrefHashFilterTest : public testing::TestWithParam<EnforcementLevel>,
                           public prefs::mojom::ResetOnLoadObserver {
 public:
  PrefHashFilterTest()
      : mock_pref_hash_store_(nullptr),
        mock_validation_delegate_record_(new MockValidationDelegateRecord),
        mock_validation_delegate_(mock_validation_delegate_record_),
        validation_delegate_receiver_(&mock_validation_delegate_),
        reset_recorded_(false) {}

  PrefHashFilterTest(const PrefHashFilterTest&) = delete;
  PrefHashFilterTest& operator=(const PrefHashFilterTest&) = delete;

  void SetUp() override {
    Reset();
  }

 protected:
  // Reset the PrefHashFilter instance.
  void Reset() {
    // Construct a PrefHashFilter and MockPrefHashStore for the test.
    InitializePrefHashFilter(GetConfiguration(GetParam()));
  }

  // Initializes |pref_hash_filter_| with a PrefHashFilter that uses a
  // MockPrefHashStore. The raw pointer to the MockPrefHashStore (owned by the
  // PrefHashFilter) is stored in |mock_pref_hash_store_|.
  void InitializePrefHashFilter(
      std::vector<prefs::mojom::TrackedPreferenceMetadataPtr> configuration) {
    std::unique_ptr<MockPrefHashStore> temp_mock_pref_hash_store(
        new MockPrefHashStore);
    std::unique_ptr<MockPrefHashStore>
        temp_mock_external_validation_pref_hash_store(new MockPrefHashStore);
    std::unique_ptr<MockHashStoreContents>
        temp_mock_external_validation_hash_store_contents(
            new MockHashStoreContents);
    mock_pref_hash_store_ = temp_mock_pref_hash_store.get();
    mock_external_validation_pref_hash_store_ =
        temp_mock_external_validation_pref_hash_store.get();
    mock_external_validation_hash_store_contents_ =
        temp_mock_external_validation_hash_store_contents.get();
    mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver>
        reset_on_load_observer;
    reset_on_load_observer_receivers_.Add(
        this, reset_on_load_observer.InitWithNewPipeAndPassReceiver());
    mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>
        validation_delegate_remote(
            validation_delegate_receiver_.BindNewPipeAndPassRemote());
    auto validation_delegate_remote_ref =
        base::MakeRefCounted<base::RefCountedData<
            mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>>>(
            std::move(validation_delegate_remote));
    pref_hash_filter_ = std::make_unique<PrefHashFilter>(
        std::move(temp_mock_pref_hash_store),
        PrefHashFilter::StoreContentsPair(
            std::move(temp_mock_external_validation_pref_hash_store),
            std::move(temp_mock_external_validation_hash_store_contents)),
        std::move(configuration), std::move(reset_on_load_observer),
        std::move(validation_delegate_remote_ref),
        std::size(kTestTrackedPrefs));
  }

  // Verifies whether a reset was reported by the PrefHashFiler. Also verifies
  // that kPreferenceResetTime was set (or not) accordingly.
  void VerifyRecordedReset(bool reset_expected) {
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(reset_expected, reset_recorded_);
    EXPECT_EQ(reset_expected, !!pref_store_contents_.FindByDottedPath(
                                  user_prefs::kPreferenceResetTime));
  }

  // Calls FilterOnLoad() on |pref_hash_Filter_|. |pref_store_contents_| is
  // handed off, but should be given back to us synchronously through
  // GetPrefsBack() as there is no FilterOnLoadInterceptor installed on
  // |pref_hash_filter_|.
  void DoFilterOnLoad(bool expect_prefs_modifications) {
    pref_hash_filter_->FilterOnLoad(
        base::BindOnce(&PrefHashFilterTest::GetPrefsBack,
                       base::Unretained(this), expect_prefs_modifications),
        std::move(pref_store_contents_));
  }

  raw_ptr<MockPrefHashStore, DanglingUntriaged> mock_pref_hash_store_;
  raw_ptr<MockPrefHashStore, DanglingUntriaged>
      mock_external_validation_pref_hash_store_;
  raw_ptr<MockHashStoreContents, DanglingUntriaged>
      mock_external_validation_hash_store_contents_;
  base::Value::Dict pref_store_contents_;
  scoped_refptr<MockValidationDelegateRecord> mock_validation_delegate_record_;
  std::unique_ptr<PrefHashFilter> pref_hash_filter_;

 private:
  // Stores |prefs| back in |pref_store_contents| and ensure
  // |expected_schedule_write| matches the reported |schedule_write|.
  void GetPrefsBack(bool expected_schedule_write,
                    base::Value::Dict prefs,
                    bool schedule_write) {
    pref_store_contents_ = std::move(prefs);
    EXPECT_EQ(expected_schedule_write, schedule_write);
  }

  void OnResetOnLoad() override {
    // As-is |reset_recorded_| is only designed to remember a single reset, make
    // sure none was previously recorded.
    EXPECT_FALSE(reset_recorded_);
    reset_recorded_ = true;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockValidationDelegate mock_validation_delegate_;
  mojo::Receiver<prefs::mojom::TrackedPreferenceValidationDelegate>
      validation_delegate_receiver_;
  mojo::ReceiverSet<prefs::mojom::ResetOnLoadObserver>
      reset_on_load_observer_receivers_;
  bool reset_recorded_;
};

TEST_P(PrefHashFilterTest, EmptyAndUnchanged) {
  DoFilterOnLoad(false);
  // All paths checked.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  // No paths stored, since they all return |UNCHANGED|.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
  // Since there was nothing in |pref_store_contents_| the checked value should
  // have been nullptr for all tracked preferences.
  for (const auto& pref : kTestTrackedPrefs) {
    ASSERT_FALSE(mock_pref_hash_store_->checked_value(pref.name).first);
  }
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);

  // Delegate saw all paths, and all unchanged.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));
}

TEST_P(PrefHashFilterTest, StampSuperMACAltersStore) {
  mock_pref_hash_store_->set_stamp_super_mac_result(true);
  DoFilterOnLoad(true);
  // No paths stored, since they all return |UNCHANGED|. The StampSuperMAC
  // result is the only reason the prefs were considered altered.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
}

TEST_P(PrefHashFilterTest, FilterTrackedPrefUpdate) {
  base::Value::Dict root_dict;
  base::Value* string_value = root_dict.Set(kAtomicPref, "string value");

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData.
  pref_hash_filter_->FilterSerializeData(root_dict);
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(string_value, stored_value.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_value.second);

  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);
}

TEST_P(PrefHashFilterTest, FilterTrackedPrefClearing) {
  base::Value::Dict root_dict;
  // We don't actually add the pref's value to root_dict to simulate that
  // it was just cleared in the PrefStore.

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData, with no value.
  pref_hash_filter_->FilterSerializeData(root_dict);
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_FALSE(stored_value.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_value.second);

  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);
}

TEST_P(PrefHashFilterTest, FilterSplitPrefUpdate) {
  base::Value::Dict root_dict;
  base::Value::Dict& dict_value =
      root_dict.Set(kSplitPref, base::Value::Dict())->GetDict();
  dict_value.Set("a", "foo");
  dict_value.Set("b", 1234);

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kSplitPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData.
  pref_hash_filter_->FilterSerializeData(root_dict);
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(&dict_value, stored_value.first);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_value.second);

  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);
}

TEST_P(PrefHashFilterTest, FilterTrackedSplitPrefClearing) {
  base::Value::Dict root_dict;
  // We don't actually add the pref's value to root_dict to simulate that
  // it was just cleared in the PrefStore.

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kSplitPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData, with no value.
  pref_hash_filter_->FilterSerializeData(root_dict);
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_FALSE(stored_value.first);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_value.second);

  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());
  VerifyRecordedReset(false);
}

TEST_P(PrefHashFilterTest, FilterUntrackedPrefUpdate) {
  base::Value::Dict root_dict;
  root_dict.Set("untracked", "some value");
  pref_hash_filter_->FilterUpdate("untracked");

  // No paths should be stored on FilterUpdate.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // Nor on FilterSerializeData.
  pref_hash_filter_->FilterSerializeData(root_dict);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // No transaction should even be started on FilterSerializeData() if there are
  // no updates to perform.
  ASSERT_EQ(0u, mock_pref_hash_store_->transactions_performed());
}

TEST_P(PrefHashFilterTest, MultiplePrefsFilterSerializeData) {
  base::Value::Dict root_dict;
  base::Value* int_value1 = root_dict.Set(kAtomicPref, 1);
  root_dict.Set(kAtomicPref2, 2);
  root_dict.Set(kAtomicPref3, 3);
  root_dict.Set("untracked", 4);
  base::Value* dict_value = root_dict.Set(kSplitPref, base::Value::Dict());
  dict_value->GetDict().Set("a", true);

  // Only update kAtomicPref, kAtomicPref3, and kSplitPref.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  pref_hash_filter_->FilterUpdate(kAtomicPref3);
  pref_hash_filter_->FilterUpdate(kSplitPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // Update kAtomicPref3 again, nothing should be stored still.
  base::Value* int_value5 = root_dict.Set(kAtomicPref3, 5);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // On FilterSerializeData, only kAtomicPref, kAtomicPref3, and kSplitPref
  // should get a new hash.
  pref_hash_filter_->FilterSerializeData(root_dict);
  ASSERT_EQ(3u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value_atomic1 =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(int_value1, stored_value_atomic1.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_value_atomic1.second);
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  MockPrefHashStore::ValuePtrStrategyPair stored_value_atomic3 =
      mock_pref_hash_store_->stored_value(kAtomicPref3);
  ASSERT_EQ(int_value5, stored_value_atomic3.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_value_atomic3.second);

  MockPrefHashStore::ValuePtrStrategyPair stored_value_split =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(dict_value, stored_value_split.first);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_value_split.second);
}

// TODO(https://crbug.com/1401148): Reenable.
TEST_P(PrefHashFilterTest, DISABLED_UnknownNullValue) {
  ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));
  // nullptr values are always trusted by the PrefHashStore.
  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        ValueState::TRUSTED_NULL_VALUE);
  mock_pref_hash_store_->SetCheckResult(kSplitPref,
                                        ValueState::TRUSTED_NULL_VALUE);
  DoFilterOnLoad(false);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_FALSE(stored_atomic_value.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value.second);

  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_FALSE(stored_split_value.first);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value.second);

  // Delegate saw all prefs, two of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(2u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::TRUSTED_NULL_VALUE));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  const MockValidationDelegateRecord::ValidationEvent* validated_split_pref =
      mock_validation_delegate_record_->GetEventForPath(kSplitPref);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, validated_split_pref->strategy);
  ASSERT_FALSE(validated_split_pref->is_personal);
  const MockValidationDelegateRecord::ValidationEvent* validated_atomic_pref =
      mock_validation_delegate_record_->GetEventForPath(kAtomicPref);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, validated_atomic_pref->strategy);
  ASSERT_TRUE(validated_atomic_pref->is_personal);
}

// TODO(https://crbug.com/1401148): Reenable.
TEST_P(PrefHashFilterTest, DISABLED_InitialValueUnknown) {
  base::Value* string_value =
      pref_store_contents_.Set(kAtomicPref, "string value");

  base::Value* value =
      pref_store_contents_.Set(kSplitPref, base::Value::Dict());
  ASSERT_TRUE(value->is_dict());

  base::Value::Dict& dict_value = value->GetDict();

  dict_value.Set("a", "foo");
  dict_value.Set("b", 1234);

  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_TRUE(pref_store_contents_.contains(kSplitPref));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        ValueState::UNTRUSTED_UNKNOWN_VALUE);
  mock_pref_hash_store_->SetCheckResult(kSplitPref,
                                        ValueState::UNTRUSTED_UNKNOWN_VALUE);
  // If we are enforcing, expect this to report changes.
  DoFilterOnLoad(GetParam() >= EnforcementLevel::ENFORCE_ON_LOAD);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  // Delegate saw all prefs, two of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(2u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::UNTRUSTED_UNKNOWN_VALUE));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value.second);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value.second);
  if (GetParam() == EnforcementLevel::ENFORCE_ON_LOAD) {
    // Ensure the prefs were cleared and the hashes for nullptr were restored if
    // the current enforcement level denies seeding.
    ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
    ASSERT_FALSE(stored_atomic_value.first);

    ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));
    ASSERT_FALSE(stored_split_value.first);

    VerifyRecordedReset(true);
  } else {
    // Otherwise the values should have remained intact and the hashes should
    // have been updated to match them.
    const base::Value* atomic_value_in_store =
        pref_store_contents_.Find(kAtomicPref);
    ASSERT_TRUE(atomic_value_in_store);
    ASSERT_EQ(string_value, atomic_value_in_store);
    ASSERT_EQ(string_value, stored_atomic_value.first);

    const base::Value* split_value_in_store =
        pref_store_contents_.Find(kSplitPref);
    ASSERT_TRUE(split_value_in_store);
    ASSERT_EQ(value, split_value_in_store);
    ASSERT_EQ(value, stored_split_value.first);

    VerifyRecordedReset(false);
  }
}

// TODO(https://crbug.com/1401148): Reenable.
TEST_P(PrefHashFilterTest, DISABLED_InitialValueTrustedUnknown) {
  base::Value* string_value = pref_store_contents_.Set(kAtomicPref, "test");

  auto* value = pref_store_contents_.Set(kSplitPref, base::Value::Dict());
  ASSERT_TRUE(value->is_dict());

  auto& dict_value = value->GetDict();
  dict_value.Set("a", "foo");
  dict_value.Set("b", 1234);

  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_TRUE(pref_store_contents_.contains(kSplitPref));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        ValueState::TRUSTED_UNKNOWN_VALUE);
  mock_pref_hash_store_->SetCheckResult(kSplitPref,
                                        ValueState::TRUSTED_UNKNOWN_VALUE);
  DoFilterOnLoad(false);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  // Delegate saw all prefs, two of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(2u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::TRUSTED_UNKNOWN_VALUE));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  // Seeding is always allowed for trusted unknown values.
  const base::Value* atomic_value_in_store =
      pref_store_contents_.Find(kAtomicPref);
  ASSERT_TRUE(atomic_value_in_store);
  ASSERT_EQ(string_value, atomic_value_in_store);
  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(string_value, stored_atomic_value.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value.second);

  const base::Value* split_value_in_store =
      pref_store_contents_.Find(kSplitPref);
  ASSERT_TRUE(split_value_in_store);
  ASSERT_EQ(value, split_value_in_store);
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(value, stored_split_value.first);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value.second);
}

TEST_P(PrefHashFilterTest, InitialValueChanged) {
  base::Value* int_value = pref_store_contents_.Set(kAtomicPref, 1234);

  base::Value* value =
      pref_store_contents_.Set(kSplitPref, base::Value::Dict());
  ASSERT_TRUE(value->is_dict());

  base::Value::Dict& dict_value = value->GetDict();
  dict_value.Set("a", "foo");
  dict_value.Set("b", 1234);
  dict_value.Set("c", 56);
  dict_value.Set("d", false);

  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_TRUE(pref_store_contents_.contains(kSplitPref));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::CHANGED);

  std::vector<std::string> mock_invalid_keys;
  mock_invalid_keys.push_back("a");
  mock_invalid_keys.push_back("c");
  mock_pref_hash_store_->SetInvalidKeysResult(kSplitPref, mock_invalid_keys);

  DoFilterOnLoad(GetParam() >= EnforcementLevel::ENFORCE_ON_LOAD);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value.second);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value.second);
  if (GetParam() == EnforcementLevel::ENFORCE_ON_LOAD) {
    // Ensure the atomic pref was cleared and the hash for nullptr was restored
    // if the current enforcement level prevents changes.
    ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
    ASSERT_FALSE(stored_atomic_value.first);

    // The split pref on the other hand should only have been stripped of its
    // invalid keys.
    const base::Value* split_value_in_store =
        pref_store_contents_.Find(kSplitPref);
    ASSERT_TRUE(split_value_in_store);
    ASSERT_EQ(2U, dict_value.size());
    ASSERT_FALSE(dict_value.contains("a"));
    ASSERT_TRUE(dict_value.contains("b"));
    ASSERT_FALSE(dict_value.contains("c"));
    ASSERT_TRUE(dict_value.contains("d"));
    ASSERT_EQ(value, stored_split_value.first);

    VerifyRecordedReset(true);
  } else {
    // Otherwise the value should have remained intact and the hash should have
    // been updated to match it.
    const base::Value* atomic_value_in_store =
        pref_store_contents_.Find(kAtomicPref);
    ASSERT_TRUE(atomic_value_in_store);
    ASSERT_EQ(int_value, atomic_value_in_store);
    ASSERT_EQ(int_value, stored_atomic_value.first);

    const base::Value* split_value_in_store =
        pref_store_contents_.Find(kSplitPref);
    ASSERT_TRUE(split_value_in_store);
    ASSERT_EQ(value, split_value_in_store);
    ASSERT_EQ(4U, dict_value.size());
    ASSERT_TRUE(dict_value.contains("a"));
    ASSERT_TRUE(dict_value.contains("b"));
    ASSERT_TRUE(dict_value.contains("c"));
    ASSERT_TRUE(dict_value.contains("d"));
    ASSERT_EQ(value, stored_split_value.first);

    VerifyRecordedReset(false);
  }
}

// TODO(https://crbug.com/1401148): Reenable.
TEST_P(PrefHashFilterTest, DISABLED_EmptyCleared) {
  ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));
  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::CLEARED);
  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::CLEARED);
  DoFilterOnLoad(false);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  // Delegate saw all prefs, two of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(2u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::CLEARED));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  // Regardless of the enforcement level, the only thing that should be done is
  // to restore the hash for nullptr. The value itself should still be nullptr.
  ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_FALSE(stored_atomic_value.first);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value.second);

  ASSERT_FALSE(pref_store_contents_.contains(kSplitPref));
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_FALSE(stored_split_value.first);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value.second);
}

// TODO(https://crbug.com/1401148): Reenable.
TEST_P(PrefHashFilterTest, DISABLED_InitialValueUnchangedLegacyId) {
  base::Value* string_value =
      pref_store_contents_.Set(kAtomicPref, "string value");

  base::Value* value =
      pref_store_contents_.Set(kSplitPref, base::Value::Dict());
  ASSERT_TRUE(value->is_dict());

  base::Value::Dict& dict_value = value->GetDict();
  dict_value.Set("a", "foo");
  dict_value.Set("b", 1234);

  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_TRUE(pref_store_contents_.contains(kSplitPref));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::SECURE_LEGACY);
  mock_pref_hash_store_->SetCheckResult(kSplitPref, ValueState::SECURE_LEGACY);
  DoFilterOnLoad(false);
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  // Delegate saw all prefs, two of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(2u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::SECURE_LEGACY));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  // Ensure that both the atomic and split hashes were restored.
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());

  // In all cases, the values should have remained intact and the hashes should
  // have been updated to match them.

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(PrefTrackingStrategy::ATOMIC, stored_atomic_value.second);
  const base::Value* atomic_value_in_store =
      pref_store_contents_.Find(kAtomicPref);
  ASSERT_TRUE(atomic_value_in_store);
  ASSERT_EQ(string_value, atomic_value_in_store);
  ASSERT_EQ(string_value, stored_atomic_value.first);

  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
      mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(PrefTrackingStrategy::SPLIT, stored_split_value.second);
  const base::Value* split_value_in_store =
      pref_store_contents_.Find(kSplitPref);
  ASSERT_TRUE(split_value_in_store);
  ASSERT_EQ(value, split_value_in_store);
  ASSERT_EQ(value, stored_split_value.first);

  VerifyRecordedReset(false);
}

// TODO(https://crbug.com/1401148): Reenable.
TEST_P(PrefHashFilterTest, DISABLED_DontResetReportOnly) {
  base::Value* int_value1 = pref_store_contents_.Set(kAtomicPref, 1);
  base::Value* int_value2 = pref_store_contents_.Set(kAtomicPref2, 2);
  base::Value* report_only_val = pref_store_contents_.Set(kReportOnlyPref, 3);
  base::Value* report_only_split_val =
      pref_store_contents_.Set(kReportOnlySplitPref, base::Value::Dict());
  ASSERT_TRUE(report_only_split_val->is_dict());
  report_only_split_val->GetDict().Set("a", 1234);

  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref));
  ASSERT_TRUE(pref_store_contents_.contains(kAtomicPref2));
  ASSERT_TRUE(pref_store_contents_.contains(kReportOnlyPref));
  ASSERT_TRUE(pref_store_contents_.contains(kReportOnlySplitPref));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref, ValueState::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kAtomicPref2, ValueState::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kReportOnlyPref, ValueState::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kReportOnlySplitPref,
                                        ValueState::CHANGED);

  DoFilterOnLoad(GetParam() >= EnforcementLevel::ENFORCE_ON_LOAD);
  // All prefs should be checked and a new hash should be stored for each tested
  // pref.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(4u, mock_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->transactions_performed());

  // Delegate saw all prefs, four of which had the expected value_state.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());
  ASSERT_EQ(4u, mock_validation_delegate_record_->CountValidationsOfState(
                    ValueState::CHANGED));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 4u,
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  // No matter what the enforcement level is, the report only pref should never
  // be reset.
  ASSERT_TRUE(pref_store_contents_.contains(kReportOnlyPref));
  ASSERT_TRUE(pref_store_contents_.contains(kReportOnlySplitPref));
  ASSERT_EQ(report_only_val,
            mock_pref_hash_store_->stored_value(kReportOnlyPref).first);
  ASSERT_EQ(report_only_split_val,
            mock_pref_hash_store_->stored_value(kReportOnlySplitPref).first);

  // All other prefs should have been reset if the enforcement level allows it.
  if (GetParam() == EnforcementLevel::ENFORCE_ON_LOAD) {
    ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref));
    ASSERT_FALSE(pref_store_contents_.contains(kAtomicPref2));
    ASSERT_FALSE(mock_pref_hash_store_->stored_value(kAtomicPref).first);
    ASSERT_FALSE(mock_pref_hash_store_->stored_value(kAtomicPref2).first);

    VerifyRecordedReset(true);
  } else {
    const base::Value* value_in_store = pref_store_contents_.Find(kAtomicPref);
    const base::Value* value_in_store2 =
        pref_store_contents_.Find(kAtomicPref2);
    ASSERT_TRUE(value_in_store);
    ASSERT_TRUE(value_in_store2);
    ASSERT_EQ(int_value1, value_in_store);
    ASSERT_EQ(int_value1,
              mock_pref_hash_store_->stored_value(kAtomicPref).first);
    ASSERT_EQ(int_value2, value_in_store2);
    ASSERT_EQ(int_value2,
              mock_pref_hash_store_->stored_value(kAtomicPref2).first);

    VerifyRecordedReset(false);
  }
}

// TODO(https://crbug.com/1401148): Reenable.
TEST_P(PrefHashFilterTest, DISABLED_CallFilterSerializeDataCallbacks) {
  base::Value::Dict root_dict;
  base::Value::Dict dict_value;
  dict_value.Set("a", true);
  root_dict.Set(kAtomicPref, 1);
  root_dict.Set(kAtomicPref2, 2);
  root_dict.Set(kSplitPref, std::move(dict_value));

  // Skip updating kAtomicPref2.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  pref_hash_filter_->FilterUpdate(kSplitPref);

  PrefHashFilter::OnWriteCallbackPair callbacks =
      pref_hash_filter_->FilterSerializeData(root_dict);

  ASSERT_FALSE(callbacks.first.is_null());

  // Prefs should be cleared from the external validation store only once the
  // before-write callback is run.
  ASSERT_EQ(
      0u, mock_external_validation_hash_store_contents_->cleared_paths_count());
  std::move(callbacks.first).Run();
  ASSERT_EQ(
      2u, mock_external_validation_hash_store_contents_->cleared_paths_count());

  // No pref write should occur before the after-write callback is run.
  ASSERT_EQ(
      0u, mock_external_validation_hash_store_contents_->stored_hashes_count());

  std::move(callbacks.second).Run(true);

  ASSERT_EQ(
      2u, mock_external_validation_hash_store_contents_->stored_hashes_count());
  ASSERT_EQ(
      "atomic mac for: atomic_pref",
      mock_external_validation_hash_store_contents_->GetStoredMac(kAtomicPref));
  ASSERT_EQ("split mac for: split_pref/a",
            mock_external_validation_hash_store_contents_->GetStoredSplitMac(
                kSplitPref, "a"));

  // The callbacks should write directly to the contents without going through
  // a pref hash store.
  ASSERT_EQ(0u,
            mock_external_validation_pref_hash_store_->stored_paths_count());
}

TEST_P(PrefHashFilterTest, CallFilterSerializeDataCallbacksWithFailure) {
  base::Value::Dict root_dict;
  root_dict.Set(kAtomicPref, 1);

  // Only update kAtomicPref.
  pref_hash_filter_->FilterUpdate(kAtomicPref);

  PrefHashFilter::OnWriteCallbackPair callbacks =
      pref_hash_filter_->FilterSerializeData(root_dict);

  ASSERT_FALSE(callbacks.first.is_null());

  std::move(callbacks.first).Run();

  // The pref should have been cleared from the external validation store.
  ASSERT_EQ(
      1u, mock_external_validation_hash_store_contents_->cleared_paths_count());

  std::move(callbacks.second).Run(false);

  // Expect no writes to the external validation hash store contents.
  ASSERT_EQ(0u,
            mock_external_validation_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(
      0u, mock_external_validation_hash_store_contents_->stored_hashes_count());
}

// TODO(https://crbug.com/1401148): Reenable.
TEST_P(PrefHashFilterTest, DISABLED_ExternalValidationValueChanged) {
  pref_store_contents_.Set(kAtomicPref, 1234);

  base::Value::Dict dict_value;
  dict_value.Set("a", "foo");
  dict_value.Set("b", 1234);
  dict_value.Set("c", 56);
  dict_value.Set("d", false);
  pref_store_contents_.Set(kSplitPref, std::move(dict_value));

  mock_external_validation_pref_hash_store_->SetCheckResult(
      kAtomicPref, ValueState::CHANGED);
  mock_external_validation_pref_hash_store_->SetCheckResult(
      kSplitPref, ValueState::CHANGED);

  std::vector<std::string> mock_invalid_keys;
  mock_invalid_keys.push_back("a");
  mock_invalid_keys.push_back("c");
  mock_external_validation_pref_hash_store_->SetInvalidKeysResult(
      kSplitPref, mock_invalid_keys);

  DoFilterOnLoad(false);

  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_external_validation_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u,
            mock_external_validation_pref_hash_store_->stored_paths_count());
  ASSERT_EQ(
      1u, mock_external_validation_pref_hash_store_->transactions_performed());

  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->recorded_validations_count());

  // Regular validation should not have any CHANGED prefs.
  ASSERT_EQ(std::size(kTestTrackedPrefs),
            mock_validation_delegate_record_->CountValidationsOfState(
                ValueState::UNCHANGED));

  // External validation should have two CHANGED prefs (kAtomic and kSplit).
  ASSERT_EQ(2u,
            mock_validation_delegate_record_->CountExternalValidationsOfState(
                ValueState::CHANGED));
  ASSERT_EQ(std::size(kTestTrackedPrefs) - 2u,
            mock_validation_delegate_record_->CountExternalValidationsOfState(
                ValueState::UNCHANGED));
}

TEST_P(PrefHashFilterTest, CleanupDeprecatedTrackedDictionary) {
  // Fake a preference value and stored hash from an old version of Chrome.
  base::Value pref_value(1234);
  pref_store_contents_.SetByDottedPath(kDeprecatedTrackedDictionaryEntry, 1234);
  {
    std::unique_ptr<PrefHashStoreTransaction> transaction(
        mock_pref_hash_store_->BeginTransaction(nullptr));
    transaction->StoreHash(kDeprecatedTrackedDictionaryEntry, &pref_value);
  }

  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());

  std::vector<const char*> test_deprecated_prefs{
      kDeprecatedTrackedDictionaryEntry,
      kDeprecatedUntrackedDictionary,
  };
  PrefHashFilter::SetDeprecatedPrefsForTesting(test_deprecated_prefs);

  DoFilterOnLoad(false);

  EXPECT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
  EXPECT_FALSE(pref_store_contents_.contains("dictionary.pref"));
  EXPECT_FALSE(pref_store_contents_.contains("dictionary"));
}

INSTANTIATE_TEST_SUITE_P(PrefHashFilterTestInstance,
                         PrefHashFilterTest,
                         testing::Values(EnforcementLevel::NO_ENFORCEMENT,
                                         EnforcementLevel::ENFORCE_ON_LOAD));
