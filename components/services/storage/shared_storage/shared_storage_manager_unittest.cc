// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_manager.h"

#include <queue>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/async_shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "components/services/storage/shared_storage/shared_storage_test_utils.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using InitStatus = SharedStorageDatabase::InitStatus;
using SetBehavior = SharedStorageDatabase::SetBehavior;
using OperationResult = SharedStorageDatabase::OperationResult;
using GetResult = SharedStorageDatabase::GetResult;
using BudgetResult = SharedStorageDatabase::BudgetResult;
using TimeResult = SharedStorageDatabase::TimeResult;
using MetadataResult = SharedStorageDatabase::MetadataResult;
using EntriesResult = SharedStorageDatabase::EntriesResult;
using StorageKeyPolicyMatcherFunction =
    SharedStorageDatabase::StorageKeyPolicyMatcherFunction;
using DBOperation = TestDatabaseOperationReceiver::DBOperation;
using Type = DBOperation::Type;
using DBType = SharedStorageTestDBType;
using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

const int kBitBudget = 8;
const int kInitialPurgeIntervalHours = 4;
const int kRecurringPurgeIntervalHours = 2;
const int kThresholdHours = 7;

class MockResultQueue {
 public:
  explicit MockResultQueue(std::queue<OperationResult> result_queue)
      : result_queue_(std::move(result_queue)) {}
  ~MockResultQueue() = default;

  void SetResultQueue(std::queue<OperationResult> result_queue) {
    result_queue_ = std::move(result_queue);
  }

  void DoNothing() { base::DoNothing(); }

  OperationResult NextOperationResult() {
    DCHECK(!result_queue_.empty());
    OperationResult next_result = result_queue_.front();
    result_queue_.pop();
    return next_result;
  }

  GetResult NextGetResult() {
    DCHECK(!result_queue_.empty());
    GetResult next_result;
    next_result.result = result_queue_.front();
    result_queue_.pop();
    return next_result;
  }

  BudgetResult NextBudgetResult() {
    DCHECK(!result_queue_.empty());
    BudgetResult next_result = MakeBudgetResultForSqlError();
    next_result.result = result_queue_.front();
    result_queue_.pop();
    return next_result;
  }

  TimeResult NextTimeResult() {
    DCHECK(!result_queue_.empty());
    TimeResult next_result;
    next_result.result = result_queue_.front();
    result_queue_.pop();
    return next_result;
  }

  bool NextBool() {
    DCHECK(!result_queue_.empty());
    bool next_success = (static_cast<int>(result_queue_.front()) < 3);
    result_queue_.pop();
    return next_success;
  }

  int NextInt() {
    DCHECK(!result_queue_.empty());
    int next_length = (static_cast<int>(result_queue_.front()) < 3) ? 0 : -1;
    result_queue_.pop();
    return next_length;
  }

  std::vector<mojom::StorageUsageInfoPtr> NextInfos() {
    DCHECK(!result_queue_.empty());
    result_queue_.pop();
    return std::vector<mojom::StorageUsageInfoPtr>();
  }

  MetadataResult NextMetadata() {
    DCHECK(!result_queue_.empty());
    MetadataResult metadata;
    metadata.time_result = result_queue_.front();
    metadata.budget_result = result_queue_.front();
    result_queue_.pop();
    return metadata;
  }

  EntriesResult NextEntries() {
    DCHECK(!result_queue_.empty());

    EntriesResult entries;
    entries.result = result_queue_.front();
    result_queue_.pop();
    return entries;
  }

 private:
  std::queue<OperationResult> result_queue_;
};

class MockAsyncSharedStorageDatabase : public AsyncSharedStorageDatabase {
 public:
  static std::unique_ptr<AsyncSharedStorageDatabase> Create() {
    return base::WrapUnique(new MockAsyncSharedStorageDatabase());
  }

  static std::unique_ptr<AsyncSharedStorageDatabase> Create(
      std::queue<OperationResult> result_queue) {
    return base::WrapUnique(
        new MockAsyncSharedStorageDatabase(std::move(result_queue)));
  }

  ~MockAsyncSharedStorageDatabase() override = default;

  // AsyncSharedStorageDatabase
  void Destroy(base::OnceCallback<void(bool)> callback) override {
    Run(std::move(callback));
  }
  void TrimMemory(base::OnceClosure callback) override {
    Run(std::move(callback));
  }
  void Get(url::Origin context_origin,
           std::u16string key,
           base::OnceCallback<void(GetResult)> callback) override {
    Run(std::move(callback));
  }
  void Set(url::Origin context_origin,
           std::u16string key,
           std::u16string value,
           base::OnceCallback<void(OperationResult)> callback,
           SetBehavior behavior = SetBehavior::kDefault) override {
    Run(std::move(callback));
  }
  void Append(url::Origin context_origin,
              std::u16string key,
              std::u16string value,
              base::OnceCallback<void(OperationResult)> callback) override {
    Run(std::move(callback));
  }
  void Delete(url::Origin context_origin,
              std::u16string key,
              base::OnceCallback<void(OperationResult)> callback) override {
    Run(std::move(callback));
  }
  void Clear(url::Origin context_origin,
             base::OnceCallback<void(OperationResult)> callback) override {
    Run(std::move(callback));
  }
  void Length(url::Origin context_origin,
              base::OnceCallback<void(int)> callback) override {
    Run(std::move(callback));
  }
  void Keys(url::Origin context_origin,
            mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
                pending_listener,
            base::OnceCallback<void(OperationResult)> callback) override {
    Run(std::move(callback));
  }
  void Entries(url::Origin context_origin,
               mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
                   pending_listener,
               base::OnceCallback<void(OperationResult)> callback) override {
    Run(std::move(callback));
  }
  void PurgeMatchingOrigins(StorageKeyPolicyMatcherFunction storage_key_matcher,
                            base::Time begin,
                            base::Time end,
                            base::OnceCallback<void(OperationResult)> callback,
                            bool perform_storage_cleanup = false) override {
    Run(std::move(callback));
  }
  void PurgeStale(base::OnceCallback<void(OperationResult)> callback) override {
    Run(std::move(callback));
  }
  void FetchOrigins(
      base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
          callback) override {
    Run(std::move(callback));
  }
  void MakeBudgetWithdrawal(
      url::Origin context_origin,
      double bits_debit,
      base::OnceCallback<void(OperationResult)> callback) override {
    Run(std::move(callback));
  }
  void GetRemainingBudget(
      url::Origin context_origin,
      base::OnceCallback<void(BudgetResult)> callback) override {
    Run(std::move(callback));
  }
  void GetCreationTime(url::Origin context_origin,
                       base::OnceCallback<void(TimeResult)> callback) override {
    Run(std::move(callback));
  }
  void GetMetadata(url::Origin context_origin,
                   base::OnceCallback<void(MetadataResult)> callback) override {
    Run(std::move(callback));
  }
  void GetEntriesForDevTools(
      url::Origin context_origin,
      base::OnceCallback<void(EntriesResult)> callback) override {
    Run(std::move(callback));
  }
  void ResetBudgetForDevTools(
      url::Origin context_origin,
      base::OnceCallback<void(OperationResult)> callback) override {
    Run(std::move(callback));
  }

  void SetResultsForTesting(std::queue<OperationResult> result_queue,
                            base::OnceClosure callback) {
    mock_result_queue_.AsyncCall(&MockResultQueue::SetResultQueue)
        .WithArgs(std::move(result_queue))
        .Then(std::move(callback));
  }

 private:
  MockAsyncSharedStorageDatabase()
      : MockAsyncSharedStorageDatabase(std::queue<OperationResult>()) {}

  explicit MockAsyncSharedStorageDatabase(
      std::queue<OperationResult> result_queue)
      : mock_result_queue_(
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::WithBaseSyncPrimitives(),
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
            std::move(result_queue)) {}

  void Run(base::OnceClosure callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::DoNothing)
        .Then(std::move(callback));
  }

  void Run(base::OnceCallback<void(bool)> callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::NextBool)
        .Then(std::move(callback));
  }

  void Run(base::OnceCallback<void(OperationResult)> callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::NextOperationResult)
        .Then(std::move(callback));
  }

  void Run(base::OnceCallback<void(GetResult)> callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::NextGetResult)
        .Then(std::move(callback));
  }

  void Run(base::OnceCallback<void(BudgetResult)> callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::NextBudgetResult)
        .Then(std::move(callback));
  }

  void Run(base::OnceCallback<void(TimeResult)> callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::NextTimeResult)
        .Then(std::move(callback));
  }

  void Run(base::OnceCallback<void(int)> callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::NextInt)
        .Then(std::move(callback));
  }

  void Run(base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
               callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::NextInfos)
        .Then(std::move(callback));
  }

  void Run(base::OnceCallback<void(MetadataResult)> callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::NextMetadata)
        .Then(std::move(callback));
  }

  void Run(base::OnceCallback<void(EntriesResult)> callback) {
    DCHECK(callback);
    mock_result_queue_.AsyncCall(&MockResultQueue::NextEntries)
        .Then(std::move(callback));
  }

  base::SequenceBound<MockResultQueue> mock_result_queue_;
};

class MockSharedStorageManager : public SharedStorageManager {
 public:
  MockSharedStorageManager(
      base::FilePath db_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<SharedStorageOptions> options)
      : SharedStorageManager(db_path,
                             std::move(special_storage_policy),
                             std::move(options)) {
    OverrideDatabaseForTesting(MockAsyncSharedStorageDatabase::Create());
  }

  ~MockSharedStorageManager() override = default;

  void SetResultsForTesting(std::queue<OperationResult> result_queue,
                            base::OnceClosure callback) {
    DCHECK(database());
    static_cast<MockAsyncSharedStorageDatabase*>(database())
        ->SetResultsForTesting(std::move(result_queue), std::move(callback));
  }
};

}  // namespace

class SharedStorageManagerTest : public testing::Test {
 public:
  SharedStorageManagerTest()
      : special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        receiver_(std::make_unique<TestDatabaseOperationReceiver>()) {}

  ~SharedStorageManagerTest() override = default;

  virtual SharedStorageManager* GetManager() { return manager_.get(); }

  void SetUp() override {
    InitSharedStorageFeature();

    if (GetType() != DBType::kInMemory)
      PrepareFileBacked();
    else
      EXPECT_TRUE(db_path_.empty());

    CreateManager();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();

    if (GetType() != DBType::kInMemory) {
      ResetManager();
      task_environment_.RunUntilIdle();
      EXPECT_TRUE(temp_dir_.Delete());
    }
  }

  virtual void InitSharedStorageFeature() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        // Set these intervals to be long enough not to interfere with the
        // basic tests.
        {{"SharedStorageStalePurgeInitialInterval",
          TimeDeltaToString(base::Hours(kInitialPurgeIntervalHours))},
         {"SharedStorageStalePurgeRecurringInterval",
          TimeDeltaToString(base::Hours(kRecurringPurgeIntervalHours))},
         {"SharedStorageStalenessThreshold",
          TimeDeltaToString(base::Hours(kThresholdHours))},
         {"SharedStorageBitBudget", base::NumberToString(kBitBudget)},
         {"SharedStorageBudgetInterval",
          TimeDeltaToString(base::Hours(kBudgetIntervalHours_))}});
  }

  // Return the relative file path in the "storage/" subdirectory of test data
  // for the SQL file from which to initialize an async shared storage database
  // instance.
  virtual std::string GetRelativeFilePath() { return nullptr; }

  virtual DBType GetType() { return DBType::kInMemory; }

  virtual void CreateManager() {
    manager_ = std::make_unique<SharedStorageManager>(
        db_path_, special_storage_policy_, SharedStorageOptions::Create());
  }

  virtual void ResetManager() {
    if (manager_)
      manager_.reset();
  }

  void PrepareFileBacked() {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestSharedStorage.db");
    if (GetType() == DBType::kFileBackedFromExisting)
      ASSERT_TRUE(CreateDatabaseFromSQL(db_path_, GetRelativeFilePath()));
    EXPECT_FALSE(db_path_.empty());
  }

  bool is_finished() const {
    DCHECK(receiver_);
    return receiver_->is_finished();
  }

  void SetExpectedOperationList(std::queue<DBOperation> expected_operations) {
    DCHECK(receiver_);
    receiver_->set_expected_operations(std::move(expected_operations));
  }

  void WaitForOperations() {
    DCHECK(receiver_);
    receiver_->WaitForOperations();
  }

  void SetDestroyCallback() {
    DCHECK(GetManager());
    destroy_loop_ = std::make_unique<base::RunLoop>();
    destroy_success_ = false;
    GetManager()->SetOnDBDestroyedCallbackForTesting(base::BindOnce(
        [](base::OnceClosure callback, bool* out_success, bool success) {
          DCHECK(out_success);
          *out_success = success;
          std::move(callback).Run();
        },
        destroy_loop_->QuitClosure(), &destroy_success_));
  }

  void VerifyDestroyAndRecreateDatabaseSync() {
    destroy_loop_->Run();
    EXPECT_TRUE(destroy_success_);
    EXPECT_TRUE(GetManager()->tried_to_recover_from_init_failure_for_testing());
    EXPECT_TRUE(GetManager()->database());
  }

  void OnMemoryPressure(MemoryPressureLevel memory_pressure_level) {
    DCHECK(GetManager());
    DCHECK(receiver_);

    auto callback = receiver_->MakeOnceClosureFromClosure(
        DBOperation(
            Type::DB_ON_MEMORY_PRESSURE,
            {TestDatabaseOperationReceiver::SerializeMemoryPressureLevel(
                memory_pressure_level)}),
        base::BindLambdaForTesting([&]() { memory_trimmed_ = true; }));
    GetManager()->OnMemoryPressure(std::move(callback), memory_pressure_level);
  }

  void Get(url::Origin context_origin,
           std::u16string key,
           GetResult* out_value) {
    DCHECK(out_value);
    DCHECK(GetManager());
    DCHECK(receiver_);

    auto callback = receiver_->MakeGetResultCallback(
        DBOperation(Type::DB_GET, context_origin, {key}), out_value);
    GetManager()->Get(std::move(context_origin), std::move(key),
                      std::move(callback));
  }

  GetResult GetSync(url::Origin context_origin, std::u16string key) {
    DCHECK(GetManager());
    base::test::TestFuture<GetResult> future;
    GetManager()->Get(std::move(context_origin), std::move(key),
                      future.GetCallback());
    return future.Take();
  }

  void Set(url::Origin context_origin,
           std::u16string key,
           std::u16string value,
           OperationResult* out_result,
           SetBehavior behavior = SetBehavior::kDefault) {
    DCHECK(out_result);
    DCHECK(GetManager());
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(
            Type::DB_SET, context_origin,
            {key, value,
             TestDatabaseOperationReceiver::SerializeSetBehavior(behavior)}),
        out_result);
    GetManager()->Set(std::move(context_origin), std::move(key),
                      std::move(value), std::move(callback), behavior);
  }

  OperationResult SetSync(url::Origin context_origin,
                          std::u16string key,
                          std::u16string value,
                          SetBehavior behavior = SetBehavior::kDefault) {
    DCHECK(GetManager());
    base::test::TestFuture<OperationResult> future;
    GetManager()->Set(std::move(context_origin), std::move(key),
                      std::move(value), future.GetCallback(), behavior);
    return future.Get();
  }

  void Append(url::Origin context_origin,
              std::u16string key,
              std::u16string value,
              OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(GetManager());
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_APPEND, context_origin, {key, value}), out_result);
    GetManager()->Append(std::move(context_origin), std::move(key),
                         std::move(value), std::move(callback));
  }

  OperationResult AppendSync(url::Origin context_origin,
                             std::u16string key,
                             std::u16string value) {
    DCHECK(GetManager());
    base::test::TestFuture<OperationResult> future;
    GetManager()->Append(std::move(context_origin), std::move(key),
                         std::move(value), future.GetCallback());
    return future.Get();
  }

  void Delete(url::Origin context_origin,
              std::u16string key,
              OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(GetManager());
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_DELETE, context_origin, {key}), out_result);
    GetManager()->Delete(std::move(context_origin), std::move(key),
                         std::move(callback));
  }

  OperationResult DeleteSync(url::Origin context_origin, std::u16string key) {
    DCHECK(GetManager());
    base::test::TestFuture<OperationResult> future;
    GetManager()->Delete(std::move(context_origin), std::move(key),
                         future.GetCallback());
    return future.Get();
  }

  void Length(url::Origin context_origin, int* out_length) {
    DCHECK(out_length);
    DCHECK(GetManager());
    DCHECK(receiver_);

    *out_length = -1;
    auto callback = receiver_->MakeIntCallback(
        DBOperation(Type::DB_LENGTH, context_origin), out_length);
    GetManager()->Length(std::move(context_origin), std::move(callback));
  }

  int LengthSync(url::Origin context_origin) {
    DCHECK(GetManager());
    base::test::TestFuture<int> future;
    GetManager()->Length(std::move(context_origin), future.GetCallback());
    return future.Get();
  }

  void Keys(url::Origin context_origin,
            TestSharedStorageEntriesListenerUtility* listener_utility,
            int listener_id,
            OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(GetManager());
    DCHECK(receiver_);

    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_KEYS, context_origin,
                    {base::NumberToString16(listener_id)}),
        out_result);
    GetManager()->Keys(
        std::move(context_origin),
        listener_utility->BindNewPipeAndPassRemoteForId(listener_id),
        std::move(callback));
  }

  OperationResult KeysSync(
      url::Origin context_origin,
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          listener) {
    DCHECK(GetManager());
    base::test::TestFuture<OperationResult> future;
    GetManager()->Keys(std::move(context_origin), std::move(listener),
                       future.GetCallback());
    return future.Get();
  }

  void Entries(url::Origin context_origin,
               TestSharedStorageEntriesListenerUtility* listener_utility,
               int listener_id,
               OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(GetManager());
    DCHECK(receiver_);

    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_ENTRIES, context_origin,
                    {base::NumberToString16(listener_id)}),
        out_result);
    GetManager()->Entries(
        std::move(context_origin),
        listener_utility->BindNewPipeAndPassRemoteForId(listener_id),
        std::move(callback));
  }

  OperationResult EntriesSync(
      url::Origin context_origin,
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          listener) {
    DCHECK(GetManager());
    base::test::TestFuture<OperationResult> future;
    GetManager()->Entries(std::move(context_origin), std::move(listener),
                          future.GetCallback());
    return future.Get();
  }

  void Clear(url::Origin context_origin, OperationResult* out_result) {
    DCHECK(out_result);
    DCHECK(GetManager());
    DCHECK(receiver_);

    *out_result = OperationResult::kSqlError;
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_CLEAR, context_origin), out_result);
    GetManager()->Clear(std::move(context_origin), std::move(callback));
  }

  OperationResult ClearSync(url::Origin context_origin) {
    DCHECK(GetManager());
    base::test::TestFuture<OperationResult> future;
    GetManager()->Clear(std::move(context_origin), future.GetCallback());
    return future.Get();
  }

  void FetchOrigins(std::vector<mojom::StorageUsageInfoPtr>* out_result) {
    DCHECK(out_result);
    DCHECK(GetManager());
    DCHECK(receiver_);

    auto callback = receiver_->MakeInfosCallback(
        DBOperation(Type::DB_FETCH_ORIGINS), out_result);
    GetManager()->FetchOrigins(std::move(callback));
  }

  std::vector<mojom::StorageUsageInfoPtr> FetchOriginsSync() {
    DCHECK(GetManager());
    base::test::TestFuture<std::vector<mojom::StorageUsageInfoPtr>> future;
    GetManager()->FetchOrigins(future.GetCallback());
    return future.Take();
  }

  void PurgeMatchingOrigins(
      StorageKeyPolicyMatcherFunctionUtility* matcher_utility,
      size_t matcher_id,
      base::Time begin,
      base::Time end,
      OperationResult* out_result,
      bool perform_storage_cleanup = false) {
    DCHECK(out_result);
    DCHECK(GetManager());
    DCHECK(receiver_);
    DCHECK(matcher_utility);
    DCHECK(!(*matcher_utility).is_empty());
    DCHECK_LT(matcher_id, (*matcher_utility).size());

    std::vector<std::u16string> params(
        {base::NumberToString16(matcher_id),
         TestDatabaseOperationReceiver::SerializeTime(begin),
         TestDatabaseOperationReceiver::SerializeTime(end),
         TestDatabaseOperationReceiver::SerializeBool(
             perform_storage_cleanup)});
    auto callback = receiver_->MakeOperationResultCallback(
        DBOperation(Type::DB_PURGE_MATCHING, std::move(params)), out_result);
    GetManager()->PurgeMatchingOrigins(
        matcher_utility->TakeMatcherFunctionForId(matcher_id), begin, end,
        std::move(callback), perform_storage_cleanup);
  }

  OperationResult PurgeMatchingOriginsSync(
      StorageKeyPolicyMatcherFunction storage_key_matcher,
      base::Time begin,
      base::Time end,
      bool perform_storage_cleanup) {
    DCHECK(GetManager());
    base::test::TestFuture<OperationResult> future;
    GetManager()->PurgeMatchingOrigins(std::move(storage_key_matcher), begin,
                                       end, future.GetCallback(),
                                       perform_storage_cleanup);
    return future.Get();
  }

  OperationResult MakeBudgetWithdrawalSync(const url::Origin& context_origin,
                                           double bits_debit) {
    DCHECK(GetManager());

    base::test::TestFuture<OperationResult> future;
    GetManager()->MakeBudgetWithdrawal(std::move(context_origin), bits_debit,
                                       future.GetCallback());
    return future.Get();
  }

  BudgetResult GetRemainingBudgetSync(const url::Origin& context_origin) {
    DCHECK(GetManager());

    base::test::TestFuture<BudgetResult> future;
    GetManager()->GetRemainingBudget(std::move(context_origin),
                                     future.GetCallback());
    return future.Take();
  }

  void OverrideCreationTime(url::Origin context_origin,
                            base::Time new_creation_time,
                            bool* out_success) {
    DCHECK(out_success);
    DCHECK(GetManager());
    DCHECK(receiver_);

    auto callback = receiver_->MakeBoolCallback(
        DBOperation(
            Type::DB_OVERRIDE_TIME_ORIGIN, context_origin,
            {TestDatabaseOperationReceiver::SerializeTime(new_creation_time)}),
        out_success);
    GetManager()->OverrideCreationTimeForTesting(
        std::move(context_origin), new_creation_time, std::move(callback));
  }

  int GetNumBudgetEntriesSync(url::Origin context_origin) {
    DCHECK(GetManager());

    base::test::TestFuture<int> future;
    GetManager()->GetNumBudgetEntriesForTesting(std::move(context_origin),
                                                future.GetCallback());
    return future.Get();
  }

  int GetTotalNumBudgetEntriesSync() {
    DCHECK(GetManager());

    base::test::TestFuture<int> future;
    GetManager()->GetTotalNumBudgetEntriesForTesting(future.GetCallback());
    return future.Get();
  }

  TimeResult GetCreationTimeSync(const url::Origin& context_origin) {
    DCHECK(GetManager());

    base::test::TestFuture<TimeResult> future;
    GetManager()->GetCreationTime(std::move(context_origin),
                                  future.GetCallback());
    return future.Take();
  }

  MetadataResult GetMetadataSync(const url::Origin& context_origin) {
    DCHECK(GetManager());

    base::test::TestFuture<MetadataResult> future;
    GetManager()->GetMetadata(std::move(context_origin), future.GetCallback());
    return future.Take();
  }

  EntriesResult GetEntriesForDevToolsSync(const url::Origin& context_origin) {
    DCHECK(GetManager());

    base::test::TestFuture<EntriesResult> future;
    GetManager()->GetEntriesForDevTools(std::move(context_origin),
                                        future.GetCallback());
    return future.Take();
  }

  OperationResult ResetBudgetForDevToolsSync(
      const url::Origin& context_origin) {
    DCHECK(GetManager());

    base::test::TestFuture<OperationResult> future;
    GetManager()->ResetBudgetForDevTools(std::move(context_origin),
                                         future.GetCallback());
    return future.Get();
  }

 protected:
  static constexpr int kBudgetIntervalHours_ =
      kInitialPurgeIntervalHours + 2 * kRecurringPurgeIntervalHours;

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<SharedStorageManager> manager_;
  std::unique_ptr<TestDatabaseOperationReceiver> receiver_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<base::RunLoop> destroy_loop_;
  bool destroy_success_ = false;
  bool memory_trimmed_ = false;
};

class SharedStorageManagerFromFileTest : public SharedStorageManagerTest {
 public:
  DBType GetType() override { return DBType::kFileBackedFromExisting; }

  std::string GetRelativeFilePath() override {
    return GetTestFileNameForCurrentVersion();
  }
};

// Test loading current version database.
TEST_F(SharedStorageManagerFromFileTest, CurrentVersion_LoadFromFile) {
  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  EXPECT_EQ(GetSync(google_com, u"key1").data, u"value1");
  EXPECT_EQ(GetSync(google_com, u"key2").data, u"value2");

  url::Origin youtube_com = url::Origin::Create(GURL("http://youtube.com/"));
  EXPECT_EQ(1L, LengthSync(youtube_com));

  EntriesResult youtube_com_entries = GetEntriesForDevToolsSync(youtube_com);
  EXPECT_EQ(OperationResult::kSuccess, youtube_com_entries.result);
  EXPECT_THAT(youtube_com_entries.entries,
              ElementsAre(Pair("visited", "1111111")));

  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  EXPECT_EQ(GetSync(chromium_org, u"a").data, u"");

  TestSharedStorageEntriesListenerUtility listener_utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = listener_utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            KeysSync(chromium_org,
                     listener_utility.BindNewPipeAndPassRemoteForId(id1)));
  listener_utility.FlushForId(id1);
  EXPECT_THAT(listener_utility.TakeKeysForId(id1),
              ElementsAre(u"a", u"b", u"c"));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id1));
  listener_utility.VerifyNoErrorForId(id1);

  size_t id2 = listener_utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            EntriesSync(chromium_org,
                        listener_utility.BindNewPipeAndPassRemoteForId(id2)));
  listener_utility.FlushForId(id2);
  EXPECT_THAT(listener_utility.TakeEntriesForId(id2),
              ElementsAre(Pair(u"a", u""), Pair(u"b", u"hello"),
                          Pair(u"c", u"goodbye")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id2));
  listener_utility.VerifyNoErrorForId(id2);

  EntriesResult chromium_org_entries = GetEntriesForDevToolsSync(chromium_org);
  EXPECT_EQ(OperationResult::kSuccess, chromium_org_entries.result);
  EXPECT_THAT(
      chromium_org_entries.entries,
      ElementsAre(Pair("a", ""), Pair("b", "hello"), Pair("c", "goodbye")));

  url::Origin google_org = url::Origin::Create(GURL("http://google.org/"));
  EXPECT_EQ(
      GetSync(google_org, u"1").data,
      u"fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "fffffffffffffffff");
  EXPECT_EQ(GetSync(google_org,
                    u"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
                .data,
            u"k");

  url::Origin abc_xyz = url::Origin::Create(GURL("http://abc.xyz"));
  EXPECT_EQ(13269481776356965, GetCreationTimeSync(abc_xyz)
                                   .time.ToDeltaSinceWindowsEpoch()
                                   .InMicroseconds());
  auto abc_xyz_metadata = GetMetadataSync(abc_xyz);
  EXPECT_EQ(OperationResult::kSuccess, abc_xyz_metadata.time_result);
  EXPECT_EQ(OperationResult::kSuccess, abc_xyz_metadata.budget_result);
  EXPECT_EQ(13269481776356965,
            abc_xyz_metadata.creation_time.ToDeltaSinceWindowsEpoch()
                .InMicroseconds());
  EXPECT_EQ(2, abc_xyz_metadata.length);
  EXPECT_DOUBLE_EQ(kBitBudget - 5.3, abc_xyz_metadata.remaining_budget);

  url::Origin growwithgoogle_com =
      url::Origin::Create(GURL("http://growwithgoogle.com"));
  EXPECT_EQ(13269546593856733, GetCreationTimeSync(growwithgoogle_com)
                                   .time.ToDeltaSinceWindowsEpoch()
                                   .InMicroseconds());
  auto growwithgoogle_com_metadata = GetMetadataSync(growwithgoogle_com);
  EXPECT_EQ(OperationResult::kSuccess, growwithgoogle_com_metadata.time_result);
  EXPECT_EQ(OperationResult::kSuccess,
            growwithgoogle_com_metadata.budget_result);
  EXPECT_EQ(13269546593856733,
            growwithgoogle_com_metadata.creation_time.ToDeltaSinceWindowsEpoch()
                .InMicroseconds());
  EXPECT_EQ(3, growwithgoogle_com_metadata.length);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.2,
                   growwithgoogle_com_metadata.remaining_budget);

  std::vector<mojom::StorageUsageInfoPtr> infos = FetchOriginsSync();
  std::vector<url::Origin> origins;
  for (const auto& info : infos)
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins,
              ElementsAre(abc_xyz, chromium_org, google_com, google_org,
                          growwithgoogle_com,
                          url::Origin::Create(GURL("http://gv.com")),
                          url::Origin::Create(GURL("http://waymo.com")),
                          url::Origin::Create(GURL("http://withgoogle.com")),
                          youtube_com));
}

class SharedStorageManagerFromFileV1NoBudgetTableTest
    : public SharedStorageManagerFromFileTest {
 public:
  std::string GetRelativeFilePath() override {
    return "shared_storage.v1.no_budget_table.sql";
  }
};

// Test loading version 1 database.
TEST_F(SharedStorageManagerFromFileV1NoBudgetTableTest,
       Version1_LoadFromFileNoBudgetTable) {
  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  EXPECT_EQ(GetSync(google_com, u"key1").data, u"value1");
  EXPECT_EQ(GetSync(google_com, u"key2").data, u"value2");

  url::Origin youtube_com = url::Origin::Create(GURL("http://youtube.com/"));
  EXPECT_EQ(1L, LengthSync(youtube_com));

  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  EXPECT_EQ(GetSync(chromium_org, u"a").data, u"");

  TestSharedStorageEntriesListenerUtility listener_utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = listener_utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            KeysSync(chromium_org,
                     listener_utility.BindNewPipeAndPassRemoteForId(id1)));
  listener_utility.FlushForId(id1);
  EXPECT_THAT(listener_utility.TakeKeysForId(id1),
              ElementsAre(u"a", u"b", u"c"));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id1));
  listener_utility.VerifyNoErrorForId(id1);

  size_t id2 = listener_utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            EntriesSync(chromium_org,
                        listener_utility.BindNewPipeAndPassRemoteForId(id2)));
  listener_utility.FlushForId(id2);
  EXPECT_THAT(listener_utility.TakeEntriesForId(id2),
              ElementsAre(Pair(u"a", u""), Pair(u"b", u"hello"),
                          Pair(u"c", u"goodbye")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id2));
  listener_utility.VerifyNoErrorForId(id2);

  url::Origin google_org = url::Origin::Create(GURL("http://google.org/"));
  EXPECT_EQ(
      GetSync(google_org, u"1").data,
      u"fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "fffffffffffffffff");
  EXPECT_EQ(GetSync(google_org,
                    u"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
                .data,
            u"k");

  std::vector<mojom::StorageUsageInfoPtr> infos = FetchOriginsSync();
  std::vector<url::Origin> origins;
  for (const auto& info : infos)
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(
      origins,
      ElementsAre(
          url::Origin::Create(GURL("http://abc.xyz")), chromium_org, google_com,
          google_org, url::Origin::Create(GURL("http://growwithgoogle.com")),
          url::Origin::Create(GURL("http://gv.com")),
          url::Origin::Create(GURL("http://waymo.com")),
          url::Origin::Create(GURL("http://withgoogle.com")), youtube_com));

  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(chromium_org).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(google_com).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(google_org).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(youtube_com).bits);
}

class SharedStorageManagerParamTest
    : public SharedStorageManagerTest,
      public testing::WithParamInterface<SharedStorageWrappedBool> {
 public:
  DBType GetType() override {
    return GetParam().in_memory_only ? DBType::kInMemory
                                     : DBType::kFileBackedFromNew;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageManagerParamTest,
                         testing::ValuesIn(GetSharedStorageWrappedBools()),
                         testing::PrintToStringParamName());

TEST_P(SharedStorageManagerParamTest, BasicOperations) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value2"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value2");

  EXPECT_EQ(OperationResult::kSuccess, DeleteSync(kOrigin1, u"key1"));
  EXPECT_EQ(OperationResult::kNotFound, GetSync(kOrigin1, u"key1").result);
}

TEST_P(SharedStorageManagerParamTest, IgnoreIfPresent) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kIgnored, SetSync(kOrigin1, u"key1", u"value2",
                                               SetBehavior::kIgnoreIfPresent));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key2").data, u"value1");

  EXPECT_EQ(OperationResult::kSet,
            SetSync(kOrigin1, u"key2", u"value2", SetBehavior::kDefault));
  EXPECT_EQ(GetSync(kOrigin1, u"key2").data, u"value2");
}

TEST_P(SharedStorageManagerParamTest, Append) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, AppendSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kSet, AppendSync(kOrigin1, u"key1", u"value1"));
  std::u16string expected_value = base::StrCat({u"value1", u"value1"});
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, expected_value);

  EXPECT_EQ(OperationResult::kSet, AppendSync(kOrigin1, u"key1", u"value1"));
  expected_value = base::StrCat({std::move(expected_value), u"value1"});
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, expected_value);
}

TEST_P(SharedStorageManagerParamTest, Length) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(0, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value3"));
  EXPECT_EQ(2, LengthSync(kOrigin1));

  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(0, LengthSync(kOrigin2));

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1, LengthSync(kOrigin2));
  EXPECT_EQ(2, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, DeleteSync(kOrigin2, u"key1"));
  EXPECT_EQ(0, LengthSync(kOrigin2));
  EXPECT_EQ(2, LengthSync(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3, LengthSync(kOrigin1));
  EXPECT_EQ(0, LengthSync(kOrigin2));
}

TEST_P(SharedStorageManagerParamTest, Keys) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value2"));

  TestSharedStorageEntriesListenerUtility listener_utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = listener_utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      KeysSync(kOrigin1, listener_utility.BindNewPipeAndPassRemoteForId(id1)));
  listener_utility.FlushForId(id1);
  EXPECT_THAT(listener_utility.TakeKeysForId(id1),
              ElementsAre(u"key1", u"key2"));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id1));
  listener_utility.VerifyNoErrorForId(id1);

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  size_t id2 = listener_utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      KeysSync(kOrigin2, listener_utility.BindNewPipeAndPassRemoteForId(id2)));
  listener_utility.FlushForId(id2);
  EXPECT_TRUE(listener_utility.TakeKeysForId(id2).empty());
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id2));
  listener_utility.VerifyNoErrorForId(id2);

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key1", u"value1"));

  size_t id3 = listener_utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      KeysSync(kOrigin2, listener_utility.BindNewPipeAndPassRemoteForId(id3)));
  listener_utility.FlushForId(id3);
  EXPECT_THAT(listener_utility.TakeKeysForId(id3),
              ElementsAre(u"key1", u"key2", u"key3"));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id3));
  listener_utility.VerifyNoErrorForId(id3);

  EXPECT_EQ(OperationResult::kSuccess, DeleteSync(kOrigin2, u"key2"));

  size_t id4 = listener_utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      KeysSync(kOrigin2, listener_utility.BindNewPipeAndPassRemoteForId(id4)));
  listener_utility.FlushForId(id4);
  EXPECT_THAT(listener_utility.TakeKeysForId(id4),
              ElementsAre(u"key1", u"key3"));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id4));
  listener_utility.VerifyNoErrorForId(id4);
}

TEST_P(SharedStorageManagerParamTest, Entries) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value2"));

  TestSharedStorageEntriesListenerUtility listener_utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = listener_utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            EntriesSync(kOrigin1,
                        listener_utility.BindNewPipeAndPassRemoteForId(id1)));
  listener_utility.FlushForId(id1);
  EXPECT_THAT(listener_utility.TakeEntriesForId(id1),
              ElementsAre(Pair(u"key1", u"value1"), Pair(u"key2", u"value2")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id1));
  listener_utility.VerifyNoErrorForId(id1);

  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  size_t id2 = listener_utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            EntriesSync(kOrigin2,
                        listener_utility.BindNewPipeAndPassRemoteForId(id2)));
  listener_utility.FlushForId(id2);
  EXPECT_TRUE(listener_utility.TakeEntriesForId(id2).empty());
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id2));
  listener_utility.VerifyNoErrorForId(id2);

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key1", u"value1"));

  size_t id3 = listener_utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            EntriesSync(kOrigin2,
                        listener_utility.BindNewPipeAndPassRemoteForId(id3)));
  listener_utility.FlushForId(id3);
  EXPECT_THAT(listener_utility.TakeEntriesForId(id3),
              ElementsAre(Pair(u"key1", u"value1"), Pair(u"key2", u"value2"),
                          Pair(u"key3", u"value3")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id3));
  listener_utility.VerifyNoErrorForId(id3);

  EXPECT_EQ(OperationResult::kSuccess, DeleteSync(kOrigin2, u"key2"));

  size_t id4 = listener_utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            EntriesSync(kOrigin2,
                        listener_utility.BindNewPipeAndPassRemoteForId(id4)));
  listener_utility.FlushForId(id4);
  EXPECT_THAT(listener_utility.TakeEntriesForId(id4),
              ElementsAre(Pair(u"key1", u"value1"), Pair(u"key3", u"value3")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id4));
  listener_utility.VerifyNoErrorForId(id4);
}

TEST_P(SharedStorageManagerParamTest, Clear) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key3", u"value3"));

  EXPECT_EQ(3, LengthSync(kOrigin1));

  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key2", u"value2"));

  EXPECT_EQ(2, LengthSync(kOrigin2));

  EXPECT_EQ(OperationResult::kSuccess, ClearSync(kOrigin1));
  EXPECT_EQ(0, LengthSync(kOrigin1));
  EXPECT_EQ(2, LengthSync(kOrigin2));

  EXPECT_EQ(OperationResult::kSuccess, ClearSync(kOrigin2));
  EXPECT_EQ(0, LengthSync(kOrigin2));
}

TEST_P(SharedStorageManagerParamTest,
       FetchOriginsAndSimplePurgeMatchingOrigins) {
  EXPECT_TRUE(FetchOriginsSync().empty());

  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key3", u"value3"));

  EXPECT_EQ(3, LengthSync(kOrigin1));

  std::vector<url::Origin> origins;
  for (const auto& info : FetchOriginsSync())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin1));

  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key2", u"value2"));

  EXPECT_EQ(2, LengthSync(kOrigin2));

  origins.clear();
  for (const auto& info : FetchOriginsSync())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2));

  StorageKeyPolicyMatcherFunctionUtility matcher_utility;
  EXPECT_EQ(
      OperationResult::kSuccess,
      PurgeMatchingOriginsSync(matcher_utility.MakeMatcherFunction({kOrigin1}),
                               base::Time::Min(), base::Time::Max(),
                               /*perform_storage_cleanup=*/true));

  origins.clear();
  for (const auto& info : FetchOriginsSync())
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin2));

  EXPECT_EQ(OperationResult::kSuccess,
            PurgeMatchingOriginsSync(
                matcher_utility.MakeMatcherFunction({kOrigin1, kOrigin2}),
                base::Time::Min(), base::Time::Max(),
                /*perform_storage_cleanup=*/true));

  EXPECT_TRUE(FetchOriginsSync().empty());
}

TEST_P(SharedStorageManagerParamTest, DevTools) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key3", u"value3"));

  EXPECT_EQ(3, LengthSync(kOrigin1));

  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin2, u"key2", u"value2"));

  EXPECT_EQ(2, LengthSync(kOrigin2));

  EntriesResult origin1_entries = GetEntriesForDevToolsSync(kOrigin1);
  EXPECT_EQ(OperationResult::kSuccess, origin1_entries.result);
  EXPECT_THAT(origin1_entries.entries,
              ElementsAre(Pair("key1", "value1"), Pair("key2", "value2"),
                          Pair("key3", "value3")));

  MetadataResult origin1_metadata = GetMetadataSync(kOrigin1);
  EXPECT_EQ(OperationResult::kSuccess, origin1_metadata.time_result);
  EXPECT_EQ(OperationResult::kSuccess, origin1_metadata.budget_result);
  EXPECT_EQ(3, origin1_metadata.length);
  EXPECT_GT(origin1_metadata.creation_time.ToDeltaSinceWindowsEpoch()
                .InMicroseconds(),
            0);
  EXPECT_DOUBLE_EQ(kBitBudget, origin1_metadata.remaining_budget);

  EntriesResult origin2_entries = GetEntriesForDevToolsSync(kOrigin2);
  EXPECT_EQ(OperationResult::kSuccess, origin2_entries.result);
  EXPECT_THAT(origin2_entries.entries,
              ElementsAre(Pair("key1", "value1"), Pair("key2", "value2")));

  MetadataResult origin2_metadata = GetMetadataSync(kOrigin2);
  EXPECT_EQ(OperationResult::kSuccess, origin2_metadata.time_result);
  EXPECT_EQ(OperationResult::kSuccess, origin2_metadata.budget_result);
  EXPECT_EQ(2, origin2_metadata.length);
  EXPECT_GT(origin2_metadata.creation_time.ToDeltaSinceWindowsEpoch()
                .InMicroseconds(),
            0);
  EXPECT_DOUBLE_EQ(kBitBudget, origin2_metadata.remaining_budget);

  url::Origin kOrigin3 = url::Origin::Create(GURL("http://www.example3.test"));

  EntriesResult origin3_entries = GetEntriesForDevToolsSync(kOrigin3);
  EXPECT_EQ(OperationResult::kSuccess, origin3_entries.result);
  EXPECT_TRUE(origin3_entries.entries.empty());

  MetadataResult origin3_metadata = GetMetadataSync(kOrigin3);
  EXPECT_EQ(OperationResult::kNotFound, origin3_metadata.time_result);
  EXPECT_EQ(OperationResult::kSuccess, origin3_metadata.budget_result);
  EXPECT_EQ(0, origin3_metadata.length);
  EXPECT_DOUBLE_EQ(kBitBudget, origin3_metadata.remaining_budget);
}

TEST_P(SharedStorageManagerParamTest, AdvanceTime_StalePurged) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_FALSE(FetchOriginsSync().empty());

  // Initial interval for checking staleness is `kInitialPurgeIntervalHours`
  // hours for this test.
  task_environment_.FastForwardBy(base::Hours(kInitialPurgeIntervalHours));
  EXPECT_FALSE(FetchOriginsSync().empty());
  EXPECT_LE(GetSync(kOrigin1, u"key1").last_used_time,
            base::Time::Now() - base::Hours(kInitialPurgeIntervalHours));
  EXPECT_LE(GetCreationTimeSync(kOrigin1).time,
            base::Time::Now() - base::Hours(kInitialPurgeIntervalHours));

  // Subsequent intervals are `kRecurringPurgeIntervalHours` hours each.
  task_environment_.FastForwardBy(base::Hours(kRecurringPurgeIntervalHours));
  EXPECT_EQ(GetSync(kOrigin1, u"key1").data, u"value1");
  EXPECT_FALSE(FetchOriginsSync().empty());

  // We have set the staleness threshold to `kThresholdHours` hours for this
  // test. So `kOrigin1` should now be cleared.
  task_environment_.FastForwardBy(base::Hours(kThresholdHours));
  EXPECT_TRUE(FetchOriginsSync().empty());
}

// Synchronously tests budget operations.
TEST_P(SharedStorageManagerParamTest, SyncMakeBudgetWithdrawal) {
  // There should be no entries in the budget table.
  EXPECT_EQ(0, GetTotalNumBudgetEntriesSync());

  // SQL database hasn't yet been lazy-initialized. Nevertheless, remaining
  // budgets should be returned as the max possible.
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kOrigin1).bits);
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kOrigin2).bits);

  // A withdrawal for `kOrigin1` doesn't affect `kOrigin2`.
  EXPECT_EQ(OperationResult::kSuccess,
            MakeBudgetWithdrawalSync(kOrigin1, 1.75));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75, GetRemainingBudgetSync(kOrigin1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kOrigin2).bits);
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kOrigin1));
  EXPECT_EQ(1, GetTotalNumBudgetEntriesSync());

  // An additional withdrawal for `kOrigin1` at or near the same time as the
  // previous one is debited appropriately.
  EXPECT_EQ(OperationResult::kSuccess, MakeBudgetWithdrawalSync(kOrigin1, 2.5));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5,
                   GetRemainingBudgetSync(kOrigin1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kOrigin2).bits);
  EXPECT_EQ(2, GetNumBudgetEntriesSync(kOrigin1));
  EXPECT_EQ(2, GetTotalNumBudgetEntriesSync());

  // A withdrawal for `kOrigin2` doesn't affect `kOrigin1`.
  EXPECT_EQ(OperationResult::kSuccess, MakeBudgetWithdrawalSync(kOrigin2, 3.4));
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, GetRemainingBudgetSync(kOrigin2).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5,
                   GetRemainingBudgetSync(kOrigin1).bits);
  EXPECT_EQ(2, GetNumBudgetEntriesSync(kOrigin1));
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kOrigin2));
  EXPECT_EQ(3, GetTotalNumBudgetEntriesSync());

  // Advance partway through the lookback window, to the point where the first
  // call to `PurgeStale()` happens.
  task_environment_.FastForwardBy(base::Hours(kInitialPurgeIntervalHours));

  // Remaining budgets continue to take into account the withdrawals above, as
  // they are still within the lookback window.
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, GetRemainingBudgetSync(kOrigin2).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5,
                   GetRemainingBudgetSync(kOrigin1).bits);

  // An additional withdrawal for `kOrigin1` at a later time from previous ones
  // is debited appropriately.
  EXPECT_EQ(OperationResult::kSuccess, MakeBudgetWithdrawalSync(kOrigin1, 1.0));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75 - 2.5 - 1.0,
                   GetRemainingBudgetSync(kOrigin1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget - 3.4, GetRemainingBudgetSync(kOrigin2).bits);
  EXPECT_EQ(3, GetNumBudgetEntriesSync(kOrigin1));
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kOrigin2));
  EXPECT_EQ(4, GetTotalNumBudgetEntriesSync());

  // Advance further through the lookback window, to the point where the second
  // call to `PurgeStale()` happens.
  task_environment_.FastForwardBy(base::Hours(kRecurringPurgeIntervalHours));
  // Advance further through the lookback window, to the point where the third
  // call to `PurgeStale()` happens.
  task_environment_.FastForwardBy(base::Hours(kRecurringPurgeIntervalHours));
  // Advance further through the lookback window, to the point where the fourth
  // call to `PurgeStale()` happens.
  task_environment_.FastForwardBy(base::Hours(kRecurringPurgeIntervalHours));

  // After `PurgeStale()` runs via the timer, there will only be the most
  // recent debit left in the budget table.
  EXPECT_DOUBLE_EQ(kBitBudget - 1.0, GetRemainingBudgetSync(kOrigin1).bits);
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kOrigin2).bits);
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kOrigin1));
  EXPECT_EQ(0, GetNumBudgetEntriesSync(kOrigin2));
  EXPECT_EQ(1, GetTotalNumBudgetEntriesSync());
}

TEST_P(SharedStorageManagerParamTest, ResetBudgetForDevTools) {
  // There should be no entries in the budget table.
  EXPECT_EQ(0, GetTotalNumBudgetEntriesSync());

  // SQL database hasn't yet been lazy-initialized. Nevertheless, remaining
  // budgets should be returned as the max possible.
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kOrigin1).bits);

  // Make withdrawal.
  EXPECT_EQ(OperationResult::kSuccess,
            MakeBudgetWithdrawalSync(kOrigin1, 1.75));
  EXPECT_DOUBLE_EQ(kBitBudget - 1.75, GetRemainingBudgetSync(kOrigin1).bits);
  EXPECT_EQ(1, GetNumBudgetEntriesSync(kOrigin1));
  EXPECT_EQ(1, GetTotalNumBudgetEntriesSync());

  // Reset budget.
  EXPECT_EQ(OperationResult::kSuccess, ResetBudgetForDevToolsSync(kOrigin1));
  EXPECT_DOUBLE_EQ(kBitBudget, GetRemainingBudgetSync(kOrigin1).bits);
  EXPECT_EQ(0, GetNumBudgetEntriesSync(kOrigin1));
  EXPECT_EQ(0, GetTotalNumBudgetEntriesSync());
}

class SharedStorageManagerErrorParamTest
    : public SharedStorageManagerTest,
      public testing::WithParamInterface<SharedStorageWrappedBool> {
 public:
  SharedStorageManagerErrorParamTest() = default;

  ~SharedStorageManagerErrorParamTest() override = default;

  SharedStorageManager* GetManager() override { return mock_manager_.get(); }

  DBType GetType() override {
    return GetParam().in_memory_only ? DBType::kInMemory
                                     : DBType::kFileBackedFromNew;
  }

  void CreateManager() override {
    mock_manager_ = std::make_unique<MockSharedStorageManager>(
        db_path_, special_storage_policy_, SharedStorageOptions::Create());
  }

  void ResetManager() override {
    if (mock_manager_)
      mock_manager_.reset();
  }

  void SetResults(std::queue<OperationResult> result_queue) {
    DCHECK(mock_manager_);
    base::RunLoop loop;
    mock_manager_->SetResultsForTesting(std::move(result_queue),
                                        loop.QuitClosure());
    loop.Run();
  }

 protected:
  std::unique_ptr<MockSharedStorageManager> mock_manager_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageManagerErrorParamTest,
                         testing::ValuesIn(GetSharedStorageWrappedBools()),
                         testing::PrintToStringParamName());

// Disabled because it is flaky. crbug.com/1312044
TEST_P(SharedStorageManagerErrorParamTest,
       DISABLED_SqlErrors_ShutdownMetricsReported) {
  ASSERT_TRUE(GetManager());
  ASSERT_TRUE(GetManager()->database());

  // Note that the result queue needs to contain the int result -1 for
  // `AsyncSharedStorageDatabase::Length()` cast to an `OperationResult`.
  std::queue<OperationResult> result_queue1(
      {OperationResult::kSuccess, OperationResult::kSqlError,
       OperationResult::kSuccess, OperationResult::kSqlError,
       OperationResult::kSqlError, OperationResult::kSqlError,
       OperationResult::kSet, OperationResult::kSqlError /* -> -1 */,
       OperationResult::kSqlError, OperationResult::kSuccess});
  SetResults(std::move(result_queue1));

  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());
  EXPECT_FALSE(GetManager()->tried_to_recover_from_init_failure_for_testing());

  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSuccess, GetSync(kOrigin1, u"key1").result);
  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kSqlError,
            AppendSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kSuccess, DeleteSync(kOrigin1, u"key1"));
  EXPECT_EQ(1, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kSqlError, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(2, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kSqlError, DeleteSync(kOrigin1, u"key1"));
  EXPECT_EQ(3, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kSqlError, ClearSync(kOrigin1));
  EXPECT_EQ(4, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kSet, SetSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(4, GetManager()->operation_sql_error_count_for_testing());

  // -1 is an error for `Length()`.
  EXPECT_EQ(-1, LengthSync(kOrigin1));
  EXPECT_EQ(5, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kSqlError,
            AppendSync(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(6, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kSuccess, ClearSync(kOrigin1));
  EXPECT_EQ(6, GetManager()->operation_sql_error_count_for_testing());

  ResetManager();
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.OnShutdown.NumSqlErrors", 6, 1);
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.OnShutdown.RecoveryFromInitFailureAttempted",
      false, 1);
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.OnShutdown.RecoveryOnDiskAttempted", false, 1);
}

// TODO(crbug.com/1312273): Test is flaky.
TEST_P(SharedStorageManagerErrorParamTest,
       DISABLED_InitFailure_DestroyAndRecreateDatabase) {
  ASSERT_TRUE(GetManager());
  ASSERT_TRUE(GetManager()->database());
  SetDestroyCallback();

  // Note that the result queue needs to contain the bool result true for
  // `AsyncSharedStorageDatabase::Destroy()` cast to an
  // `OperationResult::kSuccess`.
  std::queue<OperationResult> result_queue1(
      {OperationResult::kInitFailure, OperationResult::kSuccess /* -> true */});
  SetResults(std::move(result_queue1));

  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());
  EXPECT_FALSE(GetManager()->tried_to_recover_from_init_failure_for_testing());

  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kInitFailure,
            AppendSync(kOrigin1, u"key1", u"value1"));

  // `OperationResult::kInitFailure` is not added to the error tally.
  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());
  VerifyDestroyAndRecreateDatabaseSync();
  EXPECT_EQ(!GetParam().in_memory_only,
            GetManager()->tried_to_recreate_on_disk_for_testing());

  // Reset the callback.
  SetDestroyCallback();

  // Since the database has been recreated, we need to override it again in
  // order to use a `MockAsyncSharedStorageDatabase` so that we can force
  // errors.
  //
  // Note that the result queue needs to contain the bool result true for
  // `AsyncSharedStorageDatabase::Destroy()` cast to an
  // `OperationResult::kSuccess`.
  std::queue<OperationResult> result_queue2(
      {OperationResult::kInitFailure, OperationResult::kSuccess /* -> true */});
  GetManager()->OverrideDatabaseForTesting(
      MockAsyncSharedStorageDatabase::Create(std::move(result_queue2)));
  ASSERT_TRUE(GetManager()->database());

  EXPECT_EQ(OperationResult::kInitFailure,
            SetSync(kOrigin1, u"key1", u"value1"));

  // Again, `OperationResult::kInitFailure` is not added to the error tally.
  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());

  // If in memory, we do nothing because we have already recreated the database
  // once. Otherwise we `DestroyAndRecreateDatabase()` again`, this time in
  // memory.
  if (!GetParam().in_memory_only)
    VerifyDestroyAndRecreateDatabaseSync();
  EXPECT_EQ(!GetParam().in_memory_only, destroy_success_);
  EXPECT_TRUE(GetManager()->in_memory());

  EXPECT_EQ(OperationResult::kSuccess, ClearSync(kOrigin1));
  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());

  ResetManager();
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.OnShutdown.NumSqlErrors", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.OnShutdown.RecoveryFromInitFailureAttempted", true,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.OnShutdown.RecoveryOnDiskAttempted",
      !GetParam().in_memory_only, 1);
}

// Disabled because it is flaky. crbug.com/1312044
TEST_P(SharedStorageManagerErrorParamTest,
       DISABLED_OtherOperationResults_NoErrorsAdded) {
  ASSERT_TRUE(GetManager());
  ASSERT_TRUE(GetManager()->database());
  SetDestroyCallback();

  std::queue<OperationResult> result_queue1(
      {OperationResult::kSuccess, OperationResult::kSet,
       OperationResult::kIgnored, OperationResult::kNoCapacity,
       OperationResult::kInvalidAppend});
  SetResults(std::move(result_queue1));

  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());
  EXPECT_FALSE(GetManager()->tried_to_recover_from_init_failure_for_testing());

  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSuccess, GetSync(kOrigin1, u"key1").result);

  // `OperationResult::kSuccess` is not added to the error tally.
  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kSet, AppendSync(kOrigin1, u"key1", u"value1"));

  // `OperationResult::kSet` is not added to the error tally.
  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kIgnored, SetSync(kOrigin1, u"key1", u"value1",
                                               SetBehavior::kIgnoreIfPresent));

  // `OperationResult::kIgnored` is not added to the error tally.
  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kNoCapacity,
            SetSync(kOrigin1, u"key1", u"value1"));

  // `OperationResult::kNoCapacity` is not added to the error tally.
  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_EQ(OperationResult::kInvalidAppend,
            AppendSync(kOrigin1, u"key1", u"value1"));

  // `OperationResult::kInvalidAppend` is not added to the error tally.
  EXPECT_EQ(0, GetManager()->operation_sql_error_count_for_testing());

  EXPECT_FALSE(GetManager()->tried_to_recover_from_init_failure_for_testing());
  EXPECT_FALSE(destroy_success_);

  ResetManager();
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.OnShutdown.NumSqlErrors", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.OnShutdown.RecoveryFromInitFailureAttempted",
      false, 1);
  histogram_tester_.ExpectUniqueSample(
      "Storage.SharedStorage.OnShutdown.RecoveryOnDiskAttempted", false, 1);
}

// Verifies that the async operations are executed in order and without races.
TEST_P(SharedStorageManagerParamTest, AsyncOperations) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  TestSharedStorageEntriesListenerUtility listener_utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = listener_utility.RegisterListener();
  size_t id2 = listener_utility.RegisterListener();

  std::queue<DBOperation> operation_list(
      {{Type::DB_SET,
        kOrigin1,
        {u"key1", u"value1",
         TestDatabaseOperationReceiver::SerializeSetBehavior(
             SetBehavior::kDefault)}},
       {Type::DB_GET, kOrigin1, {u"key1"}},
       {Type::DB_SET,
        kOrigin1,
        {u"key1", u"value2",
         TestDatabaseOperationReceiver::SerializeSetBehavior(
             SetBehavior::kDefault)}},
       {Type::DB_GET, kOrigin1, {u"key1"}},
       {Type::DB_SET,
        kOrigin1,
        {u"key2", u"value1",
         TestDatabaseOperationReceiver::SerializeSetBehavior(
             SetBehavior::kDefault)}},
       {Type::DB_GET, kOrigin1, {u"key2"}},
       {Type::DB_LENGTH, kOrigin1},
       {Type::DB_DELETE, kOrigin1, {u"key1"}},
       {Type::DB_LENGTH, kOrigin1},
       {Type::DB_APPEND, kOrigin1, {u"key1", u"value1"}},
       {Type::DB_GET, kOrigin1, {u"key1"}},
       {Type::DB_LENGTH, kOrigin1},
       {Type::DB_APPEND, kOrigin1, {u"key1", u"value1"}},
       {Type::DB_GET, kOrigin1, {u"key1"}},
       {Type::DB_LENGTH, kOrigin1},
       {Type::DB_KEYS, kOrigin1, {base::NumberToString16(id1)}},
       {Type::DB_ENTRIES, kOrigin1, {base::NumberToString16(id2)}},
       {Type::DB_CLEAR, kOrigin1},
       {Type::DB_LENGTH, kOrigin1},
       {Type::DB_ON_MEMORY_PRESSURE,
        {TestDatabaseOperationReceiver::SerializeMemoryPressureLevel(
            MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL)}}});

  SetExpectedOperationList(std::move(operation_list));
  ASSERT_TRUE(GetManager());
  ASSERT_TRUE(GetManager()->database());

  OperationResult result1 = OperationResult::kSqlError;
  Set(kOrigin1, u"key1", u"value1", &result1);
  GetResult value1;
  Get(kOrigin1, u"key1", &value1);
  OperationResult result2 = OperationResult::kSqlError;
  Set(kOrigin1, u"key1", u"value2", &result2);
  GetResult value2;
  Get(kOrigin1, u"key1", &value2);
  OperationResult result3 = OperationResult::kSqlError;
  Set(kOrigin1, u"key2", u"value1", &result3);
  GetResult value3;
  Get(kOrigin1, u"key2", &value3);
  int length1 = -1;
  Length(kOrigin1, &length1);

  OperationResult result4 = OperationResult::kSqlError;
  Delete(kOrigin1, u"key1", &result4);
  int length2 = -1;
  Length(kOrigin1, &length2);

  OperationResult result5 = OperationResult::kSqlError;
  Append(kOrigin1, u"key1", u"value1", &result5);
  GetResult value4;
  Get(kOrigin1, u"key1", &value4);
  int length3 = -1;
  Length(kOrigin1, &length3);

  OperationResult result6 = OperationResult::kSqlError;
  Append(kOrigin1, u"key1", u"value1", &result6);
  GetResult value5;
  Get(kOrigin1, u"key1", &value5);
  int length4 = -1;
  Length(kOrigin1, &length4);

  OperationResult result7 = OperationResult::kSqlError;
  Keys(kOrigin1, &listener_utility, id1, &result7);

  OperationResult result8 = OperationResult::kSqlError;
  Entries(kOrigin1, &listener_utility, id2, &result8);

  OperationResult result9 = OperationResult::kSqlError;
  Clear(kOrigin1, &result9);
  int length5 = -1;
  Length(kOrigin1, &length5);

  EXPECT_FALSE(memory_trimmed_);
  OnMemoryPressure(MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  EXPECT_EQ(OperationResult::kSet, result1);
  EXPECT_EQ(value1.data, u"value1");
  EXPECT_EQ(OperationResult::kSet, result2);
  EXPECT_EQ(value2.data, u"value2");
  EXPECT_EQ(OperationResult::kSet, result3);
  EXPECT_EQ(value3.data, u"value1");
  EXPECT_EQ(2, length1);

  EXPECT_EQ(OperationResult::kSuccess, result4);
  EXPECT_EQ(1, length2);

  EXPECT_EQ(OperationResult::kSet, result5);
  EXPECT_EQ(value4.data, u"value1");
  EXPECT_EQ(2, length3);

  EXPECT_EQ(OperationResult::kSet, result6);
  EXPECT_EQ(value5.data, u"value1value1");
  EXPECT_EQ(2, length4);

  EXPECT_EQ(OperationResult::kSuccess, result7);
  listener_utility.FlushForId(id1);
  EXPECT_THAT(listener_utility.TakeKeysForId(id1),
              ElementsAre(u"key1", u"key2"));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id1));
  listener_utility.VerifyNoErrorForId(id1);
  listener_utility.FlushForId(id2);

  EXPECT_EQ(OperationResult::kSuccess, result8);
  EXPECT_THAT(
      listener_utility.TakeEntriesForId(id2),
      ElementsAre(Pair(u"key1", u"value1value1"), Pair(u"key2", u"value1")));
  EXPECT_EQ(1U, listener_utility.BatchCountForId(id2));
  listener_utility.VerifyNoErrorForId(id2);

  EXPECT_EQ(OperationResult::kSuccess, result9);
  EXPECT_EQ(0, length5);

  EXPECT_TRUE(memory_trimmed_);
}

class SharedStorageManagerPurgeMatchingOriginsParamTest
    : public SharedStorageManagerTest,
      public testing::WithParamInterface<PurgeMatchingOriginsParams> {
 public:
  DBType GetType() override {
    return GetParam().in_memory_only ? DBType::kInMemory
                                     : DBType::kFileBackedFromNew;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageManagerPurgeMatchingOriginsParamTest,
                         testing::ValuesIn(GetPurgeMatchingOriginsParams()),
                         testing::PrintToStringParamName());

TEST_P(SharedStorageManagerPurgeMatchingOriginsParamTest, SinceThreshold) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  url::Origin kOrigin3 = url::Origin::Create(GURL("http://www.example3.test"));
  url::Origin kOrigin4 = url::Origin::Create(GURL("http://www.example4.test"));
  url::Origin kOrigin5 = url::Origin::Create(GURL("http://www.example5.test"));

  std::queue<DBOperation> operation_list;
  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  base::Time threshold1 = base::Time::Now();
  StorageKeyPolicyMatcherFunctionUtility matcher_utility;
  size_t matcher_id1 = matcher_utility.RegisterMatcherFunction({kOrigin1});

  operation_list.push(DBOperation(
      Type::DB_PURGE_MATCHING,
      {base::NumberToString16(matcher_id1),
       TestDatabaseOperationReceiver::SerializeTime(threshold1),
       TestDatabaseOperationReceiver::SerializeTime(base::Time::Max()),
       TestDatabaseOperationReceiver::SerializeBool(
           GetParam().perform_storage_cleanup)}));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin1,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin1,
                  {u"key2", u"value2",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin2,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin2));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin3,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin3,
                  {u"key2", u"value2",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin3,
                  {u"key3", u"value3",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin3));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key2", u"value2",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key3", u"value3",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin4,
                  {u"key4", u"value4",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin4));

  operation_list.push(
      DBOperation(Type::DB_SET, kOrigin5,
                  {u"key1", u"value1",
                   TestDatabaseOperationReceiver::SerializeSetBehavior(
                       SetBehavior::kDefault)}));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin5));

  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  base::Time threshold2 = base::Time::Now() + base::Days(1);
  base::Time override_time1 = threshold2 + base::Milliseconds(5);
  operation_list.push(DBOperation(
      Type::DB_OVERRIDE_TIME_ORIGIN, kOrigin1,
      {TestDatabaseOperationReceiver::SerializeTime(override_time1)}));

  size_t matcher_id2 =
      matcher_utility.RegisterMatcherFunction({kOrigin1, kOrigin2, kOrigin5});
  operation_list.push(DBOperation(
      Type::DB_PURGE_MATCHING,
      {base::NumberToString16(matcher_id2),
       TestDatabaseOperationReceiver::SerializeTime(threshold2),
       TestDatabaseOperationReceiver::SerializeTime(base::Time::Max()),
       TestDatabaseOperationReceiver::SerializeBool(
           GetParam().perform_storage_cleanup)}));

  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin2));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin3));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin4));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin5));

  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  base::Time threshold3 = threshold2 + base::Days(1);
  operation_list.push(
      DBOperation(Type::DB_OVERRIDE_TIME_ORIGIN, kOrigin3,
                  {TestDatabaseOperationReceiver::SerializeTime(threshold3)}));

  base::Time threshold4 = threshold3 + base::Seconds(100);
  operation_list.push(
      DBOperation(Type::DB_OVERRIDE_TIME_ORIGIN, kOrigin5,
                  {TestDatabaseOperationReceiver::SerializeTime(threshold4)}));

  size_t matcher_id3 = matcher_utility.RegisterMatcherFunction(
      {kOrigin2, kOrigin3, kOrigin4, kOrigin5});
  operation_list.push(
      DBOperation(Type::DB_PURGE_MATCHING,
                  {base::NumberToString16(matcher_id3),
                   TestDatabaseOperationReceiver::SerializeTime(threshold3),
                   TestDatabaseOperationReceiver::SerializeTime(threshold4),
                   TestDatabaseOperationReceiver::SerializeBool(
                       GetParam().perform_storage_cleanup)}));

  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin1));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin2));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin3));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin4));
  operation_list.push(DBOperation(Type::DB_LENGTH, kOrigin5));

  operation_list.push(
      DBOperation(Type::DB_ON_MEMORY_PRESSURE,
                  {TestDatabaseOperationReceiver::SerializeMemoryPressureLevel(
                      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL)}));

  operation_list.push(DBOperation(Type::DB_FETCH_ORIGINS));

  operation_list.push(DBOperation(Type::DB_GET, kOrigin2, {u"key1"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key1"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key2"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key3"}));
  operation_list.push(DBOperation(Type::DB_GET, kOrigin4, {u"key4"}));

  SetExpectedOperationList(std::move(operation_list));

  // Check that origin list is initially empty due to the database not being
  // initialized.
  std::vector<mojom::StorageUsageInfoPtr> infos1;
  FetchOrigins(&infos1);

  OperationResult result1 = OperationResult::kSqlError;
  PurgeMatchingOrigins(&matcher_utility, matcher_id1, threshold1,
                       base::Time::Max(), &result1,
                       GetParam().perform_storage_cleanup);

  OperationResult result2 = OperationResult::kSqlError;
  Set(kOrigin1, u"key1", u"value1", &result2);

  OperationResult result3 = OperationResult::kSqlError;
  Set(kOrigin1, u"key2", u"value2", &result3);
  int length1 = -1;
  Length(kOrigin1, &length1);

  OperationResult result4 = OperationResult::kSqlError;
  Set(kOrigin2, u"key1", u"value1", &result4);
  int length2 = -1;
  Length(kOrigin2, &length2);

  OperationResult result5 = OperationResult::kSqlError;
  Set(kOrigin3, u"key1", u"value1", &result5);
  OperationResult result6 = OperationResult::kSqlError;
  Set(kOrigin3, u"key2", u"value2", &result6);
  OperationResult result7 = OperationResult::kSqlError;
  Set(kOrigin3, u"key3", u"value3", &result7);
  int length3 = -1;
  Length(kOrigin3, &length3);

  OperationResult result8 = OperationResult::kSqlError;
  Set(kOrigin4, u"key1", u"value1", &result8);
  OperationResult result9 = OperationResult::kSqlError;
  Set(kOrigin4, u"key2", u"value2", &result9);
  OperationResult result10 = OperationResult::kSqlError;
  Set(kOrigin4, u"key3", u"value3", &result10);
  OperationResult result11 = OperationResult::kSqlError;
  Set(kOrigin4, u"key4", u"value4", &result11);
  int length4 = -1;
  Length(kOrigin4, &length4);

  OperationResult result12 = OperationResult::kSqlError;
  Set(kOrigin5, u"key1", u"value1", &result12);
  int length5 = -1;
  Length(kOrigin5, &length5);

  std::vector<mojom::StorageUsageInfoPtr> infos2;
  FetchOrigins(&infos2);

  bool success1 = false;
  OverrideCreationTime(kOrigin1, override_time1, &success1);

  // Verify that the only match we get is for `kOrigin1`, whose `last_used_time`
  // is between the time parameters.
  OperationResult result13 = OperationResult::kSqlError;
  PurgeMatchingOrigins(&matcher_utility, matcher_id2, threshold2,
                       base::Time::Max(), &result13,
                       GetParam().perform_storage_cleanup);

  int length6 = -1;
  Length(kOrigin1, &length6);
  int length7 = -1;
  Length(kOrigin2, &length7);
  int length8 = -1;
  Length(kOrigin3, &length8);
  int length9 = -1;
  Length(kOrigin4, &length9);
  int length10 = -1;
  Length(kOrigin5, &length10);

  std::vector<mojom::StorageUsageInfoPtr> infos3;
  FetchOrigins(&infos3);

  bool success2 = false;
  OverrideCreationTime(kOrigin3, threshold3, &success2);
  bool success3 = false;
  OverrideCreationTime(kOrigin5, threshold4, &success3);

  // Verify that we still get matches for `kOrigin3`, whose `last_used_time` is
  // exactly at the `begin` time, as well as for `kOrigin5`, whose
  // `last_used_time` is exactly at the `end` time.
  OperationResult result14 = OperationResult::kSqlError;
  PurgeMatchingOrigins(&matcher_utility, matcher_id3, threshold3, threshold4,
                       &result14, GetParam().perform_storage_cleanup);

  int length11 = -1;
  Length(kOrigin1, &length11);
  int length12 = -1;
  Length(kOrigin2, &length12);
  int length13 = -1;
  Length(kOrigin3, &length13);
  int length14 = -1;
  Length(kOrigin4, &length14);
  int length15 = -1;
  Length(kOrigin5, &length15);

  EXPECT_FALSE(memory_trimmed_);
  OnMemoryPressure(MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL);

  std::vector<mojom::StorageUsageInfoPtr> infos4;
  FetchOrigins(&infos4);

  GetResult value1;
  Get(kOrigin2, u"key1", &value1);
  GetResult value2;
  Get(kOrigin4, u"key1", &value2);
  GetResult value3;
  Get(kOrigin4, u"key2", &value3);
  GetResult value4;
  Get(kOrigin4, u"key3", &value4);
  GetResult value5;
  Get(kOrigin4, u"key4", &value5);

  WaitForOperations();
  EXPECT_TRUE(is_finished());

  // No error from calling `PurgeMatchingOrigins()` on an uninitialized
  // database.
  EXPECT_EQ(OperationResult::kSuccess, result1);

  // The call to `Set()` initializes the database.
  EXPECT_EQ(OperationResult::kSet, result2);

  EXPECT_EQ(OperationResult::kSet, result3);
  EXPECT_EQ(2, length1);

  EXPECT_EQ(OperationResult::kSet, result4);
  EXPECT_EQ(1, length2);

  EXPECT_EQ(OperationResult::kSet, result5);
  EXPECT_EQ(OperationResult::kSet, result6);
  EXPECT_EQ(OperationResult::kSet, result7);
  EXPECT_EQ(3, length3);

  EXPECT_EQ(OperationResult::kSet, result8);
  EXPECT_EQ(OperationResult::kSet, result9);
  EXPECT_EQ(OperationResult::kSet, result10);
  EXPECT_EQ(OperationResult::kSet, result11);
  EXPECT_EQ(4, length4);

  EXPECT_EQ(OperationResult::kSet, result12);
  EXPECT_EQ(1, length5);

  std::vector<url::Origin> origins;
  for (const auto& info : infos2)
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins,
              ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4, kOrigin5));

  EXPECT_EQ(value1.data, u"value1");
  EXPECT_EQ(OperationResult::kSuccess, value1.result);

  EXPECT_TRUE(success1);
  EXPECT_EQ(OperationResult::kSuccess, result13);

  // `kOrigin1` is cleared. The other origins are not.
  EXPECT_EQ(0, length6);
  EXPECT_EQ(1, length7);
  EXPECT_EQ(3, length8);
  EXPECT_EQ(4, length9);
  EXPECT_EQ(1, length10);

  origins.clear();
  for (const auto& info : infos3)
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin3, kOrigin4, kOrigin5));

  EXPECT_TRUE(success2);
  EXPECT_TRUE(success3);

  EXPECT_EQ(OperationResult::kSuccess, result14);

  // `kOrigin3` and `kOrigin5` are  cleared. The others weren't modified within
  // the given time period.
  EXPECT_EQ(0, length11);
  EXPECT_EQ(1, length12);
  EXPECT_EQ(0, length13);
  EXPECT_EQ(4, length14);
  EXPECT_EQ(0, length15);

  EXPECT_TRUE(memory_trimmed_);

  origins.clear();
  for (const auto& info : infos4)
    origins.push_back(info->storage_key.origin());
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin4));

  // Database is still intact after trimming memory (and possibly performing
  // storage cleanup).
  EXPECT_EQ(value1.data, u"value1");
  EXPECT_EQ(OperationResult::kSuccess, value1.result);
  EXPECT_EQ(value2.data, u"value1");
  EXPECT_EQ(OperationResult::kSuccess, value2.result);
  EXPECT_EQ(value3.data, u"value2");
  EXPECT_EQ(OperationResult::kSuccess, value3.result);
  EXPECT_EQ(value4.data, u"value3");
  EXPECT_EQ(OperationResult::kSuccess, value4.result);
  EXPECT_EQ(value5.data, u"value4");
  EXPECT_EQ(OperationResult::kSuccess, value5.result);
}

}  // namespace storage
