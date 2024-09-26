// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using ui::TimeFormat;

constexpr size_t kNumMocks = 3;

api::passwords_private::PasswordUiEntry CreateEntry(int id) {
  api::passwords_private::PasswordUiEntry entry;
  entry.urls.shown = "test" + base::NumberToString(id) + ".com";
  entry.urls.signon_realm = "http://" + entry.urls.shown + "/login";
  entry.urls.link = entry.urls.signon_realm;
  entry.username = "testName" + base::NumberToString(id);
  entry.id = id;
  entry.stored_in = api::passwords_private::PASSWORD_STORE_SET_DEVICE;
  return entry;
}

api::passwords_private::ExceptionEntry CreateException(int id) {
  api::passwords_private::ExceptionEntry exception;
  exception.urls.shown = "exception" + base::NumberToString(id) + ".com";
  exception.urls.signon_realm = "http://" + exception.urls.shown + "/login";
  exception.urls.link = exception.urls.signon_realm;
  exception.id = id;
  return exception;
}
}  // namespace

TestPasswordsPrivateDelegate::TestPasswordsPrivateDelegate()
    : profile_(nullptr) {
  // Create mock data.
  for (size_t i = 0; i < kNumMocks; i++) {
    current_entries_.push_back(CreateEntry(i));
    current_exceptions_.push_back(CreateException(i));
  }
}
TestPasswordsPrivateDelegate::~TestPasswordsPrivateDelegate() = default;

void TestPasswordsPrivateDelegate::GetSavedPasswordsList(
    UiEntriesCallback callback) {
  std::move(callback).Run(current_entries_);
}

PasswordsPrivateDelegate::CredentialsGroups
TestPasswordsPrivateDelegate::GetCredentialGroups() {
  std::vector<api::passwords_private::CredentialGroup> groups;
  api::passwords_private::CredentialGroup group_api;
  group_api.name = "test.com";
  for (size_t i = 0; i < kNumMocks; i++) {
    group_api.entries.push_back(CreateEntry(i));
  }
  groups.push_back(std::move(group_api));
  return groups;
}

void TestPasswordsPrivateDelegate::GetPasswordExceptionsList(
    ExceptionEntriesCallback callback) {
  std::move(callback).Run(current_exceptions_);
}

absl::optional<api::passwords_private::UrlCollection>
TestPasswordsPrivateDelegate::GetUrlCollection(const std::string& url) {
  if (url.empty()) {
    return absl::nullopt;
  }
  return absl::optional<api::passwords_private::UrlCollection>(
      api::passwords_private::UrlCollection());
}

bool TestPasswordsPrivateDelegate::IsAccountStoreDefault(
    content::WebContents* web_contents) {
  return is_account_store_default_;
}

bool TestPasswordsPrivateDelegate::AddPassword(
    const std::string& url,
    const std::u16string& username,
    const std::u16string& password,
    const std::u16string& note,
    bool use_account_store,
    content::WebContents* web_contents) {
  return !url.empty() && !password.empty();
}

absl::optional<int> TestPasswordsPrivateDelegate::ChangeSavedPassword(
    const int id,
    const api::passwords_private::ChangeSavedPasswordParams& params) {
  if (static_cast<size_t>(id) >= current_entries_.size()) {
    return absl::nullopt;
  }

  if (params.password.empty())
    return absl::nullopt;

  return id;
}

void TestPasswordsPrivateDelegate::RemoveSavedPassword(
    int id,
    api::passwords_private::PasswordStoreSet from_stores) {
  if (current_entries_.empty())
    return;

  // Since this is just mock data, remove the first element regardless of the
  // data contained. One case where this logic is especially false is when the
  // password is stored in both stores and |store| only specifies one of them
  // (in that case the number of entries shouldn't change).
  last_deleted_entry_ = std::move(current_entries_[0]);
  current_entries_.erase(current_entries_.begin());
  SendSavedPasswordsList();
}

void TestPasswordsPrivateDelegate::RemovePasswordException(int id) {
  if (current_exceptions_.empty())
    return;

  // Since this is just mock data, remove the first element regardless of the
  // data contained.
  last_deleted_exception_ = std::move(current_exceptions_[0]);
  current_exceptions_.erase(current_exceptions_.begin());
  SendPasswordExceptionsList();
}

// Simplified version of undo logic, only use for testing.
void TestPasswordsPrivateDelegate::UndoRemoveSavedPasswordOrException() {
  if (last_deleted_entry_.has_value()) {
    current_entries_.insert(current_entries_.begin(),
                            std::move(last_deleted_entry_.value()));
    last_deleted_entry_ = absl::nullopt;
    SendSavedPasswordsList();
  } else if (last_deleted_exception_.has_value()) {
    current_exceptions_.insert(current_exceptions_.begin(),
                               std::move(last_deleted_exception_.value()));
    last_deleted_exception_ = absl::nullopt;
    SendPasswordExceptionsList();
  }
}

void TestPasswordsPrivateDelegate::RequestPlaintextPassword(
    int id,
    api::passwords_private::PlaintextReason reason,
    PlaintextPasswordCallback callback,
    content::WebContents* web_contents) {
  // Return a mocked password value.
  std::move(callback).Run(plaintext_password_);
}

void TestPasswordsPrivateDelegate::RequestCredentialsDetails(
    const std::vector<int>& ids,
    UiEntriesCallback callback,
    content::WebContents* web_contents) {
  api::passwords_private::PasswordUiEntry entry = CreateEntry(42);
  if (plaintext_password_.has_value()) {
    entry.password = base::UTF16ToUTF8(plaintext_password_.value());
    UiEntries entries;
    entries.push_back(std::move(entry));
    std::move(callback).Run({std::move(entries)});
  } else {
    std::move(callback).Run({});
  }
}

void TestPasswordsPrivateDelegate::MovePasswordsToAccount(
    const std::vector<int>& ids,
    content::WebContents* web_contents) {
  last_moved_passwords_ = ids;
}

void TestPasswordsPrivateDelegate::ImportPasswords(
    api::passwords_private::PasswordStoreSet to_store,
    ImportResultsCallback results_callback,
    content::WebContents* web_contents) {
  import_passwords_triggered_ = true;

  import_results_.status = api::passwords_private::ImportResultsStatus::
      IMPORT_RESULTS_STATUS_SUCCESS;
  import_results_.file_name = "test.csv";
  import_results_.number_imported = 42;
  std::move(results_callback).Run(import_results_);
}

void TestPasswordsPrivateDelegate::ContinueImport(
    const std::vector<int>& selected_ids,
    ImportResultsCallback results_callback,
    content::WebContents* web_contents) {
  continue_import_triggered_ = true;

  import_results_.status = api::passwords_private::ImportResultsStatus::
      IMPORT_RESULTS_STATUS_SUCCESS;
  import_results_.file_name = "test.csv";
  import_results_.number_imported = 42;
  std::move(results_callback).Run(import_results_);
}

void TestPasswordsPrivateDelegate::ResetImporter(bool delete_file) {
  reset_importer_triggered_ = true;
}

void TestPasswordsPrivateDelegate::ExportPasswords(
    base::OnceCallback<void(const std::string&)> callback,
    content::WebContents* web_contents) {
  // The testing of password exporting itself should be handled via
  // |PasswordManagerPorter|.
  export_passwords_triggered_ = true;
  std::move(callback).Run(std::string());
}

void TestPasswordsPrivateDelegate::CancelExportPasswords() {
  cancel_export_passwords_triggered_ = true;
}

api::passwords_private::ExportProgressStatus
TestPasswordsPrivateDelegate::GetExportProgressStatus() {
  // The testing of password exporting itself should be handled via
  // |PasswordManagerPorter|.
  return api::passwords_private::ExportProgressStatus::
      EXPORT_PROGRESS_STATUS_IN_PROGRESS;
}

bool TestPasswordsPrivateDelegate::IsOptedInForAccountStorage() {
  return is_opted_in_for_account_storage_;
}

void TestPasswordsPrivateDelegate::SetAccountStorageOptIn(
    bool opt_in,
    content::WebContents* web_contents) {
  is_opted_in_for_account_storage_ = opt_in;
}

std::vector<api::passwords_private::PasswordUiEntry>
TestPasswordsPrivateDelegate::GetInsecureCredentials() {
  api::passwords_private::PasswordUiEntry leaked_credential;
  leaked_credential.username = "alice";
  leaked_credential.urls.shown = "example.com";
  leaked_credential.urls.link = "https://example.com";
  leaked_credential.urls.signon_realm = "https://example.com";
  leaked_credential.is_android_credential = false;
  leaked_credential.change_password_url = "https://example.com/change-password";
  leaked_credential.compromised_info.emplace();
  // Mar 03 2020 12:00:00 UTC
  leaked_credential.compromised_info->compromise_time = 1583236800000;
  leaked_credential.compromised_info->elapsed_time_since_compromise =
      base::UTF16ToUTF8(TimeFormat::Simple(
          TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_LONG, base::Days(3)));
  leaked_credential.compromised_info->compromise_types = {
      api::passwords_private::COMPROMISE_TYPE_LEAKED};
  leaked_credential.stored_in =
      api::passwords_private::PASSWORD_STORE_SET_DEVICE;

  api::passwords_private::PasswordUiEntry weak_credential;
  weak_credential.username = "bob";
  weak_credential.urls.shown = "example.com";
  weak_credential.urls.link = "https://example.com";
  weak_credential.is_android_credential = false;
  weak_credential.change_password_url = "https://example.com/change-password";
  weak_credential.stored_in = api::passwords_private::PASSWORD_STORE_SET_DEVICE;
  weak_credential.compromised_info.emplace();
  weak_credential.compromised_info->compromise_types = {
      api::passwords_private::COMPROMISE_TYPE_WEAK};

  std::vector<api::passwords_private::PasswordUiEntry> credentials;
  credentials.push_back(std::move(leaked_credential));
  credentials.push_back(std::move(weak_credential));
  return credentials;
}

std::vector<api::passwords_private::PasswordUiEntryList>
TestPasswordsPrivateDelegate::GetCredentialsWithReusedPassword() {
  std::vector<api::passwords_private::PasswordUiEntryList> result;

  api::passwords_private::PasswordUiEntry credential_1;
  credential_1.username = "bob";
  credential_1.urls.shown = "example.com";
  credential_1.urls.link = "https://example.com";
  credential_1.is_android_credential = false;
  credential_1.change_password_url = "https://example.com/change-password";
  credential_1.stored_in = api::passwords_private::PASSWORD_STORE_SET_DEVICE;
  credential_1.compromised_info.emplace();
  credential_1.compromised_info->compromise_types = {
      api::passwords_private::COMPROMISE_TYPE_REUSED};

  api::passwords_private::PasswordUiEntry credential_2;
  credential_2.username = "angela";
  credential_2.urls.shown = "test.com";
  credential_2.urls.link = "https://test.com";
  credential_2.is_android_credential = false;
  credential_2.stored_in = api::passwords_private::PASSWORD_STORE_SET_DEVICE;
  credential_2.compromised_info.emplace();
  credential_2.compromised_info->compromise_types = {
      api::passwords_private::COMPROMISE_TYPE_REUSED};

  result.emplace_back();
  result[0].entries.push_back(std::move(credential_1));
  result[0].entries.push_back(std::move(credential_2));

  return result;
}

// Fake implementation of MuteInsecureCredential. This succeeds if the
// delegate knows of a insecure credential with the same id.
bool TestPasswordsPrivateDelegate::MuteInsecureCredential(
    const api::passwords_private::PasswordUiEntry& credential) {
  return IsCredentialPresentInInsecureCredentialsList(credential);
}

// Fake implementation of UnmuteInsecureCredential. This succeeds if the
// delegate knows of a insecure credential with the same id.
bool TestPasswordsPrivateDelegate::UnmuteInsecureCredential(
    const api::passwords_private::PasswordUiEntry& credential) {
  return IsCredentialPresentInInsecureCredentialsList(credential);
}

void TestPasswordsPrivateDelegate::RecordChangePasswordFlowStarted(
    const api::passwords_private::PasswordUiEntry& credential) {
  last_change_flow_url_ =
      credential.change_password_url ? *credential.change_password_url : "";
}

void TestPasswordsPrivateDelegate::StartPasswordCheck(
    StartPasswordCheckCallback callback) {
  start_password_check_triggered_ = true;
  std::move(callback).Run(start_password_check_state_);
}

void TestPasswordsPrivateDelegate::StopPasswordCheck() {
  stop_password_check_triggered_ = true;
}

api::passwords_private::PasswordCheckStatus
TestPasswordsPrivateDelegate::GetPasswordCheckStatus() {
  api::passwords_private::PasswordCheckStatus status;
  status.state = api::passwords_private::PASSWORD_CHECK_STATE_RUNNING;
  status.already_processed = 5;
  status.remaining_in_queue = 10;
  status.elapsed_time_since_last_check = base::UTF16ToUTF8(TimeFormat::Simple(
      TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT, base::Minutes(5)));
  return status;
}

password_manager::InsecureCredentialsManager*
TestPasswordsPrivateDelegate::GetInsecureCredentialsManager() {
  return nullptr;
}

void TestPasswordsPrivateDelegate::ExtendAuthValidity() {
  authenticator_interacted_ = true;
}

void TestPasswordsPrivateDelegate::SetProfile(Profile* profile) {
  profile_ = profile;
}

void TestPasswordsPrivateDelegate::SetOptedInForAccountStorage(bool opted_in) {
  is_opted_in_for_account_storage_ = opted_in;
}

void TestPasswordsPrivateDelegate::SetIsAccountStoreDefault(bool is_default) {
  is_account_store_default_ = is_default;
}

void TestPasswordsPrivateDelegate::AddCompromisedCredential(int id) {
  api::passwords_private::PasswordUiEntry cred;
  cred.id = id;
  insecure_credentials_.push_back(std::move(cred));
}

void TestPasswordsPrivateDelegate::SendSavedPasswordsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnSavedPasswordsListChanged(current_entries_);
}

void TestPasswordsPrivateDelegate::SendPasswordExceptionsList() {
  PasswordsPrivateEventRouter* router =
      PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
  if (router)
    router->OnPasswordExceptionsListChanged(current_exceptions_);
}

bool TestPasswordsPrivateDelegate::IsCredentialPresentInInsecureCredentialsList(
    const api::passwords_private::PasswordUiEntry& credential) {
  return base::Contains(insecure_credentials_, credential.id,
                        &api::passwords_private::PasswordUiEntry::id);
}

void TestPasswordsPrivateDelegate::SwitchBiometricAuthBeforeFillingState(
    content::WebContents* web_contents) {
  authenticator_interacted_ = true;
}

void TestPasswordsPrivateDelegate::ShowAddShortcutDialog(
    content::WebContents* web_contents) {
  add_shortcut_dialog_shown_ = true;
}

void TestPasswordsPrivateDelegate::ShowExportedFileInShell(
    content::WebContents* web_contents,
    std::string file_path) {
  exported_file_shown_in_shell_ = true;
}

}  // namespace extensions
