// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_save_manager.h"

namespace password_manager {

class PasswordGenerationManager;

enum class PendingCredentialsState {
  NONE,
  NEW_LOGIN,
  UPDATE,
  AUTOMATIC_SAVE,
  EQUAL_TO_SAVED_MATCH
};

class PasswordSaveManagerImpl : public PasswordSaveManager {
 public:
  PasswordSaveManagerImpl(std::unique_ptr<FormSaver> profile_form_saver,
                          std::unique_ptr<FormSaver> account_form_saver);
  // Convenience constructor that builds FormSavers corresponding to the
  // profile and (if it exists) account store grabbed from |client|.
  explicit PasswordSaveManagerImpl(const PasswordManagerClient* client);
  ~PasswordSaveManagerImpl() override;

  const PasswordForm& GetPendingCredentials() const override;
  const std::u16string& GetGeneratedPassword() const override;
  FormSaver* GetProfileStoreFormSaverForTesting() const override;

  // |metrics_recorder| and |votes_uploader| can both be nullptr.
  void Init(PasswordManagerClient* client,
            const FormFetcher* form_fetcher,
            scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
            VotesUploader* votes_uploader) override;

  // Create pending credentials from |parsed_submitted_form|, |observed_form|
  // and |submitted_form|.
  void CreatePendingCredentials(const PasswordForm& parsed_submitted_form,
                                const autofill::FormData* observed_form,
                                const autofill::FormData& submitted_form,
                                bool is_http_auth,
                                bool is_credential_api_save) override;

  void ResetPendingCredentials() override;

  void Save(const autofill::FormData* observed_form,
            const PasswordForm& parsed_submitted_form) override;

  void Update(const PasswordForm& credentials_to_update,
              const autofill::FormData* observed_form,
              const PasswordForm& parsed_submitted_form) override;

  void Blocklist(const PasswordFormDigest& form_digest) override;
  void Unblocklist(const PasswordFormDigest& form_digest) override;

  // Called when generated password is accepted or changed by user.
  void PresaveGeneratedPassword(PasswordForm parsed_form) override;

  // Called when user wants to start generation flow for |generated|.
  void GeneratedPasswordAccepted(
      PasswordForm parsed_form,
      base::WeakPtr<PasswordManagerDriver> driver) override;

  // Signals that the user cancels password generation.
  void PasswordNoLongerGenerated() override;

  void MoveCredentialsToAccountStore(
      metrics_util::MoveToAccountStoreTrigger) override;

  void BlockMovingToAccountStoreFor(
      const autofill::GaiaIdHash& gaia_id_hash) override;

  void UpdateSubmissionIndicatorEvent(
      autofill::mojom::SubmissionIndicatorEvent event) override;

  bool IsNewLogin() const override;
  bool IsPasswordUpdate() const override;
  bool HasGeneratedPassword() const override;

  void UsernameUpdatedInBubble() override;

  std::unique_ptr<PasswordSaveManager> Clone() override;

 protected:
  static PendingCredentialsState ComputePendingCredentialsState(
      const PasswordForm& parsed_submitted_form,
      const PasswordForm* similar_saved_form);
  static PasswordForm BuildPendingCredentials(
      PendingCredentialsState pending_credentials_state,
      const PasswordForm& parsed_submitted_form,
      const autofill::FormData* observed_form,
      const autofill::FormData& submitted_form,
      const absl::optional<std::u16string>& generated_password,
      bool is_http_auth,
      bool is_credential_api_save,
      const PasswordForm* similar_saved_form);

  virtual std::pair<const PasswordForm*, PendingCredentialsState>
  FindSimilarSavedFormAndComputeState(
      const PasswordForm& parsed_submitted_form) const;

  // Returns the form_saver to be used for generated passwords. Subclasses will
  // override this method to provide different logic for get the form saver.
  virtual FormSaver* GetFormSaverForGeneration();

  // Returns the forms in |matches| that should be taken into account for
  // conflict resolution during generation. Will be overridden in subclasses.
  virtual std::vector<const PasswordForm*> GetRelevantMatchesForGeneration(
      const std::vector<const PasswordForm*>& matches);

  virtual void SavePendingToStoreImpl(
      const PasswordForm& parsed_submitted_form);

  // Clones the current object into |clone|. |clone| must not be null.
  void CloneInto(PasswordSaveManagerImpl* clone);

  // FormSaver instances for all tasks related to storing credentials - one
  // for the profile store, one for the account store.
  const std::unique_ptr<FormSaver> profile_store_form_saver_;
  // May be null on platforms that don't support the account store.
  const std::unique_ptr<FormSaver> account_store_form_saver_;

  // The client which implements embedder-specific PasswordManager operations.
  raw_ptr<PasswordManagerClient, DanglingUntriaged> client_;

  // Stores updated credentials when the form was submitted but success is still
  // unknown. This variable contains credentials that are ready to be written
  // (saved or updated) to a password store. It is calculated based on
  // |submitted_form_| and |best_matches_|.
  PasswordForm pending_credentials_;

  PendingCredentialsState pending_credentials_state_ =
      PendingCredentialsState::NONE;

  // FormFetcher instance which owns the login data from PasswordStore.
  raw_ptr<const FormFetcher, DanglingUntriaged> form_fetcher_;

 private:
  struct PendingCredentialsStates {
    PendingCredentialsState profile_store_state = PendingCredentialsState::NONE;
    PendingCredentialsState account_store_state = PendingCredentialsState::NONE;

    raw_ptr<const PasswordForm> similar_saved_form_from_profile_store = nullptr;
    raw_ptr<const PasswordForm> similar_saved_form_from_account_store = nullptr;
  };
  static PendingCredentialsStates ComputePendingCredentialsStates(
      const PasswordForm& parsed_submitted_form,
      const std::vector<const PasswordForm*>& matches,
      bool username_updated_in_bubble = false);

  std::u16string GetOldPassword(
      const PasswordForm& parsed_submitted_form) const;

  void SetVotesAndRecordMetricsForPendingCredentials(
      const PasswordForm& parsed_submitted_form);

  // Save/update |pending_credentials_| to the password store.
  void SavePendingToStore(const autofill::FormData* observed_form,
                          const PasswordForm& parsed_submitted_form);

  // This sends needed signals to the autofill server, and also triggers some
  // UMA reporting.
  void UploadVotesAndMetrics(const autofill::FormData* observed_form,
                             const PasswordForm& parsed_submitted_form);

  bool IsOptedInForAccountStorage() const;
  bool AccountStoreIsDefault() const;
  bool ShouldStoreGeneratedPasswordsInAccountStore() const;

  // Handles the user flows related to the generation.
  std::unique_ptr<PasswordGenerationManager> generation_manager_;

  // Takes care of recording metrics and events for |*this|. Can be nullptr.
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;

  // Can be nullptr.
  raw_ptr<VotesUploader, DanglingUntriaged> votes_uploader_;

  // True if the user edited the username field during the save prompt.
  bool username_updated_in_bubble_ = false;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_IMPL_H_
