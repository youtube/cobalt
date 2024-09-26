// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_form_prediction_waiter.h"
#include "components/password_manager/core/browser/password_save_manager.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {

class PasswordFormMetricsRecorder;
class PasswordManagerClient;
class PasswordManagerDriver;
struct PossibleUsernameData;

// This class helps with filling the observed form and with saving/updating the
// stored information about it.
class PasswordFormManager : public PasswordFormManagerForUI,
                            public PasswordFormPredictionWaiter::Client,
                            public FormFetcher::Consumer {
 public:
  // TODO(crbug.com/621355): So far, |form_fetcher| can be null. In that case
  // |this| creates an instance of it itself (meant for production code). Once
  // the fetcher is shared between PasswordFormManager instances, it will be
  // required that |form_fetcher| is not null. |form_saver| is used to
  // save/update the form. |metrics_recorder| records metrics for |*this|. If
  // null a new instance will be created.
  PasswordFormManager(
      PasswordManagerClient* client,
      const base::WeakPtr<PasswordManagerDriver>& driver,
      const autofill::FormData& observed_form_data,
      FormFetcher* form_fetcher,
      std::unique_ptr<PasswordSaveManager> password_save_manager,
      scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder);

  // Constructor for http authentication (aka basic authentication).
  PasswordFormManager(
      PasswordManagerClient* client,
      PasswordFormDigest observed_http_auth_digest,
      FormFetcher* form_fetcher,
      std::unique_ptr<PasswordSaveManager> password_save_manager);

  PasswordFormManager(const PasswordFormManager&) = delete;
  PasswordFormManager& operator=(const PasswordFormManager&) = delete;

  ~PasswordFormManager() override;

  // The upper limit on how many times Chrome will try to autofill the same
  // form.
  static constexpr int kMaxTimesAutofill = 5;

  // Returns whether the form identified by |form_renderer_id| and |driver|
  // is managed by this password form manager.
  bool DoesManage(autofill::FormRendererId form_renderer_id,
                  const PasswordManagerDriver* driver) const;

  // Check that |submitted_form_| is equal to |form| from the user point of
  // view. It is used for detecting that a form is reappeared after navigation
  // for success detection.
  bool IsEqualToSubmittedForm(const autofill::FormData& form) const;

  // If |submitted_form| is managed by *this (i.e. DoesManage returns true for
  // |submitted_form| and |driver|) then saves |submitted_form| to
  // |submitted_form_| field, sets |is_submitted| = true and returns true.
  // Otherwise returns false.
  // If as a result of the parsing the username is not found, the
  // |possible_username->value| is chosen as username if it looks like an
  // username and came from the same domain as |submitted_form|.
  // If |possible_username->value| matches username field value, try sending
  // single username vote to the form |possible_username| belongs to.
  bool ProvisionallySave(const autofill::FormData& submitted_form,
                         const PasswordManagerDriver* driver,
                         const PossibleUsernameData* possible_username);

  // If |submitted_form| is managed by *this then saves |submitted_form| to
  // |submitted_form_| field, sets |is_submitted| = true and returns true.
  // Otherwise returns false.
  bool ProvisionallySaveHttpAuthForm(const PasswordForm& submitted_form);

  bool is_submitted() { return is_submitted_; }
  void set_not_submitted() { is_submitted_ = false; }

  // Returns true if |*this| manages http authentication.
  bool IsHttpAuth() const;

  // Returns true if |*this| manages saving with Credentials API. This class is
  // not used for filling with Credentials API.
  bool IsCredentialAPISave() const;

  // Returns scheme of the observed form or http authentication.
  PasswordForm::Scheme GetScheme() const;

  // Selects from |predictions| predictions that corresponds to
  // |observed_form()|, initiates filling and stores predictions in
  // |predictions_|.
  void ProcessServerPredictions(
      const std::map<autofill::FormSignature, FormPredictions>& predictions);

  // Sends fill data to the renderer.
  void Fill();

  // Sends fill data to the renderer to fill |observed_form_data| using
  // new relevant data from |predictions|.
  void FillForm(
      const autofill::FormData& observed_form_data,
      const std::map<autofill::FormSignature, FormPredictions>& predictions);

  void UpdateSubmissionIndicatorEvent(
      autofill::mojom::SubmissionIndicatorEvent event);

  // Sends the request to prefill the generated password or pops up an
  // additional UI in case of possible override.
  void OnGeneratedPasswordAccepted(
      autofill::FormData form_data,
      autofill::FieldRendererId generation_element_id,
      const std::u16string& password);

  // Sets |was_unblocklisted_while_on_page| to true.
  void MarkWasUnblocklisted();

  // Check if the |possible_username| field is present in the |observed_form()|.
  bool FormHasPossibleUsername(
      const PossibleUsernameData* possible_username) const;

  // PasswordFormManagerForUI:
  const GURL& GetURL() const override;
  const std::vector<const PasswordForm*>& GetBestMatches() const override;
  std::vector<const PasswordForm*> GetFederatedMatches() const override;
  const PasswordForm& GetPendingCredentials() const override;
  metrics_util::CredentialSourceType GetCredentialSource() const override;
  PasswordFormMetricsRecorder* GetMetricsRecorder() override;
  base::span<const InteractionsStats> GetInteractionsStats() const override;
  std::vector<const PasswordForm*> GetInsecureCredentials() const override;
  bool IsBlocklisted() const override;
  bool WasUnblocklisted() const override;
  bool IsMovableToAccountStore() const override;

  void Save() override;
  void Update(const PasswordForm& credentials_to_update) override;
  void OnUpdateUsernameFromPrompt(const std::u16string& new_username) override;
  void OnUpdatePasswordFromPrompt(const std::u16string& new_password) override;

  void OnNopeUpdateClicked() override;
  void OnNeverClicked() override;
  void OnNoInteraction(bool is_update) override;
  void Blocklist() override;
  void OnPasswordsRevealed() override;
  void MoveCredentialsToAccountStore() override;
  void BlockMovingCredentialsToAccountStore() override;

  bool IsNewLogin() const;
  FormFetcher* GetFormFetcher();
  bool IsPendingCredentialsPublicSuffixMatch() const;
  void PresaveGeneratedPassword(const autofill::FormData& form_data,
                                const std::u16string& generated_password);
  void PasswordNoLongerGenerated();
  bool HasGeneratedPassword() const;
  void SetGenerationPopupWasShown(
      autofill::password_generation::PasswordGenerationType type);
  void SetGenerationElement(autofill::FieldRendererId generation_element);
  bool HasLikelyChangePasswordFormSubmitted() const;
  bool IsPasswordUpdate() const;
  base::WeakPtr<PasswordManagerDriver> GetDriver() const;
  const PasswordForm* GetSubmittedForm() const;

  // Returns the frame id of the corresponding PasswordManagerDriver. See
  // `GetFrameId()` in PasswordManagerDriver for more details.
  int GetFrameId();

#if BUILDFLAG(IS_IOS)
  // Sets a value of the field with |field_identifier| of |observed_form()|
  // to |field_value|. In case if there is a presaved credential this function
  // updates the presaved credential.
  void UpdateStateOnUserInput(autofill::FormRendererId form_id,
                              autofill::FieldRendererId field_id,
                              const std::u16string& field_value);

  void SetDriver(const base::WeakPtr<PasswordManagerDriver>& driver);

  // Copies all known field data from FieldDataManager to |observed_form()|
  // and provisionally saves the manager if the relevant data is found.
  void ProvisionallySaveFieldDataManagerInfo(
      const autofill::FieldDataManager& field_data_manager,
      const PasswordManagerDriver* driver);
#endif  // BUILDFLAG(IS_IOS)

  // Create a copy of |*this| which can be passed to the code handling
  // save-password related UI. This omits some parts of the internal data, so
  // the result is not identical to the original.
  // TODO(crbug.com/739366): Replace with translating one appropriate class into
  // another one.
  std::unique_ptr<PasswordFormManager> Clone();

  // Because of the android integration tests, it can't be guarded by if
  // defined(UNIT_TEST).
  static void DisableFillingServerPredictionsForTesting() {
    wait_for_server_predictions_for_filling_ = false;
  }

  // Returns a pointer to the observed form if possible or nullptr otherwise.
  const autofill::FormData* observed_form() const {
    return absl::get_if<autofill::FormData>(&observed_form_or_digest_);
  }

  // Saves username value from |pending_credentials_| to votes uploader.
  void SaveSuggestedUsernameValueToVotesUploader();

#if defined(UNIT_TEST)
  static void set_wait_for_server_predictions_for_filling(bool value) {
    wait_for_server_predictions_for_filling_ = value;
  }

  FormSaver* profile_store_form_saver() const {
    return password_save_manager_->GetProfileStoreFormSaverForTesting();
  }

  const VotesUploader& votes_uploader() const { return votes_uploader_; }
#endif

 protected:
  // Constructor for Credentials API.
  PasswordFormManager(
      PasswordManagerClient* client,
      std::unique_ptr<PasswordForm> saved_form,
      std::unique_ptr<FormFetcher> form_fetcher,
      std::unique_ptr<PasswordSaveManager> password_save_manager);

  // FormFetcher::Consumer:
  void OnFetchCompleted() override;

  // PasswordFormPredictionWaiter::Client:
  void OnWaitCompleted() override;
  void OnTimeout() override;

  // Create pending credentials from |parsed_submitted_form_| and forms received
  // from the password store.
  void CreatePendingCredentials();

 private:
  using FormOrDigest = absl::variant<autofill::FormData, PasswordFormDigest>;

  // Delegating constructor.
  PasswordFormManager(
      PasswordManagerClient* client,
      FormOrDigest observed_form_or_digest,
      FormFetcher* form_fetcher,
      std::unique_ptr<PasswordSaveManager> password_save_manager,
      scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder);
  // Records the status of readonly fields during parsing, combined with the
  // overall success of the parsing. It reports through two different metrics,
  // depending on whether |mode| indicates parsing for saving or filling.
  void RecordMetricOnReadonly(
      FormDataParser::ReadonlyPasswordFields readonly_status,
      bool parsing_successful,
      FormDataParser::Mode mode);

  // Report the time between receiving credentials from the password store and
  // the autofill server responding to the lookup request.
  void ReportTimeBetweenStoreAndServerUMA();

  // Sends fill data to the http auth popup.
  void FillHttpAuth();

  // Helper function for calling form parsing and logging results if logging is
  // active.
  std::unique_ptr<PasswordForm> ParseFormAndMakeLogging(
      const autofill::FormData& form,
      FormDataParser::Mode mode);

  void PresaveGeneratedPasswordInternal(
      const autofill::FormData& form,
      const std::u16string& generated_password);

  // Returns a mutable pointer to the observed form if possible or nullptr
  // otherwise.
  autofill::FormData* mutable_observed_form() {
    return absl::get_if<autofill::FormData>(&observed_form_or_digest_);
  }

  // Returns a pointer to the observed digest if possible or nullptr otherwise.
  const PasswordFormDigest* observed_digest() const {
    return absl::get_if<PasswordFormDigest>(&observed_form_or_digest_);
  }

  // Calculates FillingAssistance metric for |submitted_form|. The metric is
  // recorded in case when the successful submission is detected.
  void CalculateFillingAssistanceMetric(
      const autofill::FormData& submitted_form);

  // Calculates SubmittedPasswordFormFrame metric value (main frame, iframe,
  // etc) for |submitted_form|. The metric is recorded when the form manager is
  // destroyed.
  void CalculateSubmittedFormFrameMetric();

  // Save/update |pending_credentials_| to the password store.
  void SavePendingToStore(bool update);

  PasswordFormDigest ConstructObservedFormDigest() const;

  // Returns whether |possible_username| data can be used in username first
  // flow.
  bool IsPossibleSingleUsernameAvailable(
      const PossibleUsernameData* possible_username) const;

  // Returns true if the form is a candidate to send single username vote.
  // Given that function returns true, |password_form_had_username| is an
  // output parameter indicating whether password form had same username value
  // as single username form.
  bool IsPasswordFormAfterSingleUsernameForm(
      const PossibleUsernameData* possible_username,
      bool& password_form_had_username);

  // Updates the predictions stored in |parser_| with predictions relevant for
  // |observed_form_or_digest_|.
  void UpdatePredictionsForObservedForm(
      const std::map<autofill::FormSignature, FormPredictions>& predictions);

  // Updates |observed_form_or_digest_| and form predictions stored in
  // |parser_| and resets the amount of autofills left.
  void UpdateFormManagerWithFormChanges(
      const autofill::FormData& observed_form_data,
      const std::map<autofill::FormSignature, FormPredictions>& predictions);

  // Sets the timer on |async_predictions_waiter_| while waiting for
  // server-side predictions.
  void DelayFillForServerSidePredictions();

  // Returns true if WebAuthn credential filling is enabled and there are
  // credentials available to use.
  bool WebAuthnCredentialsAvailable() const;

  // The client which implements embedder-specific PasswordManager operations.
  raw_ptr<PasswordManagerClient, DanglingUntriaged> client_;

  base::WeakPtr<PasswordManagerDriver> driver_;

  // The frame id of |driver_|.  See `GetFrameId()` in PasswordManagerDriver for
  // more details. This is cached since |driver_| might become null when the
  // frame is deleted.
  int cached_driver_frame_id_ = 0;

  // The id of |driver_|. Cached since |driver_| might become null when the
  // frame frame is deleted.
  int driver_id_ = 0;

  // The observed form or digest. These are mutually exclusive, hence the usage
  // of a variant.
  FormOrDigest observed_form_or_digest_;

  // If the observed form gets blocklisted through |this|, we keep the
  // information in this boolean flag until data is potentially refreshed by
  // reading from PasswordStore again. Upon reading from the store again, we set
  // this boolean to false again.
  bool newly_blocklisted_ = false;

  // Set to true when the user unblocklists the origin while on the page.
  // This is used to decide when to record
  // |PasswordManager.ResultOfSavingAfterUnblacklisting|.
  bool was_unblocklisted_while_on_page_ = false;

  // Takes care of recording metrics and events for |*this|.
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;

  // When not null, then this is the object which |form_fetcher_| points to.
  std::unique_ptr<FormFetcher> owned_form_fetcher_;

  // FormFetcher instance which owns the login data from PasswordStore.
  raw_ptr<FormFetcher, DanglingUntriaged> form_fetcher_;

  std::unique_ptr<PasswordSaveManager> password_save_manager_;

  VotesUploader votes_uploader_;

  // |is_submitted_| = true means that |*this| is ready for saving.
  // TODO(https://crubg.com/875768): Come up with a better name.
  bool is_submitted_ = false;
  autofill::FormData submitted_form_;
  std::unique_ptr<PasswordForm> parsed_submitted_form_;

  // If Chrome has already autofilled a few times, it is probable that autofill
  // is triggered by programmatic changes in the page. We set a maximum number
  // of times that Chrome will autofill to avoid being stuck in an infinite
  // loop.
  int autofills_left_ = kMaxTimesAutofill;

  // True until server predictions received or waiting for them timed out.
  bool waiting_for_server_predictions_ = false;

  // Closure to call when server predictions are received.
  base::OnceClosure server_predictions_closure_;

  // Controls whether to wait or not server before filling. It is used in tests.
  static bool wait_for_server_predictions_for_filling_;

  // Time when stored credentials are received from the store. Used for metrics.
  base::TimeTicks received_stored_credentials_time_;

  PasswordFormPredictionWaiter async_predictions_waiter_;

  // Used to transform FormData into PasswordForms.
  FormDataParser parser_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
