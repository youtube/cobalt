// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_local_data_batch_uploader.h"

#include <memory>
#include <optional>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/service/local_data_description.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

// Returns the latest of a password form's last used time, last update time and
// creation time. In some cases, last used time and last update time can be null
// (see crbug.com/1483452), so the max of the 3 is used.
base::Time GetLatestOfTimeLastUsedOrModifiedOrCreated(
    const PasswordForm& form) {
  return std::max(
      {form.date_last_used, form.date_password_modified, form.date_created});
}

// This tuple is used as a type in `syncer::LocalDataItemModel::DataId` for
// PASSWORDS. It should match with the result of `PasswordFormUniqueKey()`
// (omitting the const& since the key outlives the data).
using PasswordFormKey = std::
    tuple<std::string, GURL, std::u16string, std::u16string, std::u16string>;

std::set<PasswordFormKey> ToPasswordFormUniqueKeySet(
    const std::vector<syncer::LocalDataItemModel::DataId>& item_id_list) {
  std::set<PasswordFormKey> key_set;
  for (const syncer::LocalDataItemModel::DataId& data_id : item_id_list) {
    key_set.insert(std::get<PasswordFormKey>(data_id));
  }
  return key_set;
}

}  // namespace

class PasswordLocalDataBatchUploader::PasswordFetchRequest
    : public PasswordStoreConsumer {
 public:
  PasswordFetchRequest() = default;

  PasswordFetchRequest(const PasswordFetchRequest&) = delete;
  PasswordFetchRequest& operator=(const PasswordFetchRequest&) = delete;

  ~PasswordFetchRequest() override = default;

  // Must be called at most once. `done_callback` will be called when the
  // passwords are fetched, unless the object is destroyed before that. It's
  // safe for the callback to destroy the object, it does nothing else after
  // calling back.
  void Run(PasswordStoreInterface* password_store,
           base::OnceClosure done_callback) {
    CHECK(!done_callback_ && !results_.has_value());
    done_callback_ = std::move(done_callback);
    password_store->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
  }

  // Must only be called after the passed `done_callback` was invoked.
  std::vector<std::unique_ptr<PasswordForm>> TakeResults() {
    CHECK(results_.has_value());
    return std::move(results_.value());
  }

 private:
  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    results_ = std::move(results);
    std::move(done_callback_).Run();
    // `this` might be deleted now, do not do anything else.
  }

  base::OnceClosure done_callback_;
  std::optional<std::vector<std::unique_ptr<PasswordForm>>> results_;
  base::WeakPtrFactory<PasswordFetchRequest> weak_ptr_factory_{this};
};

PasswordLocalDataBatchUploader::PasswordLocalDataBatchUploader(
    scoped_refptr<PasswordStoreInterface> profile_store,
    scoped_refptr<PasswordStoreInterface> account_store)
    : profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)) {}

PasswordLocalDataBatchUploader::~PasswordLocalDataBatchUploader() = default;

void PasswordLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  if (!CanUpload()) {
    std::move(callback).Run(syncer::LocalDataDescription());
    return;
  }

  auto request = std::make_unique<PasswordFetchRequest>();
  PasswordFetchRequest* request_ptr = request.get();
  // Unretained() is safe, `this` outlives the `request`.
  request_ptr->Run(
      profile_store_.get(),
      base::BindOnce(
          &PasswordLocalDataBatchUploader::OnGotLocalPasswordsForDescription,
          base::Unretained(this), std::move(callback), std::move(request)));
}

void PasswordLocalDataBatchUploader::TriggerLocalDataMigrationInternal(
    std::optional<std::vector<syncer::LocalDataItemModel::DataId>> items) {
  if (!CanUpload()) {
    return;
  }

  auto profile_store_request = std::make_unique<PasswordFetchRequest>();
  auto account_store_request = std::make_unique<PasswordFetchRequest>();
  PasswordFetchRequest* profile_store_request_ptr = profile_store_request.get();
  PasswordFetchRequest* account_store_request_ptr = account_store_request.get();
  // Unretained() is safe, `this` outlives the requests.
  auto barrier_closure = base::BarrierClosure(
      2, base::BindOnce(
             &PasswordLocalDataBatchUploader::OnGotAllPasswordsForMigration,
             base::Unretained(this), std::move(profile_store_request),
             std::move(account_store_request), std::move(items)));
  trigger_local_data_migration_ongoing_ = true;
  profile_store_request_ptr->Run(profile_store_.get(), barrier_closure);
  account_store_request_ptr->Run(account_store_.get(), barrier_closure);
}

void PasswordLocalDataBatchUploader::TriggerLocalDataMigration() {
  TriggerLocalDataMigrationInternal(std::nullopt);
}

void PasswordLocalDataBatchUploader::TriggerLocalDataMigration(
    std::vector<syncer::LocalDataItemModel::DataId> items) {
  TriggerLocalDataMigrationInternal(std::move(items));
}

void PasswordLocalDataBatchUploader::OnGotLocalPasswordsForDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> description_callback,
    std::unique_ptr<PasswordFetchRequest> request) {
  const bool is_batch_upload_desktop_enabled =
      switches::IsBatchUploadDesktopEnabled();
  std::vector<GURL> urls;
  std::vector<syncer::LocalDataItemModel> local_data_models;
  for (auto& result : request->TakeResults()) {
    if (is_batch_upload_desktop_enabled) {
      syncer::LocalDataItemModel item;
      item.id = PasswordFormKey(PasswordFormUniqueKey(*result.get()));
      item.title = result->url.host();
      item.subtitle = base::UTF16ToUTF8(result->username_value);
      item.icon_url = result->url.GetWithEmptyPath();
      local_data_models.push_back(std::move(item));
    }
    urls.push_back(result->url);
  }

  syncer::LocalDataDescription local_data(std::move(urls));
  if (is_batch_upload_desktop_enabled) {
    local_data.type = syncer::DataType::PASSWORDS;
    local_data.local_data_models = std::move(local_data_models);
  }
  std::move(description_callback).Run(std::move(local_data));
}

void PasswordLocalDataBatchUploader::OnGotAllPasswordsForMigration(
    std::unique_ptr<PasswordFetchRequest> profile_store_request,
    std::unique_ptr<PasswordFetchRequest> account_store_request,
    std::optional<std::vector<syncer::LocalDataItemModel::DataId>> items) {
  trigger_local_data_migration_ongoing_ = false;

  std::optional<std::set<PasswordFormKey>> items_to_upload;
  if (items.has_value()) {
    items_to_upload = ToPasswordFormUniqueKeySet(*items);
  }

  std::vector<std::unique_ptr<PasswordForm>> local_passwords =
      profile_store_request->TakeResults();
  std::vector<std::unique_ptr<PasswordForm>> account_passwords =
      account_store_request->TakeResults();

  auto comparator = [](const std::unique_ptr<PasswordForm>& lhs,
                       const std::unique_ptr<PasswordForm>& rhs) {
    return PasswordFormUniqueKey(*lhs) < PasswordFormUniqueKey(*rhs);
  };
  std::ranges::sort(account_passwords, comparator);

  int moved_passwords_counter = 0;
  for (const std::unique_ptr<PasswordForm>& local_password : local_passwords) {
    // Check if `local_password` should be filtered out or not.
    if (items_to_upload.has_value() &&
        !items_to_upload.value().contains(
            PasswordFormUniqueKey(*local_password))) {
      // Filter out passwords not selected for upload by the user.
      continue;
    }

    // Check for conflicts in the account store. If there are none, add
    // `local_password`. Otherwise, only add if it's newer than the account
    // one.
    auto it =
        std::ranges::lower_bound(account_passwords, local_password, comparator);
    if (it == account_passwords.end() ||
        !ArePasswordFormUniqueKeysEqual(**it, *local_password)) {
      account_store_->AddLogin(*local_password);
      ++moved_passwords_counter;
    } else if ((*it)->password_value != local_password->password_value &&
               GetLatestOfTimeLastUsedOrModifiedOrCreated(**it) <
                   GetLatestOfTimeLastUsedOrModifiedOrCreated(
                       *local_password)) {
      account_store_->UpdateLogin(*local_password);
      ++moved_passwords_counter;
    }
    profile_store_->RemoveLogin(FROM_HERE, *local_password);
  }

  base::UmaHistogramCounts1M("Sync.PasswordsBatchUpload.Count",
                             moved_passwords_counter);
}

bool PasswordLocalDataBatchUploader::CanUpload() const {
  // Note that if `trigger_local_data_migration_ongoing_` is true the function
  // returns false. This is because migrations include all local data, and
  // therefore upon completion it is extremely likely that there is no local
  // data left. Without this special-casing, a call to
  // `GetLocalDataDescription` closely following `TriggerLocalDataMigration`
  // could incorrectly report that local data exists, simply because the
  // migration hasn't completed just yet.
  return profile_store_ && account_store_ &&
         account_store_->IsAbleToSavePasswords() &&
         !trigger_local_data_migration_ongoing_;
}

}  // namespace password_manager
