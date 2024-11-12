// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/jobs/get_isolated_web_app_size_job.h"

#include <memory>
#include <optional>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/storage_partition_config.h"
#include "ui/base/models/tree_model.h"
#include "url/origin.h"

namespace web_app {

namespace {
const char kDebugOriginKey[] = "iwa_origins";

// Estimates the size in bytes of a non-default StoragePartition by summing the
// size of all browsing data stored within it.
class StoragePartitionSizeEstimator : private ProfileObserver {
 public:
  static void EstimateSize(
      Profile* profile,
      const content::StoragePartitionConfig& storage_partition_config,
      base::OnceCallback<void(int64_t)> complete_callback) {
    DCHECK(!storage_partition_config.is_default());

    // |owning_closure| will own the StoragePartitionSizeEstimator and delete
    // it when called or reset.
    auto* estimator = new StoragePartitionSizeEstimator(profile);
    base::OnceClosure owning_closure =
        base::BindOnce(&DeleteEstimatorSoon, base::WrapUnique(estimator));

    estimator->Start(
        storage_partition_config,
        std::move(complete_callback).Then(std::move(owning_closure)));
  }

 private:
  static void DeleteEstimatorSoon(
      std::unique_ptr<StoragePartitionSizeEstimator> estimator) {
    estimator->profile_->RemoveObserver(estimator.get());
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(estimator));
  }

  explicit StoragePartitionSizeEstimator(Profile* profile) : profile_(profile) {
    profile_->AddObserver(this);
  }

  void Start(const content::StoragePartitionConfig& storage_partition_config,
             base::OnceCallback<void(int64_t)> complete_callback) {
    complete_callback_ = std::move(complete_callback);
    content::StoragePartition* storage_partition =
        profile_->GetStoragePartition(storage_partition_config);
    BrowsingDataModel::BuildFromNonDefaultStoragePartition(
        storage_partition,
        ChromeBrowsingDataModelDelegate::CreateForStoragePartition(
            profile_, storage_partition),
        base::BindOnce(&StoragePartitionSizeEstimator::BrowsingDataModelLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void BrowsingDataModelLoaded(
      std::unique_ptr<BrowsingDataModel> browsing_data_model) {
    browsing_data_model_ = std::move(browsing_data_model);

    int64_t size = 0;
    for (const BrowsingDataModel::BrowsingDataEntryView& entry :
         *browsing_data_model_) {
      size += entry.data_details->storage_size;
    }
    std::move(complete_callback_).Run(size);
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    // Abort if the Profile is being deleted. |complete_callback_| owns the
    // object, so resetting it will delete |this|.
    complete_callback_.Reset();
  }

  raw_ptr<Profile> profile_ = nullptr;
  base::OnceCallback<void(int64_t)> complete_callback_;
  std::unique_ptr<BrowsingDataModel> browsing_data_model_;
  base::WeakPtrFactory<StoragePartitionSizeEstimator> weak_ptr_factory_{this};
};

}  // namespace

GetIsolatedWebAppSizeJob::GetIsolatedWebAppSizeJob(
    Profile* profile,
    const webapps::AppId& app_id,
    base::Value::Dict& debug_value,
    ResultCallback result_callback)
    : app_id_(app_id),
      profile_(profile),
      debug_value_(debug_value),
      result_callback_(std::move(result_callback)) {
  debug_value_->Set("profile", profile->GetDebugName());
}

GetIsolatedWebAppSizeJob::~GetIsolatedWebAppSizeJob() = default;

void GetIsolatedWebAppSizeJob::Start(
    WithAppResources* lock_with_app_resources) {
  CHECK(lock_with_app_resources);
  lock_with_app_resources_ = lock_with_app_resources;

  const WebAppRegistrar& web_app_registrar =
      lock_with_app_resources_->registrar();
  ASSIGN_OR_RETURN(const WebApp& isolated_web_app,
                   GetIsolatedWebAppById(web_app_registrar, app_id_),
                   [&](const std::string& error) {
                     CHECK_EQ(pending_task_count_, 0);
                     std::move(result_callback_).Run(std::nullopt);
                   });

  pending_task_count_++;
  iwa_origin_ = url::Origin::Create(isolated_web_app.scope());
  for (const content::StoragePartitionConfig& storage_partition_config :
       web_app_registrar.GetIsolatedWebAppStoragePartitionConfigs(app_id_)) {
    if (storage_partition_config.in_memory()) {
      continue;
    }
    pending_task_count_++;
    debug_value_->EnsureDict(kDebugOriginKey)->Set(iwa_origin_.Serialize(), -1);
    StoragePartitionSizeEstimator::EstimateSize(
        profile_, storage_partition_config,
        base::BindOnce(&GetIsolatedWebAppSizeJob::StoragePartitionSizeFetched,
                       weak_factory_.GetWeakPtr()));
  }

  pending_task_count_--;

  MaybeCompleteCommand();
}

void GetIsolatedWebAppSizeJob::StoragePartitionSizeFetched(int64_t size) {
  DCHECK_GT(pending_task_count_, 0);
  pending_task_count_--;
  browsing_data_size_ += size;
  // Store the size as a double because Value::Dict doesn't support 64-bit
  // integers. This should only lead to data loss when size is >2^54.
  debug_value_->EnsureDict(kDebugOriginKey)
      ->Set(iwa_origin_.Serialize(), static_cast<double>(size));

  MaybeCompleteCommand();
}

void GetIsolatedWebAppSizeJob::MaybeCompleteCommand() {
  if (pending_task_count_ == 0) {
    std::move(result_callback_)
        .Run(GetIsolatedWebAppSizeJobResult{.iwa_origin = iwa_origin_,
                                            .app_size = browsing_data_size_});
  }
}

}  // namespace web_app
