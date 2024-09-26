// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_manager.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/core/prediction_model_download_manager.h"
#include "components/optimization_guide/core/prediction_model_fetcher.h"
#include "components/optimization_guide/core/prediction_model_fetcher_impl.h"
#include "components/optimization_guide/core/prediction_model_store.h"
#include "components/optimization_guide/core/proto_database_provider_test_base.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

using leveldb_proto::test::FakeDB;

namespace {

// Retry delay is 2 minutes to allow for fetch retry delay + some random delay
// to pass.
constexpr int kTestFetchRetryDelaySecs = 60 * 2 + 62;
// 24 hours + random fetch delay.
constexpr int kUpdateFetchModelAndFeaturesTimeSecs = 24 * 60 * 60 + 62;

constexpr char kTestLocale[] = "en-US";

}  // namespace

namespace optimization_guide {

proto::PredictionModel CreatePredictionModel() {
  proto::PredictionModel prediction_model;

  proto::ModelInfo* model_info = prediction_model.mutable_model_info();
  model_info->set_version(1);
  model_info->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_engine_versions(
      proto::ModelEngineVersion::MODEL_ENGINE_VERSION_TFLITE_2_8);
  prediction_model.mutable_model()->set_download_url(
      "https://example.com/model");
  return prediction_model;
}

std::unique_ptr<proto::GetModelsResponse> BuildGetModelsResponse() {
  std::unique_ptr<proto::GetModelsResponse> get_models_response =
      std::make_unique<proto::GetModelsResponse>();

  proto::PredictionModel prediction_model = CreatePredictionModel();
  prediction_model.mutable_model_info()->set_version(2);
  *get_models_response->add_models() = std::move(prediction_model);

  return get_models_response;
}

proto::ModelCacheKey GetTestModelCacheKey() {
  proto::ModelCacheKey model_cache_key;
  model_cache_key.set_locale(kTestLocale);
  return model_cache_key;
}

class FakeOptimizationTargetModelObserver
    : public OptimizationTargetModelObserver {
 public:
  void OnModelUpdated(proto::OptimizationTarget optimization_target,
                      const ModelInfo& model_info) override {
    last_received_models_.insert_or_assign(optimization_target, model_info);
  }

  absl::optional<ModelInfo> last_received_model_for_target(
      proto::OptimizationTarget optimization_target) const {
    auto model_it = last_received_models_.find(optimization_target);
    if (model_it == last_received_models_.end())
      return absl::nullopt;
    return model_it->second;
  }

  // Resets the state of the observer.
  void Reset() { last_received_models_.clear(); }

 private:
  base::flat_map<proto::OptimizationTarget, ModelInfo> last_received_models_;
};

class FakePredictionModelDownloadManager
    : public PredictionModelDownloadManager {
 public:
  explicit FakePredictionModelDownloadManager(
      GetBaseModelDirForDownloadCallback
          get_base_model_dir_for_download_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : PredictionModelDownloadManager(
            /*download_service=*/nullptr,
            get_base_model_dir_for_download_callback,
            task_runner) {}
  ~FakePredictionModelDownloadManager() override = default;

  void StartDownload(const GURL& url,
                     proto::OptimizationTarget optimization_target) override {
    last_requested_download_ = url;
    last_requested_optimization_target_ = optimization_target;
  }

  GURL last_requested_download() const { return last_requested_download_; }

  proto::OptimizationTarget last_requested_optimization_target() const {
    return last_requested_optimization_target_;
  }

  void CancelAllPendingDownloads() override { cancel_downloads_called_ = true; }
  bool cancel_downloads_called() const { return cancel_downloads_called_; }

  bool IsAvailableForDownloads() const override { return is_available_; }
  void SetAvailableForDownloads(bool is_available) {
    is_available_ = is_available;
  }

 private:
  GURL last_requested_download_;
  proto::OptimizationTarget last_requested_optimization_target_;
  bool cancel_downloads_called_ = false;
  bool is_available_ = true;
};

enum class PredictionModelFetcherEndState {
  kFetchFailed = 0,
  kFetchSuccessWithModels = 1,
  kFetchSuccessWithEmptyResponse = 2,
};

void RunGetModelsCallback(
    ModelsFetchedCallback callback,
    std::unique_ptr<proto::GetModelsResponse> get_models_response) {
  if (get_models_response) {
    std::move(callback).Run(std::move(get_models_response));
    return;
  }
  std::move(callback).Run(absl::nullopt);
}

// A mock class implementation of PredictionModelFetcherImpl.
class TestPredictionModelFetcher : public PredictionModelFetcherImpl {
 public:
  TestPredictionModelFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& optimization_guide_service_get_models_url,
      PredictionModelFetcherEndState fetch_state)
      : PredictionModelFetcherImpl(url_loader_factory,
                                   optimization_guide_service_get_models_url),
        fetch_state_(fetch_state) {}

  bool FetchOptimizationGuideServiceModels(
      const std::vector<proto::ModelInfo>& models_request_info,
      proto::RequestContext request_context,
      const std::string& locale,
      ModelsFetchedCallback models_fetched_callback) override {
    if (!ValidateModelsInfoForFetch(models_request_info)) {
      std::move(models_fetched_callback).Run(absl::nullopt);
      return false;
    }

    std::unique_ptr<proto::GetModelsResponse> get_models_response;
    locale_requested_ = locale;
    switch (fetch_state_) {
      case PredictionModelFetcherEndState::kFetchFailed:
        get_models_response = nullptr;
        break;
      case PredictionModelFetcherEndState::kFetchSuccessWithModels:
        models_fetched_ = true;
        get_models_response = BuildGetModelsResponse();
        break;
      case PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse:
        models_fetched_ = true;
        get_models_response = std::make_unique<proto::GetModelsResponse>();
        break;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RunGetModelsCallback,
                                  std::move(models_fetched_callback),
                                  std::move(get_models_response)));
    return true;
  }

  bool ValidateModelsInfoForFetch(
      const std::vector<proto::ModelInfo>& models_request_info) {
    for (const auto& model_info : models_request_info) {
      if (model_info.supported_model_engine_versions_size() == 0 ||
          !proto::ModelEngineVersion_IsValid(
              model_info.supported_model_engine_versions(0))) {
        return false;
      }
      if (!model_info.has_optimization_target() ||
          !proto::OptimizationTarget_IsValid(
              model_info.optimization_target())) {
        return false;
      }

      if (check_expected_version_) {
        auto version_it =
            expected_version_.find(model_info.optimization_target());
        if (model_info.has_version() !=
            (version_it != expected_version_.end())) {
          return false;
        }
        if (model_info.has_version() &&
            model_info.version() != version_it->second) {
          return false;
        }
      }

      auto it = expected_metadata_.find(model_info.optimization_target());
      if (model_info.has_model_metadata() != (it != expected_metadata_.end()))
        return false;
      if (model_info.has_model_metadata()) {
        proto::Any expected_metadata = it->second;
        if (model_info.model_metadata().type_url() !=
            expected_metadata.type_url()) {
          return false;
        }
        if (model_info.model_metadata().value() != expected_metadata.value())
          return false;
      }
    }
    return true;
  }

  void SetExpectedModelMetadataForOptimizationTarget(
      proto::OptimizationTarget optimization_target,
      const proto::Any& model_metadata) {
    expected_metadata_[optimization_target] = model_metadata;
  }

  void SetExpectedVersionForOptimizationTarget(
      proto::OptimizationTarget optimization_target,
      int64_t version) {
    expected_version_[optimization_target] = version;
  }

  void SetCheckExpectedVersion() { check_expected_version_ = true; }

  void Reset() { models_fetched_ = false; }

  bool models_fetched() const { return models_fetched_; }

  std::string locale_requested() const { return locale_requested_; }

 private:
  bool models_fetched_ = false;
  bool check_expected_version_ = false;
  std::string locale_requested_;
  // The desired behavior of the TestPredictionModelFetcher.
  PredictionModelFetcherEndState fetch_state_;
  base::flat_map<proto::OptimizationTarget, proto::Any> expected_metadata_;
  base::flat_map<proto::OptimizationTarget, int64_t> expected_version_;
};

class TestOptimizationGuideStore : public OptimizationGuideStore {
 public:
  TestOptimizationGuideStore(
      std::unique_ptr<StoreEntryProtoDatabase> database,
      scoped_refptr<base::SequencedTaskRunner> store_task_runner)
      : OptimizationGuideStore(std::move(database),
                               store_task_runner,
                               nullptr) {}

  ~TestOptimizationGuideStore() override = default;

  void Initialize(bool purge_existing_data,
                  base::OnceClosure callback) override {
    init_callback_ = std::move(callback);
    status_ = Status::kAvailable;
  }

  void RunInitCallback(bool load_models = true,
                       bool have_models_in_store = true) {
    load_models_ = load_models;
    have_models_in_store_ = have_models_in_store;
    if (init_callback_)
      std::move(init_callback_).Run();
  }

  void LoadPredictionModel(const EntryKey& prediction_model_entry_key,
                           PredictionModelLoadedCallback callback) override {
    model_loaded_ = true;
    if (load_models_) {
      std::move(callback).Run(
          std::make_unique<proto::PredictionModel>(CreatePredictionModel()));
    } else {
      std::move(callback).Run(nullptr);
    }
  }

  bool FindPredictionModelEntryKey(
      proto::OptimizationTarget optimization_target,
      OptimizationGuideStore::EntryKey* out_prediction_model_entry_key)
      override {
    if (optimization_target ==
        proto::OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN) {
      return false;
    }
    if (have_models_in_store_) {
      *out_prediction_model_entry_key =
          "4_" + base::NumberToString(static_cast<int>(optimization_target));
      return true;
    }
    return false;
  }

  void UpdatePredictionModels(
      std::unique_ptr<StoreUpdateData> prediction_models_update_data,
      base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  bool WasModelLoaded() const { return model_loaded_; }

 private:
  base::OnceClosure init_callback_;
  bool model_loaded_ = false;
  bool load_models_ = true;
  bool have_models_in_store_ = true;
};

class TestPredictionManager : public PredictionManager {
 public:
  TestPredictionManager(
      base::WeakPtr<OptimizationGuideStore> model_and_features_store,
      PredictionModelStore* prediction_model_store,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      ComponentUpdatesEnabledProvider component_updates_enabled_provider,
      bool off_the_record,
      const std::string& application_locale,
      const base::FilePath& models_dir_path)
      : PredictionManager(
            model_and_features_store,
            prediction_model_store,
            url_loader_factory,
            pref_service,
            off_the_record,
            application_locale,
            models_dir_path,
            &optimization_guide_logger_,
            /*background_download_service_provider=*/
            base::OnceCallback<download::BackgroundDownloadService*()>(),
            component_updates_enabled_provider) {}

  ~TestPredictionManager() override = default;

 private:
  OptimizationGuideLogger optimization_guide_logger_;
};

class PredictionManagerTestBase : public ProtoDatabaseProviderTestBase {
 public:
  using StoreEntry = proto::StoreEntry;
  using StoreEntryMap = std::map<OptimizationGuideStore::EntryKey, StoreEntry>;
  PredictionManagerTestBase() = default;
  ~PredictionManagerTestBase() override = default;

  PredictionManagerTestBase(const PredictionManagerTestBase&) = delete;
  PredictionManagerTestBase& operator=(const PredictionManagerTestBase&) =
      delete;

  void SetUp() override {
    ProtoDatabaseProviderTestBase::SetUp();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterProfilePrefs(pref_service_->registry());

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableCheckingUserPermissionsForTesting);
  }

  void CreatePredictionManager() {
    if (prediction_manager_) {
      db_store_.clear();
      model_and_features_store_.reset();
      prediction_manager_.reset();
    }

    model_and_features_store_ = CreateModelAndHostModelFeaturesStore();
    prediction_manager_ = std::make_unique<TestPredictionManager>(
        !features::IsInstallWideModelStoreEnabled()
            ? model_and_features_store_->AsWeakPtr()
            : nullptr,
        prediction_model_store_.get(), url_loader_factory_, pref_service_.get(),
        base::BindRepeating(
            &PredictionManagerTestBase::AreComponentUpdatesEnabled,
            base::Unretained(this)),
        false, kTestLocale, temp_dir());
    prediction_manager_->SetClockForTesting(task_environment_.GetMockClock());
  }

  std::unique_ptr<TestOptimizationGuideStore>
  CreateModelAndHostModelFeaturesStore() {
    // Setup the fake db and the class under test.
    auto db = std::make_unique<FakeDB<StoreEntry>>(&db_store_);

    return std::make_unique<TestOptimizationGuideStore>(
        std::move(db), task_environment_.GetMainThreadTaskRunner());
  }

  TestPredictionManager* prediction_manager() const {
    return prediction_manager_.get();
  }

  void TearDown() override { ProtoDatabaseProviderTestBase::TearDown(); }

  std::unique_ptr<TestPredictionModelFetcher> BuildTestPredictionModelFetcher(
      PredictionModelFetcherEndState end_state) {
    std::unique_ptr<TestPredictionModelFetcher> prediction_model_fetcher =
        std::make_unique<TestPredictionModelFetcher>(
            url_loader_factory_, GURL("https://hintsserver.com"), end_state);
    return prediction_model_fetcher;
  }

  void SetStoreInitialized(bool load_models = true,
                           bool have_models_in_store = true) {
    if (features::IsInstallWideModelStoreEnabled()) {
      prediction_manager_->MaybeInitializeModelDownloads(
          /*background_download_service=*/nullptr);
    } else {
      models_and_features_store()->RunInitCallback(load_models,
                                                   have_models_in_store);
    }
    RunUntilIdle();
    // Move clock forward for any short delays added for the fetcher, until the
    // startup fetch could start.
    MoveClockForwardBy(base::Seconds(12));
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  TestPredictionModelFetcher* prediction_model_fetcher() const {
    return static_cast<TestPredictionModelFetcher*>(
        prediction_manager()->prediction_model_fetcher());
  }

  void CreatePredictionModelDownloadManager() {
    prediction_manager()->SetPredictionModelDownloadManagerForTesting(
        std::make_unique<FakePredictionModelDownloadManager>(
            base::BindRepeating(&PredictionManager::GetBaseModelDirForDownload,
                                base::Unretained(prediction_manager())),
            task_environment()->GetMainThreadTaskRunner()));
  }

  FakePredictionModelDownloadManager* prediction_model_download_manager()
      const {
    return static_cast<FakePredictionModelDownloadManager*>(
        base::BindRepeating(&PredictionManager::GetBaseModelDirForDownload,
                            base::Unretained(prediction_manager())),
        prediction_manager()->prediction_model_download_manager());
  }

  TestOptimizationGuideStore* models_and_features_store() const {
    base::WeakPtr<OptimizationGuideStore> store =
        prediction_manager()->model_and_features_store();
    DCHECK(features::IsInstallWideModelStoreEnabled() || store);
    return static_cast<TestOptimizationGuideStore*>(store.get());
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  TestingPrefServiceSimple* pref_service() const { return pref_service_.get(); }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  void SetComponentUpdatesPrefEnabled(bool enabled) {
    component_updates_enabled_ = enabled;
  }

  bool AreComponentUpdatesEnabled() const { return component_updates_enabled_; }

 protected:
  // |feature_list_| needs to be destroyed after |task_environment_|, to avoid
  // tsan flakes caused by other tasks running while |feature_list_| is
  // destroyed.
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PredictionModelStore> prediction_model_store_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  StoreEntryMap db_store_;
  std::unique_ptr<TestOptimizationGuideStore> model_and_features_store_;
  std::unique_ptr<TestPredictionManager> prediction_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_prefs_;
  bool component_updates_enabled_ = true;
};

class PredictionManagerRemoteFetchingDisabledTest
    : public PredictionManagerTestBase {
 public:
  PredictionManagerRemoteFetchingDisabledTest() {
    // This needs to be done before any tasks are run that might check if a
    // feature is enabled, to avoid tsan errors.
    feature_list_.InitAndDisableFeature(
        features::kRemoteOptimizationGuideFetching);
  }
};

TEST_F(PredictionManagerRemoteFetchingDisabledTest, RemoteFetchingDisabled) {
  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);
  SetStoreInitialized();

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
}

class PredictionManagerModelDownloadingDisabledTest
    : public PredictionManagerTestBase {
 public:
  PredictionManagerModelDownloadingDisabledTest() {
    // This needs to be done before any tasks are run that might check if a
    // feature is enabled, to avoid tsan errors.
    feature_list_.InitAndDisableFeature(
        features::kOptimizationGuideModelDownloading);
  }
};

TEST_F(PredictionManagerModelDownloadingDisabledTest,
       ModelDownloadingDisabledShouldNotFetch) {
  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);
  SetStoreInitialized();

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
}

class PredictionManagerTest : public testing::WithParamInterface<bool>,
                              public PredictionManagerTestBase {
 public:
  PredictionManagerTest() {
    // This needs to be done before any tasks are run that might check if a
    // feature is enabled, to avoid tsan errors.

    std::vector<base::test::FeatureRef> enabled_features = {
        features::kRemoteOptimizationGuideFetching,
        features::kOptimizationGuideModelDownloading,
    };
    if (ShouldEnableInstallWideModelStore()) {
      local_state_prefs_ = std::make_unique<TestingPrefServiceSimple>();
      prefs::RegisterLocalStatePrefs(local_state_prefs_->registry());
      enabled_features.emplace_back(
          features::kOptimizationGuideInstallWideModelStore);
    }
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  void SetUp() override {
    PredictionManagerTestBase::SetUp();
    if (ShouldEnableInstallWideModelStore()) {
      prediction_model_store_ =
          PredictionModelStore::CreatePredictionModelStoreForTesting(
              local_state_prefs_.get(), temp_dir());
    }
  }

  void CreateTestModelFiles(const proto::ModelInfo model_info,
                            const base::FilePath& base_model_dir) {
    CreateDirectory(base_model_dir);
    WriteFile(base_model_dir.Append(GetBaseFileNameForModels()), "");
    std::string model_info_str;
    ASSERT_TRUE(model_info.SerializeToString(&model_info_str));
    WriteFile(base_model_dir.Append(GetBaseFileNameForModelInfo()),
              model_info_str);
    RunUntilIdle();
  }

  PredictionModelStore* prediction_model_store() {
    return prediction_model_store_.get();
  }

  bool ShouldEnableInstallWideModelStore() const { return GetParam(); }

 private:
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<TestingPrefServiceSimple> local_state_prefs_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PredictionManagerTest,
                         /*ShouldEnableInstallWideModelStore=*/testing::Bool());

TEST_P(PredictionManagerTest, RemoteFetchingPrefDisabled) {
  SetComponentUpdatesPrefEnabled(false);
  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);
  SetStoreInitialized();

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
}

TEST_P(PredictionManagerTest, AddObserverForOptimizationTargetModel) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));
  proto::Any model_metadata;
  model_metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.PageEntitiesModelMetadata");
  prediction_model_fetcher()->SetExpectedModelMetadataForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.RegistrationTimeSinceServiceInit."
      "PainfulPageLoad",
      0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.FirstModelFetchSinceServiceInit", 0);

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata, &observer);
  SetStoreInitialized(/* load_models= */ false,
                      /* have_models_in_store= */ false);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "PainfulPageLoad",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.RegistrationTimeSinceServiceInit."
      "PainfulPageLoad",
      1);

  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  // Make sure the test histogram is recorded. We don't check for value here
  // since that is too much toil for someone whenever they add a new version.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.SupportedModelEngineVersion", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.FirstModelFetchSinceServiceInit", 1);

  EXPECT_TRUE(prediction_manager()->GetRegisteredOptimizationTargets().contains(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());

  base::FilePath additional_file_path =
      temp_dir().AppendASCII("foo").AppendASCII("additional_file.txt");
  proto::ModelInfo model_info;
  model_info.set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info.set_version(1);
  model_info.mutable_model_metadata()->set_type_url("sometypeurl");
  model_info.add_additional_files()->set_file_path(
      FilePathToString(additional_file_path));
  // An empty file path should be be ignored.
  model_info.add_additional_files()->set_file_path("");

  // Ensure observer is hooked up.
  {
    base::HistogramTester model_ready_histogram_tester;

    proto::PredictionModel model1;
    *model1.mutable_model_info() = model_info;
    model1.mutable_model()->set_download_url(FilePathToString(
        temp_dir().AppendASCII("foo").Append(GetBaseFileNameForModels())));
    prediction_manager()->OnModelReady(temp_dir().AppendASCII("foo"), model1);
    RunUntilIdle();

    absl::optional<ModelInfo> received_model =
        observer.last_received_model_for_target(
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_EQ(received_model->GetModelMetadata()->type_url(), "sometypeurl");
    EXPECT_EQ(temp_dir().AppendASCII("foo").Append(GetBaseFileNameForModels()),
              received_model->GetModelFilePath());
    EXPECT_EQ(received_model->GetAdditionalFiles(),
              base::flat_set<base::FilePath>{additional_file_path});

    // Make sure we do not record the model available histogram again.
    model_ready_histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
        "PainfulPageLoad",
        0);
  }

  // Reset fetcher and make sure version is sent in the new request and not
  // counted as re-loaded or updated.
  {
    base::HistogramTester histogram_tester2;

    prediction_model_fetcher()->Reset();
    prediction_model_fetcher()->SetCheckExpectedVersion();
    prediction_model_fetcher()->SetExpectedVersionForOptimizationTarget(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 1);
    MoveClockForwardBy(base::Seconds(kUpdateFetchModelAndFeaturesTimeSecs));
    EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
    histogram_tester2.ExpectTotalCount(
        "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
    histogram_tester2.ExpectTotalCount(
        "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
    histogram_tester2.ExpectTotalCount(
        "OptimizationGuide.PredictionModelRemoved.PainfulPageLoad", 0);
  }

  // Now remove and reset observer.
  prediction_manager()->RemoveObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, &observer);
  observer.Reset();
  proto::PredictionModel model2;
  model_info.clear_additional_files();
  *model2.mutable_model_info() = model_info;
  model2.mutable_model_info()->set_version(2);
  model2.mutable_model()->set_download_url(FilePathToString(
      temp_dir().AppendASCII("bar").AppendASCII("bar_model.tflite")));
  prediction_manager()->OnModelReady(temp_dir().AppendASCII("bar"), model2);
  RunUntilIdle();

  // Last received path should not have been updated since the observer was
  // removed.
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());
}

TEST_P(PredictionManagerTest,
       AddObserverForOptimizationTargetModelAddAnotherObserverForSameTarget) {
  // Fails under "threadsafe" mode.
  testing::GTEST_FLAG(death_test_style) = "fast";

  CreatePredictionManager();

  FakeOptimizationTargetModelObserver observer1;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt, &observer1);
  SetStoreInitialized(/* load_models= */ false,
                      /* have_models_in_store= */ false);

  proto::ModelInfo model_info;
  model_info.set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info.set_version(1);

  // Ensure observer is hooked up.
  proto::PredictionModel model1;
  *model1.mutable_model_info() = model_info;
  model1.mutable_model()->set_download_url(FilePathToString(
      temp_dir().AppendASCII("foo").Append(GetBaseFileNameForModels())));
  prediction_manager()->OnModelReady(temp_dir().AppendASCII("foo"), model1);
  RunUntilIdle();

  EXPECT_EQ(temp_dir().AppendASCII("foo").Append(GetBaseFileNameForModels()),
            observer1
                .last_received_model_for_target(
                    proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                ->GetModelFilePath());

#if !BUILDFLAG(IS_WIN)
  // Do not run the DCHECK death test on Windows since there's some weird
  // behavior there.

  // Now, register a new observer - it should die.
  FakeOptimizationTargetModelObserver observer2;
  EXPECT_DCHECK_DEATH(
      prediction_manager()->AddObserverForOptimizationTargetModel(
          proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
          /*model_metadata=*/absl::nullopt, &observer2));
  RunUntilIdle();
#endif
}

// See crbug/1227996.
#if !BUILDFLAG(IS_WIN)
TEST_P(PredictionManagerTest,
       AddObserverForOptimizationTargetModelCommandLineOverride) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Any metadata;
  metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.PageEntitiesModelMetadata");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  base::Base64Encode(encoded_metadata, &encoded_metadata);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s:%s",
                         kTestAbsoluteFilePath, encoded_metadata.c_str()));

  CreatePredictionManager();

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithEmptyResponse));
  proto::Any model_metadata;
  model_metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.PageEntitiesModelMetadata");
  prediction_model_fetcher()->SetExpectedModelMetadataForOptimizationTarget(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata);

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_metadata, &observer);
  SetStoreInitialized(/* load_models= */ false,
                      /* have_models_in_store= */ false);

  // Make sure no models are fetched.
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  // However, expect that the histogram for model engine version is recorded.
  // We don't check for value here since that is too much toil for someone
  // whenever they add a new version.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.SupportedModelEngineVersion", 1);

  EXPECT_TRUE(prediction_manager()->GetRegisteredOptimizationTargets().contains(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_EQ(
      observer
          .last_received_model_for_target(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
          ->GetModelMetadata()
          ->type_url(),
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1.PageEntitiesModelMetadata");
  EXPECT_EQ(observer
                .last_received_model_for_target(
                    proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                ->GetModelFilePath()
                .value(),
            FILE_PATH_LITERAL(kTestAbsoluteFilePath));

  // Now reset observer. New model downloads should not update the observer.
  observer.Reset();
  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(1);
  model.mutable_model()->set_download_url(FilePathToString(
      temp_dir().AppendASCII("foo").Append(GetBaseFileNameForModels())));
  prediction_manager()->OnModelReady(temp_dir().AppendASCII("foo"), model);
  RunUntilIdle();

  // Last received path should not have been updated since the observer was
  // reset and override is in place.
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());
}
#endif

TEST_P(PredictionManagerTest,
       NoPredictionModelForRegisteredOptimizationTarget) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();
  SetStoreInitialized(/*load_models=*/false, /*have_models_in_store=*/false);
  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_MODEL_VALIDATION, absl::nullopt, &observer);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "ModelValidation",
      false, 1);
}

TEST_P(PredictionManagerTest, UpdatePredictionModelsWithInvalidModel) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt, &observer);

  // Set invalid model with no download url.
  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(3);
  model.mutable_model();
  prediction_manager()->OnModelReady(temp_dir().AppendASCII("foo"), model);
  RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.IsPredictionModelValid.PainfulPageLoad", false, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelValidationLatency.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
  if (!ShouldEnableInstallWideModelStore()) {
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelRemoved.PainfulPageLoad", true, 1);
  }
}

TEST_P(PredictionManagerTest, UpdateModelFileWithSameVersion) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt, &observer);

  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(3);
  model.mutable_model()->set_download_url(FilePathToString(
      temp_dir().AppendASCII("foo").Append(GetBaseFileNameForModels())));
  prediction_manager()->OnModelReady(temp_dir().AppendASCII("foo"), model);
  RunUntilIdle();

  EXPECT_TRUE(observer
                  .last_received_model_for_target(
                      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                  .has_value());

  // Now reset the observer state.
  observer.Reset();

  // Send the same model again.
  prediction_manager()->OnModelReady(temp_dir().AppendASCII("foo"), model);

  // The observer should not have received an update.
  EXPECT_FALSE(observer
                   .last_received_model_for_target(
                       proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
                   .has_value());
}

TEST_P(PredictionManagerTest, DownloadManagerUnavailableShouldNotFetch) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  CreatePredictionModelDownloadManager();
  prediction_model_download_manager()->SetAvailableForDownloads(false);

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);

  SetStoreInitialized(/*load_models=*/true, /*have_models_in_store=*/false);
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "DownloadServiceAvailabilityBlockedFetch",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents.PainfulPageLoad",
      ModelDeliveryEvent::kDownloadServiceUnavailable, 1);
}

TEST_P(PredictionManagerTest, UpdateModelWithDownloadUrl) {
  base::HistogramTester histogram_tester;

  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  CreatePredictionModelDownloadManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  EXPECT_TRUE(prediction_model_download_manager()->cancel_downloads_called());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "DownloadServiceAvailabilityBlockedFetch",
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.IsDownloadUrlValid.PainfulPageLoad",
      true, 1);

  EXPECT_EQ(prediction_model_download_manager()->last_requested_download(),
            GURL("https://example.com/model"));
  EXPECT_EQ(
      prediction_model_download_manager()->last_requested_optimization_target(),
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
}

TEST_P(PredictionManagerTest, UpdateModelForUnregisteredTargetOnModelReady) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();

  SetStoreInitialized();

  auto base_model_dir = temp_dir().AppendASCII("foo");
  proto::PredictionModel model;
  model.mutable_model_info()->set_optimization_target(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model.mutable_model_info()->set_version(3);
  model.mutable_model()->set_download_url(
      FilePathToString(base_model_dir.Append(GetBaseFileNameForModels())));
  if (ShouldEnableInstallWideModelStore()) {
    CreateTestModelFiles(model.model_info(), base_model_dir);
  }
  prediction_manager()->OnModelReady(base_model_dir, model);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);

  // Now register the model.
  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);

  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "PainfulPageLoad",
      true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents.PainfulPageLoad",
      2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents.PainfulPageLoad",
      ModelDeliveryEvent::kModelDownloaded, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionManager.ModelDeliveryEvents.PainfulPageLoad",
      ModelDeliveryEvent::kModelDelivered, 1);
}

TEST_P(PredictionManagerTest,
       StoreInitializedAfterOptimizationTargetRegistered) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();
  if (ShouldEnableInstallWideModelStore()) {
    proto::ModelInfo model_info;
    model_info.set_optimization_target(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    model_info.set_version(1);
    auto base_model_dir = temp_dir().AppendASCII("foo");
    CreateTestModelFiles(model_info, base_model_dir);
    prediction_model_store()->UpdateModel(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, GetTestModelCacheKey(),
        model_info, base_model_dir, base::DoNothing());
    RunUntilIdle();
  }

  // Ensure that the fetch does not cause any models or features to load.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);
  if (!ShouldEnableInstallWideModelStore())
    EXPECT_FALSE(models_and_features_store()->WasModelLoaded());

  SetStoreInitialized();
  if (!ShouldEnableInstallWideModelStore())
    EXPECT_TRUE(models_and_features_store()->WasModelLoaded());

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "PainfulPageLoad",
      true, 1);
}

TEST_P(PredictionManagerTest,
       StoreInitializedBeforeOptimizationTargetRegistered) {
  base::HistogramTester histogram_tester;
  CreatePredictionManager();
  if (ShouldEnableInstallWideModelStore()) {
    proto::ModelInfo model_info;
    model_info.set_optimization_target(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    model_info.set_version(1);
    auto base_model_dir = temp_dir().AppendASCII("foo");
    CreateTestModelFiles(model_info, base_model_dir);
    prediction_model_store()->UpdateModel(
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, GetTestModelCacheKey(),
        model_info, base_model_dir, base::DoNothing());
    RunUntilIdle();
  }

  // Ensure that the fetch does not cause any models or features to load.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  SetStoreInitialized();

  if (!ShouldEnableInstallWideModelStore())
    EXPECT_FALSE(models_and_features_store()->WasModelLoaded());
  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);
  RunUntilIdle();

  if (!ShouldEnableInstallWideModelStore())
    EXPECT_TRUE(models_and_features_store()->WasModelLoaded());

  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.ModelAvailableAtRegistration."
      "PainfulPageLoad",
      true, 1);
}

TEST_P(PredictionManagerTest, ModelFetcherTimerRetryDelay) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchFailed));
  CreatePredictionModelDownloadManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);

  SetStoreInitialized();
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  MoveClockForwardBy(base::Seconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());

  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));

  MoveClockForwardBy(base::Seconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
}

TEST_P(PredictionManagerTest, ModelFetcherTimerFetchSucceeds) {
  CreatePredictionManager();
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  CreatePredictionModelDownloadManager();

  FakeOptimizationTargetModelObserver observer;
  prediction_manager()->AddObserverForOptimizationTargetModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, absl::nullopt, &observer);

  SetStoreInitialized();
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
  EXPECT_EQ("en-US", prediction_model_fetcher()->locale_requested());

  // Reset the prediction model fetcher to detect when the next fetch occurs.
  prediction_manager()->SetPredictionModelFetcherForTesting(
      BuildTestPredictionModelFetcher(
          PredictionModelFetcherEndState::kFetchSuccessWithModels));
  MoveClockForwardBy(base::Seconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(prediction_model_fetcher()->models_fetched());
  MoveClockForwardBy(base::Seconds(kUpdateFetchModelAndFeaturesTimeSecs));
  EXPECT_TRUE(prediction_model_fetcher()->models_fetched());
}

}  // namespace optimization_guide
