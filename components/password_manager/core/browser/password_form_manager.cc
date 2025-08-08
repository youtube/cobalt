// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/containers/lru_cache.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_backend_error.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#endif  // BUILDFLAG(IS_ANDROID)

using autofill::FieldDataManager;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormRendererId;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::password_generation::PasswordGenerationType;
using base::TimeTicks;
using password_manager_util::IsSingleUsernameType;
using signin::GaiaIdHash;

#if BUILDFLAG(IS_ANDROID)
using webauthn::WebAuthnCredManDelegate;
#endif  // BUILDFLAG(IS_ANDROID)

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

bool PasswordFormManager::wait_for_server_predictions_for_filling_ = true;

namespace {

bool FormContainsFieldWithName(const FormData& form,
                               const std::u16string& element) {
  if (element.empty())
    return false;

  auto equals_element_case_insensitive =
      [&element](const std::u16string& name) {
        return base::EqualsCaseInsensitiveASCII(name, element);
      };
  return base::ranges::any_of(form.fields, equals_element_case_insensitive,
                              &FormFieldData::name);
}

bool IsPhished(const PasswordForm& credentials) {
  return credentials.password_issues.find(InsecureType::kPhished) !=
         credentials.password_issues.end();
}

void LogUsingPossibleUsername(PasswordManagerClient* client,
                              bool is_used,
                              const char* message) {
  if (!password_manager_util::IsLoggingActive(client))
    return;
  BrowserSavePasswordProgressLogger logger(client->GetLogManager());
  logger.LogString(is_used ? Logger::STRING_POSSIBLE_USERNAME_USED
                           : Logger::STRING_POSSIBLE_USERNAME_NOT_USED,
                   message);
}

#if BUILDFLAG(IS_ANDROID)
bool IsCurrentUserEvicted(PasswordManagerClient* client) {
  return client->GetPrefs()->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
}

bool ShouldShowErrorMessage(
    absl::optional<PasswordStoreBackendError> backend_error,
    PasswordManagerClient* client) {
  if (!backend_error.has_value())
    return false;
  PasswordStoreBackendError error = backend_error.value();
  bool is_auth_error =
      (error.type == PasswordStoreBackendErrorType::kAuthErrorResolvable) ||
      (error.type == PasswordStoreBackendErrorType::kAuthErrorUnresolvable);
  if (!is_auth_error)
    return false;
  if (IsCurrentUserEvicted(client))
    return false;
  DCHECK(error.recovery_type !=
         PasswordStoreBackendErrorRecoveryType::kUnrecoverable);
  return true;
}
#endif

// Returns true if `form`s username value equals `username_value` (case
// insensitive).
PasswordFormHadMatchingUsername FormMatchesUsername(
    const PasswordForm& form,
    const std::u16string& username_value) {
  return PasswordFormHadMatchingUsername(
      base::EqualsCaseInsensitiveASCII(username_value, form.username_value));
}

bool IsPasswordFormWithoutUsername(const PasswordForm* form) {
  return form && form->HasNonEmptyPasswordValue() &&
         form->username_value.empty();
}

// Given username found outside of the form, check and return priority
// (`UsernameFoundOutsideOfFormType`) of the field and whether it matches the
// username value found inside password form.
std::pair<UsernameFoundOutsideOfFormType, PasswordFormHadMatchingUsername>
GivePriorityToUsernameFoundOutsideOfForm(
    const PossibleUsernameData& candidate_username,
    const PasswordForm& form) {
  PasswordFormHadMatchingUsername password_form_had_matching_username =
      FormMatchesUsername(form, candidate_username.value);
  if (candidate_username.HasSingleUsernameOverride()) {
    return {UsernameFoundOutsideOfFormType::kSingleUsernameOverride,
            password_form_had_matching_username};
  }
  if (candidate_username.HasSingleUsernameServerPrediction()) {
    return {UsernameFoundOutsideOfFormType::kSingleUsernamePrediction,
            password_form_had_matching_username};
  }
  if (password_form_had_matching_username) {
    return {UsernameFoundOutsideOfFormType::kMatchingUsername,
            PasswordFormHadMatchingUsername(true)};
  }
  if (candidate_username.autocomplete_attribute_has_username &&
      !candidate_username.HasServerPrediction() &&
      base::FeatureList::IsEnabled(
          password_manager::features::kUsernameFirstFlowHonorAutocomplete)) {
    return {UsernameFoundOutsideOfFormType::kUsernameAutocomplete,
            PasswordFormHadMatchingUsername(false)};
  }
  // Return the weakest signal otherwise. User typed something and there is a
  // chance that it was a single username.
  return {UsernameFoundOutsideOfFormType::kUserModifiedTextField,
          PasswordFormHadMatchingUsername(false)};
}

void SetUsernameValueFromOutsideOfForm(const std::u16string& value,
                                       PasswordForm& form) {
  // Username is found outside of the password form. Clear username field
  // predictions that is inside the password form to not send incorrect
  // votes.
  form.username_value = value;
  form.username_element_renderer_id = FieldRendererId();
  form.username_element.clear();
}

// Checks whether username found outside of the password form was found using
// more reliable signal than the username found inside the password form.
bool UsernameOutsideOfFormHasHigherPriority(
    UsernameFoundOutsideOfFormType possible_username_type,
    FormDataParser::UsernameDetectionMethod
        username_in_the_password_form_type) {
  return possible_username_type ==
             UsernameFoundOutsideOfFormType::kSingleUsernameOverride ||
         (possible_username_type ==
              UsernameFoundOutsideOfFormType::kSingleUsernamePrediction &&
          username_in_the_password_form_type !=
              FormDataParser::UsernameDetectionMethod::kServerSidePrediction);
}

}  // namespace

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const FormData& observed_form_data,
    FormFetcher* form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder)
    : PasswordFormManager(client,
                          observed_form_data,
                          form_fetcher,
                          std::move(password_save_manager),
                          metrics_recorder) {
  driver_ = driver;
  if (driver_) {
    driver_id_ = driver->GetId();
    cached_driver_frame_id_ = driver->GetFrameId();
  }

  metrics_recorder_->RecordFormSignature(
      CalculateFormSignature(*observed_form()));
  // Do not fetch saved credentials for Chrome sync form, since nor filling nor
  // saving are supported.
  if (owned_form_fetcher_ &&
      !observed_form()->is_gaia_with_skip_save_password_form) {
    owned_form_fetcher_->Fetch();

    WebAuthnCredentialsDelegate* delegate =
        client_->GetWebAuthnCredentialsDelegateForDriver(driver_.get());
    if (delegate) {
      delegate->RetrievePasskeys(async_predictions_waiter_.CreateClosure());
    }
  }
  votes_uploader_.StoreInitialFieldValues(*observed_form());
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    PasswordFormDigest observed_http_auth_digest,
    FormFetcher* form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager)
    : PasswordFormManager(client,
                          observed_http_auth_digest,
                          form_fetcher,
                          std::move(password_save_manager),
                          nullptr /* metrics_recorder */) {
  if (owned_form_fetcher_)
    owned_form_fetcher_->Fetch();
}

PasswordFormManager::~PasswordFormManager() {
  form_fetcher_->RemoveConsumer(this);
}

bool PasswordFormManager::DoesManage(
    autofill::FormRendererId form_renderer_id,
    const PasswordManagerDriver* driver) const {
  if (driver != driver_.get())
    return false;
  CHECK(observed_form());
  return observed_form()->unique_renderer_id == form_renderer_id;
}

bool PasswordFormManager::IsEqualToSubmittedForm(
    const autofill::FormData& form) const {
  if (!is_submitted_)
    return false;
  if (IsHttpAuth())
    return false;

  if (form.action.is_valid() && !form.is_action_empty &&
      !submitted_form_.is_action_empty &&
      submitted_form_.action == form.action) {
    return true;
  }

  // Match the form if username and password fields are same.
  if (FormContainsFieldWithName(form,
                                parsed_submitted_form_->username_element) &&
      FormContainsFieldWithName(form,
                                parsed_submitted_form_->password_element)) {
    return true;
  }

  // Match the form if the observed username field has the same value as in
  // the submitted form.
  if (!parsed_submitted_form_->username_value.empty()) {
    for (const auto& field : form.fields) {
      if (field.value == parsed_submitted_form_->username_value)
        return true;
    }
  }
  return false;
}

const GURL& PasswordFormManager::GetURL() const {
  return observed_form() ? observed_form()->url : observed_digest()->url;
}

const std::vector<const PasswordForm*>& PasswordFormManager::GetBestMatches()
    const {
  return form_fetcher_->GetBestMatches();
}

std::vector<const PasswordForm*> PasswordFormManager::GetFederatedMatches()
    const {
  return form_fetcher_->GetFederatedMatches();
}

const PasswordForm& PasswordFormManager::GetPendingCredentials() const {
  return password_save_manager_->GetPendingCredentials();
}

metrics_util::CredentialSourceType PasswordFormManager::GetCredentialSource()
    const {
  return metrics_util::CredentialSourceType::kPasswordManager;
}

PasswordFormMetricsRecorder* PasswordFormManager::GetMetricsRecorder() {
  return metrics_recorder_.get();
}

base::span<const InteractionsStats> PasswordFormManager::GetInteractionsStats()
    const {
  return base::make_span(form_fetcher_->GetInteractionsStats());
}

std::vector<const PasswordForm*> PasswordFormManager::GetInsecureCredentials()
    const {
  return form_fetcher_->GetInsecureCredentials();
}

int PasswordFormManager::GetFrameId() {
  if (driver_) {
    // When possible, use the most up-to-date frame id, as it can change after
    // events such as prerender activations.
    cached_driver_frame_id_ = driver_->GetFrameId();
  }
  return cached_driver_frame_id_;
}

bool PasswordFormManager::IsBlocklisted() const {
  return form_fetcher_->IsBlocklisted() || newly_blocklisted_;
}

bool PasswordFormManager::IsMovableToAccountStore() const {
  DCHECK(
      client_->GetPasswordFeatureManager()->ShouldShowAccountStorageBubbleUi())
      << "Ensure that the client supports moving passwords for this user!";
  signin::IdentityManager* identity_manager = client_->GetIdentityManager();
  DCHECK(identity_manager);
  const std::string gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  DCHECK(!gaia_id.empty()) << "Cannot move without signed in user";

  const std::u16string& username = GetPendingCredentials().username_value;
  const std::u16string& password = GetPendingCredentials().password_value;
  // If no match in the profile store with the same username and password exist,
  // then there is nothing to move.
  auto is_movable = [&](const PasswordForm* match) {
    return !match->IsUsingAccountStore() && match->username_value == username &&
           match->password_value == password;
  };
  return base::ranges::any_of(form_fetcher_->GetBestMatches(), is_movable) &&
         !form_fetcher_->IsMovingBlocked(GaiaIdHash::FromGaiaId(gaia_id),
                                         username);
}

void PasswordFormManager::Save() {
  CHECK_EQ(form_fetcher_->GetState(), FormFetcher::State::NOT_WAITING);
  CHECK(!client_->IsOffTheRecord());
  if (IsBlocklisted()) {
    password_save_manager_->Unblocklist(ConstructObservedFormDigest());
    newly_blocklisted_ = false;
  }

  // This is potentially the conclusion of a password change flow. It might also
  // not be related to such a flow at all, but the tracker will figure it out.
  PasswordChangeSuccessTracker::EndEvent end_event =
      HasGeneratedPassword() ? PasswordChangeSuccessTracker::EndEvent::
                                   kManualFlowGeneratedPasswordChosen
                             : PasswordChangeSuccessTracker::EndEvent::
                                   kManualFlowOwnPasswordChosen;
  client_->GetPasswordChangeSuccessTracker()->OnChangePasswordFlowCompleted(
      parsed_submitted_form_->url,
      base::UTF16ToUTF8(GetPendingCredentials().username_value), end_event,
      IsPhished(GetPendingCredentials()));

  password_save_manager_->Save(observed_form(), *parsed_submitted_form_);

  client_->UpdateFormManagers();
}

void PasswordFormManager::Update(const PasswordForm& credentials_to_update) {
  // This is potentially the conclusion of a password change flow. It might also
  // not be related to such a flow at all, but the tracker will figure it out.
  PasswordChangeSuccessTracker::EndEvent end_event =
      HasGeneratedPassword() ? PasswordChangeSuccessTracker::EndEvent::
                                   kManualFlowGeneratedPasswordChosen
                             : PasswordChangeSuccessTracker::EndEvent::
                                   kManualFlowOwnPasswordChosen;
  client_->GetPasswordChangeSuccessTracker()->OnChangePasswordFlowCompleted(
      parsed_submitted_form_->url,
      base::UTF16ToUTF8(GetPendingCredentials().username_value), end_event,
      IsPhished(credentials_to_update));

  password_save_manager_->Update(credentials_to_update, observed_form(),
                                 *parsed_submitted_form_);

  client_->UpdateFormManagers();
}

void PasswordFormManager::OnUpdateUsernameFromPrompt(
    const std::u16string& new_username) {
  DCHECK(parsed_submitted_form_);
  parsed_submitted_form_->username_value = new_username;
  parsed_submitted_form_->username_element_renderer_id =
      autofill::FieldRendererId();
  parsed_submitted_form_->username_element.clear();

  password_save_manager_->UsernameUpdatedInBubble();
  metrics_recorder_->set_username_updated_in_bubble(true);

  if (!new_username.empty()) {
    // Try to find `new_username` in the usernames `parsed_submitted_form_`
    // knows about. Set `votes_uploader_`'s UsernameChangeState depending on
    // whether the username is present or not. Also set `username_element` if it
    // is a known username.
    const auto& alternative_usernames =
        parsed_submitted_form_->all_alternative_usernames;
    auto alternative_username_it = base::ranges::find(
        alternative_usernames, new_username, &AlternativeElement::value);

    if (alternative_username_it != alternative_usernames.end()) {
      parsed_submitted_form_->username_element = alternative_username_it->name;
      parsed_submitted_form_->username_element_renderer_id =
          alternative_username_it->field_renderer_id;
      votes_uploader_.set_username_change_state(
          VotesUploader::UsernameChangeState::kChangedToKnownValue);
    } else {
      votes_uploader_.set_username_change_state(
          VotesUploader::UsernameChangeState::kChangedToUnknownValue);
    }
  }

  CreatePendingCredentials();
}

void PasswordFormManager::OnUpdatePasswordFromPrompt(
    const std::u16string& new_password) {
  DCHECK(parsed_submitted_form_);
  parsed_submitted_form_->password_value = new_password;
  parsed_submitted_form_->password_element.clear();
  parsed_submitted_form_->password_element_renderer_id = FieldRendererId();

  // The user updated a password from the prompt. It means that heuristics were
  // wrong. So clear new password, since it is likely wrong.
  parsed_submitted_form_->new_password_value.clear();
  parsed_submitted_form_->new_password_element.clear();
  parsed_submitted_form_->new_password_element_renderer_id = FieldRendererId();

  const AlternativeElementVector& alternative_passwords =
      parsed_submitted_form_->all_alternative_passwords;
  auto alternative_password_it = base::ranges::find(
      alternative_passwords, new_password, &AlternativeElement::value);
  if (alternative_password_it != alternative_passwords.end()) {
    parsed_submitted_form_->password_element = alternative_password_it->name;
    parsed_submitted_form_->password_element_renderer_id =
        alternative_password_it->field_renderer_id;
  }

  CreatePendingCredentials();
}

void PasswordFormManager::UpdateSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event) {
  parsed_submitted_form_->form_data.submission_event = event;
  parsed_submitted_form_->submission_event = event;
  password_save_manager_->UpdateSubmissionIndicatorEvent(event);
}

void PasswordFormManager::OnNopeUpdateClicked() {
  votes_uploader_.UploadPasswordVote(*parsed_submitted_form_,
                                     *parsed_submitted_form_,
                                     autofill::NOT_NEW_PASSWORD, std::string());
}

void PasswordFormManager::OnNeverClicked() {
  // |UNKNOWN_TYPE| is sent in order to record that a generation popup was
  // shown and ignored.
  votes_uploader_.UploadPasswordVote(*parsed_submitted_form_,
                                     *parsed_submitted_form_,
                                     autofill::UNKNOWN_TYPE, std::string());
  Blocklist();
}

void PasswordFormManager::OnNoInteraction(bool is_update) {
  // |UNKNOWN_TYPE| is sent in order to record that a generation popup was
  // shown and ignored.
  votes_uploader_.UploadPasswordVote(
      *parsed_submitted_form_, *parsed_submitted_form_,
      is_update ? autofill::PROBABLY_NEW_PASSWORD : autofill::UNKNOWN_TYPE,
      std::string());
}

void PasswordFormManager::Blocklist() {
  CHECK(!client_->IsOffTheRecord());
  password_save_manager_->Blocklist(ConstructObservedFormDigest());
  newly_blocklisted_ = true;
}

PasswordFormDigest PasswordFormManager::ConstructObservedFormDigest() const {
  std::string signon_realm;
  GURL url;
  if (observed_digest()) {
    url = observed_digest()->url;
    // GetSignonRealm is not suitable for http auth credentials.
    signon_realm = IsHttpAuth() ? observed_digest()->signon_realm
                                : GetSignonRealm(observed_digest()->url);
  } else {
    url = observed_form()->url;
    signon_realm = GetSignonRealm(observed_form()->url);
  }
  return PasswordFormDigest(GetScheme(), signon_realm, url);
}

void PasswordFormManager::OnPasswordsRevealed() {
  votes_uploader_.set_has_passwords_revealed_vote(true);
}

void PasswordFormManager::MoveCredentialsToAccountStore() {
  DCHECK(client_->GetPasswordFeatureManager()->IsOptedInForAccountStorage());
  password_save_manager_->MoveCredentialsToAccountStore(
      metrics_util::MoveToAccountStoreTrigger::
          kSuccessfulLoginWithProfileStorePassword);
}

void PasswordFormManager::BlockMovingCredentialsToAccountStore() {
  // Nothing to do if there is no signed in user or the credentials are already
  // blocked for moving.
  if (!IsMovableToAccountStore())
    return;
  const std::string gaia_id =
      client_->GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  // The above call to IsMovableToAccountStore() guarantees there is a signed in
  // user.
  DCHECK(!gaia_id.empty());
  password_save_manager_->BlockMovingToAccountStoreFor(
      GaiaIdHash::FromGaiaId(gaia_id));
}

bool PasswordFormManager::IsNewLogin() const {
  return password_save_manager_->IsNewLogin();
}

FormFetcher* PasswordFormManager::GetFormFetcher() {
  return form_fetcher_;
}

void PasswordFormManager::PresaveGeneratedPassword(
    const FormData& form_data,
    const std::u16string& generated_password) {
  *mutable_observed_form() = form_data;
  PresaveGeneratedPasswordInternal(form_data, generated_password);
}

void PasswordFormManager::PasswordNoLongerGenerated() {
  if (!HasGeneratedPassword())
    return;

  password_save_manager_->PasswordNoLongerGenerated();
}

bool PasswordFormManager::HasGeneratedPassword() const {
  return password_save_manager_->HasGeneratedPassword();
}

void PasswordFormManager::SetGenerationPopupWasShown(
    PasswordGenerationType type) {
  const bool is_manual_generation = type == PasswordGenerationType::kManual;
  votes_uploader_.set_generation_popup_was_shown(true);
  votes_uploader_.set_is_manual_generation(is_manual_generation);
  metrics_recorder_->SetPasswordGenerationPopupShown(true,
                                                     is_manual_generation);
}

void PasswordFormManager::SetGenerationElement(
    FieldRendererId generation_element) {
  votes_uploader_.set_generation_element(generation_element);
}

bool PasswordFormManager::HasLikelyChangeOrResetFormSubmitted() const {
  return parsed_submitted_form_ &&
         (parsed_submitted_form_->IsLikelyChangePasswordForm() ||
          parsed_submitted_form_->IsLikelyResetPasswordForm());
}

bool PasswordFormManager::IsPasswordUpdate() const {
  return password_save_manager_->IsPasswordUpdate();
}

base::WeakPtr<PasswordManagerDriver> PasswordFormManager::GetDriver() const {
  return driver_;
}

const PasswordForm* PasswordFormManager::GetSubmittedForm() const {
  return parsed_submitted_form_.get();
}

#if BUILDFLAG(IS_IOS)
void PasswordFormManager::UpdateStateOnUserInput(
    FormRendererId form_id,
    FieldRendererId field_id,
    const std::u16string& field_value) {
  DCHECK(observed_form()->unique_renderer_id == form_id);
  // Update the observed field value.
  auto modified_field = base::ranges::find_if(
      mutable_observed_form()->fields, [&field_id](const FormFieldData& field) {
        return field.unique_renderer_id == field_id;
      });
  if (modified_field == mutable_observed_form()->fields.end())
    return;
  modified_field->value = field_value;

  if (!HasGeneratedPassword())
    return;
  // Update the presaved password form. Even if generated password was not
  // modified, the user might have modified the username.
  std::u16string generated_password =
      password_save_manager_->GetGeneratedPassword();
  CHECK(!generated_password.empty());
  if (votes_uploader_.get_generation_element() == field_id) {
    generated_password = field_value;
    CHECK(!generated_password.empty());
  }
  PresaveGeneratedPasswordInternal(*observed_form(), generated_password);
}

void PasswordFormManager::SetDriver(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  driver_ = driver;
}

void PasswordFormManager::ProvisionallySaveFieldDataManagerInfo(
    const FieldDataManager& field_data_manager,
    const PasswordManagerDriver* driver) {
  bool data_found = false;
  for (FormFieldData& field : mutable_observed_form()->fields) {
    FieldRendererId field_id = field.unique_renderer_id;
    if (!field_data_manager.HasFieldData(field_id))
      continue;
    field.user_input = field_data_manager.GetUserInput(field_id);
    field.properties_mask = field_data_manager.GetFieldPropertiesMask(field_id);
    data_found = true;
  }

  // Provisionally save form and set the manager to be submitted if valid
  // data was recovered.
  if (data_found)
    ProvisionallySave(*observed_form(), driver, nullptr);
}
#endif  // BUILDFLAG(IS_IOS)

void PasswordFormManager::SaveSuggestedUsernameValueToVotesUploader() {
  votes_uploader_.set_suggested_username(
      GetPendingCredentials().username_value);
}

std::unique_ptr<PasswordFormManager> PasswordFormManager::Clone() {
  // Fetcher is cloned to avoid re-fetching data from PasswordStore.
  std::unique_ptr<FormFetcher> fetcher = form_fetcher_->Clone();

  // Some data is filled through the constructor. No PasswordManagerDriver is
  // needed, because the UI does not need any functionality related to the
  // renderer process, to which the driver serves as an interface.
  auto result = base::WrapUnique(new PasswordFormManager(
      client_, observed_form_or_digest_, fetcher.get(),
      password_save_manager_->Clone(), metrics_recorder_));

  // The constructor only can take a weak pointer to the fetcher, so moving the
  // owning one needs to happen explicitly.
  result->owned_form_fetcher_ = std::move(fetcher);

  // These data members all satisfy:
  //   (1) They could have been changed by |*this| between its construction and
  //       calling Clone().
  //   (2) They are potentially used in the clone as the clone is used in the UI
  //       code.
  //   (3) They are not changed during OnFetchCompleted, triggered at some point
  //   by the
  //       cloned FormFetcher.
  result->votes_uploader_ = votes_uploader_;

  if (parser_.predictions())
    result->parser_.set_predictions(*parser_.predictions());

  if (parsed_submitted_form_) {
    result->parsed_submitted_form_ =
        std::make_unique<PasswordForm>(*parsed_submitted_form_);
  }
  result->is_submitted_ = is_submitted_;
  result->password_save_manager_->Init(result->client_, result->form_fetcher_,
                                       result->metrics_recorder_,
                                       &result->votes_uploader_);
  return result;
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    std::unique_ptr<PasswordForm> saved_form,
    std::unique_ptr<FormFetcher> form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager)
    : PasswordFormManager(client,
                          PasswordFormDigest(*saved_form),
                          form_fetcher.get(),
                          std::move(password_save_manager),
                          nullptr /* metrics_recorder */) {
  parsed_submitted_form_ = std::move(saved_form);
  is_submitted_ = true;
  owned_form_fetcher_ = std::move(form_fetcher);
  owned_form_fetcher_->Fetch();
}

void PasswordFormManager::DelayFillForServerSidePredictions() {
  async_predictions_waiter_.StartTimer();
  server_predictions_closure_ = async_predictions_waiter_.CreateClosure();
}

void PasswordFormManager::OnFetchCompleted() {
  received_stored_credentials_time_ = TimeTicks::Now();

  newly_blocklisted_ = false;
  autofills_left_ = kMaxTimesAutofill;

#if BUILDFLAG(IS_ANDROID)
  absl::optional<PasswordStoreBackendError> backend_error =
      form_fetcher_->GetProfileStoreBackendError();
  if (ShouldShowErrorMessage(backend_error, client_)) {
    // If there is no FormData, this is an http authentication form. We don't
    // show the message for it because it would be hidden behind a sign in
    // dialog and the user could miss it.
    if (observed_form() != nullptr) {
      std::unique_ptr<PasswordForm> password_form =
          parser_.Parse(*observed_form(), FormDataParser::Mode::kFilling,
                        GetStoredUsernames());
      client_->ShowPasswordManagerErrorMessage(
          password_form && (password_form->IsLikelySignupForm() ||
                            password_form->IsLikelyChangePasswordForm() ||
                            password_form->IsLikelyResetPasswordForm())
              ? password_manager::ErrorMessageFlowType::kSaveFlow
              : password_manager::ErrorMessageFlowType::kFillFlow,
          backend_error->type);
    }
  }
#endif

  if (IsCredentialAPISave()) {
    // This is saving with credential API, there is no form to fill, so no
    // filling required.
    return;
  }

  client_->UpdateCredentialCache(url::Origin::Create(GetURL()),
                                 form_fetcher_->GetBestMatches(),
                                 form_fetcher_->IsBlocklisted());

  if (is_submitted_)
    CreatePendingCredentials();

  if (IsHttpAuth()) {
    // No server prediction for http auth, so no need to wait.
    FillHttpAuth();
  } else if (parser_.predictions() ||
             !wait_for_server_predictions_for_filling_) {
    ReportTimeBetweenStoreAndServerUMA();
    FillNow();
  } else if (!async_predictions_waiter_.IsActive()) {
    DelayFillForServerSidePredictions();
  }
}

void PasswordFormManager::OnWaitCompleted() {
  FillNow();
}

void PasswordFormManager::OnTimeout() {
  FillNow();
}

bool PasswordFormManager::WebAuthnCredentialsAvailable() const {
  auto check_credentials_delegate = [=]() {
    WebAuthnCredentialsDelegate* delegate =
        client_->GetWebAuthnCredentialsDelegateForDriver(driver_.get());
    return delegate && delegate->GetPasskeys().has_value();
  };
#if BUILDFLAG(IS_ANDROID)
  auto check_cred_man_delegate = [=]() {
    WebAuthnCredManDelegate* delegate =
        client_->GetWebAuthnCredManDelegateForDriver(driver_.get());
    return delegate &&
           delegate->HasPasskeys() == WebAuthnCredManDelegate::kHasPasskeys;
  };
  switch (WebAuthnCredManDelegate::CredManMode()) {
    case webauthn::WebAuthnCredManDelegate::kNotEnabled:
      return check_credentials_delegate();
    case webauthn::WebAuthnCredManDelegate::kAllCredMan:
      return check_cred_man_delegate();
    case webauthn::WebAuthnCredManDelegate::kNonGpmPasskeys:
      // In this mode, passkeys can exist in WebAuthnCredentialsDelegate or
      // WebAuthnCredManDelegate.
      return check_cred_man_delegate() || check_credentials_delegate();
  }
#else
  return check_credentials_delegate();
#endif  // BUILDFLAG(IS_ANDROID)
}

void PasswordFormManager::CreatePendingCredentials() {
  DCHECK(is_submitted_);
  if (!parsed_submitted_form_)
    return;

  password_save_manager_->CreatePendingCredentials(
      *parsed_submitted_form_, observed_form(), submitted_form_, IsHttpAuth(),
      IsCredentialAPISave());
}

bool PasswordFormManager::ProvisionallySave(
    const FormData& submitted_form,
    const PasswordManagerDriver* driver,
    const base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>*
        possible_usernames) {
  DCHECK(DoesManage(submitted_form.unique_renderer_id, driver));
  DCHECK(client_->IsSavingAndFillingEnabled(submitted_form.url));
  auto [parsed_submitted_form, in_form_username_detection_method] =
      ParseFormAndMakeLogging(submitted_form, FormDataParser::Mode::kSaving);
  RecordMetricOnReadonly(parser_.readonly_status(), !!parsed_submitted_form,
                         FormDataParser::Mode::kSaving);
  if (parsed_submitted_form) {
    metrics_recorder_->CalculateParsingDifferenceOnSavingAndFilling(
        *parsed_submitted_form.get());
  }

  bool have_password_to_save =
      parsed_submitted_form &&
      parsed_submitted_form->HasNonEmptyPasswordValue();
  if (!have_password_to_save) {
    // In case of error during parsing, reset the state.
    parsed_submitted_form_.reset();
    submitted_form_ = FormData();
    password_save_manager_->ResetPendingCredentials();
    is_submitted_ = false;
    return false;
  }

  parsed_submitted_form_ = std::move(parsed_submitted_form);
  submitted_form_ = submitted_form;
  is_submitted_ = true;
  CalculateFillingAssistanceMetric(submitted_form);
  CalculateSubmittedFormFrameMetric();
  CalculateSubmittedFormTypeMetric();
  metrics_recorder_->set_possible_username_used(false);
  votes_uploader_.clear_single_username_votes_data();
  votes_uploader_.set_should_send_username_first_flow_votes(false);

  if (possible_usernames && !possible_usernames->empty()) {
    HandleUsernameFirstFlow(*possible_usernames,
                            in_form_username_detection_method);
  }
  HandleForgotPasswordFormData();

  CreatePendingCredentials();
  return true;
}

bool PasswordFormManager::ProvisionallySaveHttpAuthForm(
    const PasswordForm& submitted_form) {
  if (!IsHttpAuth())
    return false;
  CHECK(observed_digest());
  if (*observed_digest() != PasswordFormDigest(submitted_form)) {
    return false;
  }

  parsed_submitted_form_ = std::make_unique<PasswordForm>(submitted_form);
  is_submitted_ = true;
  CreatePendingCredentials();
  return true;
}

bool PasswordFormManager::IsHttpAuth() const {
  return GetScheme() != PasswordForm::Scheme::kHtml;
}

bool PasswordFormManager::IsCredentialAPISave() const {
  return observed_digest() && !IsHttpAuth();
}

PasswordForm::Scheme PasswordFormManager::GetScheme() const {
  return observed_digest() ? observed_digest()->scheme
                           : PasswordForm::Scheme::kHtml;
}

void PasswordFormManager::ProcessServerPredictions(
    const std::map<FormSignature, FormPredictions>& predictions) {
  if (parser_.predictions()) {
    // This method might be called multiple times. No need to process
    // predictions again.
    return;
  }
  UpdatePredictionsForObservedForm(predictions);
  if (parser_.predictions()) {
    if (!server_predictions_closure_.is_null()) {
      // Signals the availability of server predictions, but there might be
      // other callbacks still outstanding.
      std::move(server_predictions_closure_).Run();
    } else {
      FillNow();
    }
  }
}

void PasswordFormManager::Fill() {
  if (parser_.predictions() || !wait_for_server_predictions_for_filling_) {
    FillNow();
    return;
  }

  // Start a waiter for predictions if there is currently no active one.
  if (!async_predictions_waiter_.IsActive()) {
    DelayFillForServerSidePredictions();
  }
}

void PasswordFormManager::FillNow() {
  if (!driver_)
    return;

  if (form_fetcher_->GetState() == FormFetcher::State::WAITING)
    return;

  if (autofills_left_ <= 0)
    return;
  autofills_left_--;

  // There are additional signals (server-side data) and parse results in
  // filling and saving mode might be different so it is better not to cache
  // parse result, but to parse each time again.
  CHECK(observed_form());
  auto [observed_password_form, username_detection_method] =
      ParseFormAndMakeLogging(*observed_form(), FormDataParser::Mode::kFilling);
  RecordMetricOnReadonly(parser_.readonly_status(), !!observed_password_form,
                         FormDataParser::Mode::kFilling);
  if (!observed_password_form)
    return;
  metrics_recorder_->CacheParsingResultInFillingMode(
      *observed_password_form.get());

  if (observed_password_form->is_new_password_reliable && !IsBlocklisted()) {
    driver_->FormEligibleForGenerationFound({
#if BUILDFLAG(IS_IOS)
      .form_renderer_id = observed_password_form->form_data.unique_renderer_id,
#endif
      .new_password_renderer_id =
          observed_password_form->new_password_element_renderer_id,
      .confirmation_password_renderer_id =
          observed_password_form->confirmation_password_element_renderer_id,
    });
  }

#if BUILDFLAG(IS_IOS)
  // On iOS, filling on username first flow is only supported when the feature
  // is enabled.
  if (observed_password_form->IsSingleUsername() &&
      !base::FeatureList::IsEnabled(
          password_manager::features::kIOSPasswordSignInUff)) {
    return;
  }
#endif

  SendFillInformationToRenderer(
      client_, driver_.get(), *observed_password_form.get(),
      form_fetcher_->GetBestMatches(), form_fetcher_->GetFederatedMatches(),
      form_fetcher_->GetPreferredMatch(), form_fetcher_->IsBlocklisted(),
      metrics_recorder_.get(), WebAuthnCredentialsAvailable());
}

void PasswordFormManager::OnGeneratedPasswordAccepted(
    FormData form_data,
    autofill::FieldRendererId generation_element_id,
    const std::u16string& password) {
  // Find the generating element to update its value. The parser needs a non
  // empty value.
  auto it = base::ranges::find(form_data.fields, generation_element_id,
                               &FormFieldData::unique_renderer_id);
  // The parameters are coming from the renderer and can't be trusted.
  if (it == form_data.fields.end())
    return;
  it->value = password;
  auto [parsed_form, username_detection_method] =
      ParseFormAndMakeLogging(form_data, FormDataParser::Mode::kSaving);
  if (!parsed_form) {
    // Create a password form with a minimum data.
    parsed_form = std::make_unique<PasswordForm>();
    parsed_form->url = form_data.url;
    parsed_form->signon_realm = GetSignonRealm(form_data.url);
  }
  parsed_form->password_value = password;
  password_save_manager_->GeneratedPasswordAccepted(*parsed_form, driver_);
}

bool PasswordFormManager::ObservedFormHasField(int driver_id,
                                               FieldRendererId field_id) const {
  if (driver_id != driver_id_) {
    return false;
  }
  CHECK(observed_form());
  for (const auto& field : observed_form()->fields) {
    if (field.unique_renderer_id == field_id) {
      LogUsingPossibleUsername(client_, /*is_used*/ false, "Same form");
      return true;
    }
  }
  return false;
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    FormOrDigest observed_form_or_digest,
    FormFetcher* form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder)
    : client_(client),
      observed_form_or_digest_(std::move(observed_form_or_digest)),
      metrics_recorder_(metrics_recorder),
      owned_form_fetcher_(form_fetcher
                              ? nullptr
                              : std::make_unique<FormFetcherImpl>(
                                    observed_digest()
                                        ? *observed_digest()
                                        : PasswordFormDigest(*observed_form()),
                                    client_,
                                    true /* should_migrate_http_passwords */)),
      form_fetcher_(form_fetcher ? form_fetcher : owned_form_fetcher_.get()),
      password_save_manager_(std::move(password_save_manager)),
      // TODO(https://crbug.com/831123): set correctly
      // |is_possible_change_password_form| in |votes_uploader_| constructor
      votes_uploader_(client, false /* is_possible_change_password_form */),
      async_predictions_waiter_(this) {
  form_fetcher_->AddConsumer(this);
  if (!metrics_recorder_) {
    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        client_->IsCommittedMainFrameSecure(), client_->GetUkmSourceId(),
        client_->GetPrefs());
  }
  password_save_manager_->Init(client_, form_fetcher_, metrics_recorder_,
                               &votes_uploader_);
  base::UmaHistogramEnumeration("PasswordManager.FormVisited.PerProfileType",
                                client_->GetProfileType());
}

void PasswordFormManager::RecordMetricOnReadonly(
    FormDataParser::ReadonlyPasswordFields readonly_status,
    bool parsing_successful,
    FormDataParser::Mode mode) {
  // The reported value is combined of the |readonly_status| shifted by one bit
  // to the left, and the success bit put in the least significant bit. Note:
  // C++ guarantees that bool->int conversions map false to 0 and true to 1.
  uint64_t value = static_cast<uint64_t>(parsing_successful) +
                   (static_cast<uint64_t>(readonly_status) << 1);
  switch (mode) {
    case FormDataParser::Mode::kSaving:
      metrics_recorder_->RecordReadonlyWhenSaving(value);
      break;
    case FormDataParser::Mode::kFilling:
      metrics_recorder_->RecordReadonlyWhenFilling(value);
      break;
  }
}

void PasswordFormManager::ReportTimeBetweenStoreAndServerUMA() {
  if (!received_stored_credentials_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PasswordManager.TimeBetweenStoreAndServer",
                        TimeTicks::Now() - received_stored_credentials_time_);
  }
}

void PasswordFormManager::FillHttpAuth() {
  DCHECK(IsHttpAuth());
  if (!form_fetcher_->GetPreferredMatch())
    return;
  client_->AutofillHttpAuth(*form_fetcher_->GetPreferredMatch(), this);
}

std::tuple<std::unique_ptr<PasswordForm>,
           FormDataParser::UsernameDetectionMethod>
PasswordFormManager::ParseFormAndMakeLogging(const FormData& form,
                                             FormDataParser::Mode mode) {
  auto [password_form, username_detection_method] =
      parser_.ParseAndReturnUsernameDetection(form, mode, GetStoredUsernames());

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormData(Logger::STRING_FORM_PARSING_INPUT, form);
    if (password_form) {
      logger.LogPasswordForm(Logger::STRING_FORM_PARSING_OUTPUT,
                             *password_form);
    }
  }
  return {std::move(password_form), username_detection_method};
}

void PasswordFormManager::PresaveGeneratedPasswordInternal(
    const FormData& form,
    const std::u16string& generated_password) {
  auto [parsed_form, username_detection_method] =
      ParseFormAndMakeLogging(form, FormDataParser::Mode::kSaving);

  if (!parsed_form) {
    // Create a password form with a minimum data.
    parsed_form = std::make_unique<PasswordForm>();
    parsed_form->url = form.url;
    parsed_form->signon_realm = GetSignonRealm(form.url);
  }
  // Set |password_value| to the generated password in order to ensure that
  // the generated password is saved.
  parsed_form->password_value = generated_password;

  password_save_manager_->PresaveGeneratedPassword(std::move(*parsed_form));
}

void PasswordFormManager::CalculateFillingAssistanceMetric(
    const FormData& submitted_form) {
  std::set<std::pair<std::u16string, PasswordForm::Store>> saved_usernames;
  std::set<std::pair<std::u16string, PasswordForm::Store>> saved_passwords;

  for (auto* saved_form : form_fetcher_->GetNonFederatedMatches()) {
    // Saved credentials might have empty usernames which are not interesting
    // for filling assistance metric.
    if (!saved_form->username_value.empty())
      saved_usernames.emplace(saved_form->username_value, saved_form->in_store);
    saved_passwords.emplace(saved_form->password_value, saved_form->in_store);
  }

  metrics_recorder_->CalculateFillingAssistanceMetric(
      submitted_form, saved_usernames, saved_passwords, IsBlocklisted(),
      form_fetcher_->GetInteractionsStats(),
      client_->GetPasswordFeatureManager()
          ->ComputePasswordAccountStorageUsageLevel());
}

void PasswordFormManager::CalculateSubmittedFormFrameMetric() {
  if (!driver_)
    return;

  const PasswordForm& form = *GetSubmittedForm();
  metrics_util::SubmittedFormFrame frame;
  if (driver_->IsInPrimaryMainFrame()) {
    frame = metrics_util::SubmittedFormFrame::MAIN_FRAME;
  } else if (form.url == client_->GetLastCommittedURL()) {
    frame =
        metrics_util::SubmittedFormFrame::IFRAME_WITH_SAME_URL_AS_MAIN_FRAME;
  } else {
    std::string main_frame_signon_realm =
        GetSignonRealm(client_->GetLastCommittedURL());
    if (main_frame_signon_realm == form.signon_realm) {
      frame = metrics_util::SubmittedFormFrame::
          IFRAME_WITH_DIFFERENT_URL_SAME_SIGNON_REALM_AS_MAIN_FRAME;
    } else if (IsPublicSuffixDomainMatch(form.signon_realm,
                                         main_frame_signon_realm)) {
      frame = metrics_util::SubmittedFormFrame::
          IFRAME_WITH_PSL_MATCHED_SIGNON_REALM;
    } else {
      frame = metrics_util::SubmittedFormFrame::
          IFRAME_WITH_DIFFERENT_AND_NOT_PSL_MATCHED_SIGNON_REALM;
    }
  }
  metrics_recorder_->set_submitted_form_frame(frame);
}

void PasswordFormManager::CalculateSubmittedFormTypeMetric() {
  if (!parsed_submitted_form_) {
    return;
  }
  metrics_util::SubmittedFormType form_type(
      metrics_util::SubmittedFormType::kUndefined);
  if (parsed_submitted_form_->IsLikelyLoginForm()) {
    form_type = metrics_util::SubmittedFormType::kLogin;
  } else if (parsed_submitted_form_->IsLikelySignupForm()) {
    form_type = metrics_util::SubmittedFormType::kSignup;
  } else if (parsed_submitted_form_->IsLikelyChangePasswordForm()) {
    form_type = metrics_util::SubmittedFormType::kChangePassword;
  } else if (parsed_submitted_form_->IsLikelyResetPasswordForm()) {
    form_type = metrics_util::SubmittedFormType::kResetPassword;
  } else if (parsed_submitted_form_->IsSingleUsername()) {
    form_type = metrics_util::SubmittedFormType::kSingleUsername;
  }
  metrics_recorder_->SetSubmittedFormType(form_type);
}

bool PasswordFormManager::IsPossibleSingleUsernameAvailable(
    const PossibleUsernameData& possible_username) const {
  // The username form and password forms signon realms must be the same or
  // an eTLD+1 match.
  // TODO(crbug.com/1470586): Extend to match affiliated domains.
  if (!IsPublicSuffixDomainMatch(possible_username.signon_realm,
                                 parsed_submitted_form_->signon_realm)) {
    LogUsingPossibleUsername(client_, /*is_used*/ false, "Different domains");
    return false;
  }

  if (possible_username.value.empty()) {
    LogUsingPossibleUsername(client_, /*is_used*/ false,
                             "Empty possible username value");
    return false;
  }

  if (possible_username.IsStale()) {
    LogUsingPossibleUsername(client_, /*is_used*/ false,
                             "Possible username data expired");
    return false;
  }

  if (possible_username.is_likely_otp &&
      !possible_username.HasSingleUsernameServerPrediction()) {
    LogUsingPossibleUsername(client_, /*is_used*/ false,
                             "Possible username field is an OTP field");
    return false;
  }

  // The username candidate field should not be in |observed_form()|, otherwise
  // that is a task of FormParser to choose it from |observed_form()|.
  if (ObservedFormHasField(possible_username.driver_id,
                           possible_username.renderer_id)) {
    return false;
  }

  return true;
}

std::optional<UsernameFoundOutsideOfForm>
PasswordFormManager::FindBestPossibleUsernameCandidate(
    const base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>&
        possible_usernames) {
  std::optional<UsernameFoundOutsideOfForm> result = std::nullopt;
  // Search for a candidate among all recently user modified fields.
  // If `kUsernameFirstFlowWithIntermediateValues` feature is off, the most
  // recent user-typed field is picked if available.
  // If `kUsernameFirstFlowWithIntermediateValues` feature is on, look for
  // other field in cache that has a higher priority.
  for (auto [field_identifier, candidate_username] : possible_usernames) {
    if (!IsPossibleSingleUsernameAvailable(candidate_username)) {
      continue;
    }
    // Reassign the best candidate if new candidate has higher priority.
    // Priorities are numbered so that the lowest has priority 0.
    auto [priority, password_form_had_matching_username] =
        GivePriorityToUsernameFoundOutsideOfForm(candidate_username,
                                                 *parsed_submitted_form_.get());
    if (!result.has_value() ||
        (priority > result.value().priority &&
         base::FeatureList::IsEnabled(
             password_manager::features::
                 kUsernameFirstFlowWithIntermediateValues))) {
      result = {priority, password_form_had_matching_username,
                candidate_username};
    }
  }
  return result;
}

void PasswordFormManager::UpdatePredictionsForObservedForm(
    const std::map<FormSignature, FormPredictions>& predictions) {
  CHECK(observed_form());
  FormSignature observed_form_signature =
      CalculateFormSignature(*observed_form());
  auto it = predictions.find(observed_form_signature);
  if (it == predictions.end())
    return;

  ReportTimeBetweenStoreAndServerUMA();
  parser_.set_predictions(it->second);
}

void PasswordFormManager::UpdateFormManagerWithFormChanges(
    const FormData& observed_form_data,
    const std::map<FormSignature, FormPredictions>& predictions) {
  *mutable_observed_form() = observed_form_data;

  // If the observed form has changed, it might be autofilled again.
  async_predictions_waiter_.Reset();
  server_predictions_closure_.Reset();
  autofills_left_ = kMaxTimesAutofill;
  parser_.reset_predictions();
  UpdatePredictionsForObservedForm(predictions);
}

// Best candidate is considered for single username vote in four cases:
// 1) There is a password field and no username field in the current form.
// 2) There are both password and username fields, and the username
// value matches the username value (`possible_username`) in the single
// username form.
// 3) There is a username field outside of the password form that is a
// server override.
// 4) Username field outside of the password form has a server prediction,
// while username in the password form was found using client-side
// heuristics.
// If no case is suitable, don't consider for single username vote.
bool PasswordFormManager::ShouldPreferUsernameFoundOutsideOfForm(
    const std::optional<UsernameFoundOutsideOfForm>& best_candidate,
    FormDataParser::UsernameDetectionMethod in_form_username_detection_method) {
  if (IsPasswordFormWithoutUsername(
          parsed_submitted_form_.get())) {  // Case (1).
    // Don't check for best candidate. If it is empty, vote for fallback
    // classifier must be sent.
    return true;
  }
  if (best_candidate.has_value()) {
    if (best_candidate.value()
            .password_form_had_matching_username) {  // Case (2).
      return true;
    }
    if (UsernameOutsideOfFormHasHigherPriority(
            best_candidate.value().priority,
            in_form_username_detection_method)) {  // Case (3) & (4).
      return true;
    }
  }
  return false;
}

void PasswordFormManager::HandleUsernameFirstFlow(
    const base::LRUCache<PossibleUsernameFieldIdentifier, PossibleUsernameData>&
        possible_usernames,
    FormDataParser::UsernameDetectionMethod in_form_username_detection_method) {
  std::optional<UsernameFoundOutsideOfForm> best_candidate =
      FindBestPossibleUsernameCandidate(possible_usernames);
  bool should_prefer_username_found_outside_of_form =
      ShouldPreferUsernameFoundOutsideOfForm(best_candidate,
                                             in_form_username_detection_method);
  votes_uploader_.set_should_send_username_first_flow_votes(
      should_prefer_username_found_outside_of_form);

  if (!best_candidate.has_value()) {
    // Happens when there is no username field in the password form as well.
    // If no single username typing preceded single password typing, set
    // empty single username vote data for the fallback classifier.
    votes_uploader_.add_single_username_vote_data(SingleUsernameVoteData());
    return;
  }

  const UsernameFoundOutsideOfForm& picked_username = best_candidate.value();
  if (base::FeatureList::IsEnabled(
          features::kUsernameFirstFlowWithIntermediateValuesVoting)) {
    // Cache voting data for all candidates outside of the password form.
    // Will send votes only if `should_prefer_username_found_outside_of_form` is
    // true or there is an `IN_FORM_OVERRULE` vote among any of them.
    for (const auto& it : possible_usernames) {
      votes_uploader_.add_single_username_vote_data(SingleUsernameVoteData(
          it.second.renderer_id, it.second.value,
          it.second.form_predictions.value_or(FormPredictions()),
          form_fetcher_->GetBestMatches(),
          FormMatchesUsername(*parsed_submitted_form_.get(), it.second.value)));
    }
  } else {
    // Cache voting data for the best possible username candidate user modified
    // field.
    votes_uploader_.add_single_username_vote_data(SingleUsernameVoteData(
        picked_username.data.renderer_id, picked_username.data.value,
        picked_username.data.form_predictions.value_or(FormPredictions()),
        form_fetcher_->GetBestMatches(),
        picked_username.password_form_had_matching_username));
  }

  if (!should_prefer_username_found_outside_of_form) {
    return;
  }

  // Suggest the possible username value in a prompt in three cases:
  // (1) If single username field is a server override.
  // (2) If the server prediction tells that it is a single username field and
  // there is no `USERNAME` server prediction inside the password form.
  // (3) If the field has autocomplete = "username" attribute (used only if
  // there are no server predictions, which lets us override the attribute).
  // Otherwise, |possible_username| is used only for voting.
  switch (picked_username.priority) {
    case UsernameFoundOutsideOfFormType::kSingleUsernamePrediction:
    case UsernameFoundOutsideOfFormType::kSingleUsernameOverride:
      // Case (1) & (2).
      LogUsingPossibleUsername(
          client_, /*is_used=*/true,
          "Single username predicted by the server, "
          "retrieved from PossibleUsernameData, populated in prompt");
      break;
    case UsernameFoundOutsideOfFormType::kUsernameAutocomplete:
      // Case (3).
      LogUsingPossibleUsername(
          client_, /*is_used=*/true,
          "Single username by autocomplete attribute, "
          "retrieved from PossibleUsernameData, populated in prompt");
      break;
    case UsernameFoundOutsideOfFormType::kMatchingUsername:
      // Password prompt is already populated with the same value.
      LogUsingPossibleUsername(
          client_, /*is_used=*/true,
          "Single username matches username found in the PasswordForm, "
          "already populated in prompt");
      break;
    case UsernameFoundOutsideOfFormType::kUserModifiedTextField:
      // None of the above, doesn't change prompt and used only for voting.
      LogUsingPossibleUsername(
          client_, /*is_used=*/true,
          "Single username by local heuristics, "
          "retrieved from PossibleUsernameData, not populated in prompt");
      // Return early since the value is not used in the prompt.
      return;
  }
  if (!picked_username.password_form_had_matching_username) {
    SetUsernameValueFromOutsideOfForm(picked_username.data.value,
                                      *parsed_submitted_form_.get());
  }
  metrics_recorder_->set_possible_username_used(true);
}

void PasswordFormManager::HandleForgotPasswordFormData() {
  FieldInfoManager* field_info_manager = client_->GetFieldInfoManager();
  // FieldInfoManager may be null in incognito and tests.
  if (!field_info_manager) {
    return;
  }

  std::vector<FieldInfo> field_info =
      field_info_manager->GetFieldInfo(parsed_submitted_form_->signon_realm);
  // No info available for the current eTLD => no voting on potential username
  // forms.
  if (field_info.empty() &&
      IsPasswordFormWithoutUsername(parsed_submitted_form_.get())) {
    // Set empty vote data for the fallback classifier.
    votes_uploader_.AddForgotPasswordVoteData(SingleUsernameVoteData());
    return;
  }

  // Iterated over text fields that the user has interacted with recently to
  // find possible username fields.
  for (const auto& field : field_info) {
    // Skip the field if any of the following is true:
    // (1) The user has erased the value.
    // (2) The field is a part of the observed password form.
    // (3) PasswordManager's heuristics suggest it's an OTP field, and there
    // is no serverside data supporting that it's a single username field.
    if (field.value.empty() ||                                    // Case (1).
        ObservedFormHasField(field.driver_id, field.field_id) ||  // Case (2).
        (field.is_likely_otp &&
         !IsSingleUsernameType(field.type))) {  // Case (3).
      continue;
    }

    PasswordFormHadMatchingUsername password_form_had_matching_username =
        FormMatchesUsername(*parsed_submitted_form_.get(), field.value);
    // Consider possible username field for voting if either:
    // 1) A password form without a username was submitted after the single
    // username form. 2) The submitted password form contains the potential
    // username.
    // TODO: crbug.com/1468297 - The distinction between (1) and (2), i.e.
    // 'password_form_had_matching_username', is used only to assess the impact
    // of (2) with metrics. The variable can be removed once the metrics are not
    // needed anymore.
    if (IsPasswordFormWithoutUsername(
            parsed_submitted_form_.get()) ||    // Case 1.
        password_form_had_matching_username) {  // Case 2.
      votes_uploader_.AddForgotPasswordVoteData(SingleUsernameVoteData(
          field.field_id, field.value,
          field.stored_predictions.value_or(FormPredictions()),
          form_fetcher_->GetBestMatches(),
          password_form_had_matching_username));

      if (password_manager_util::IsSingleUsernameType(field.type)) {
        SetUsernameValueFromOutsideOfForm(field.value,
                                          *parsed_submitted_form_.get());
        LogUsingPossibleUsername(client_, /*is_used=*/true,
                                 "Single username predicted by the server, "
                                 "retrieved from FieldInfoManager, populated "
                                 "in prompt");
      }
    }
  }
}

// Returns bit masks with differences in forms attributes which are important
// for parsing. Bits are set according to enum FormDataDifferences.
bool HasObservedFormChanged(const FormData& form_data,
                            PasswordFormManager& form_manager) {
  CHECK(form_manager.observed_form());
  const FormData& lhs = form_data;
  const FormData& rhs = *form_manager.observed_form();

  if (lhs.fields.size() != rhs.fields.size()) {
    form_manager.GetMetricsRecorder()->RecordFormChangeBitmask(
        PasswordFormMetricsRecorder::kFieldsNumber);
    return true;
  }
  size_t differences_bitmask = 0;
  for (size_t i = 0; i < lhs.fields.size(); ++i) {
    const FormFieldData& lhs_field = lhs.fields[i];
    const FormFieldData& rhs_field = rhs.fields[i];

    if (lhs_field.unique_renderer_id != rhs_field.unique_renderer_id) {
      differences_bitmask |= PasswordFormMetricsRecorder::kRendererFieldIDs;
    }

    if (lhs_field.form_control_type != rhs_field.form_control_type) {
      differences_bitmask |= PasswordFormMetricsRecorder::kFormControlTypes;
    }

    if (lhs_field.autocomplete_attribute != rhs_field.autocomplete_attribute) {
      differences_bitmask |=
          PasswordFormMetricsRecorder::kAutocompleteAttributes;
    }

    if (lhs_field.name != rhs_field.name) {
      differences_bitmask |= PasswordFormMetricsRecorder::kFormFieldNames;
    }
  }

  form_manager.GetMetricsRecorder()->RecordFormChangeBitmask(
      differences_bitmask);
  return differences_bitmask != 0;
}

base::flat_set<std::u16string> PasswordFormManager::GetStoredUsernames() const {
  base::flat_set<std::u16string> stored_usernames =
      base::MakeFlatSet<std::u16string>(
          GetBestMatches(), {}, [](const PasswordForm* password_form) {
            return base::i18n::ToLower(password_form->username_value);
          });
  if (stored_usernames.contains(u"")) {
    stored_usernames.erase(u"");
  }
  return stored_usernames;
}

}  // namespace password_manager
