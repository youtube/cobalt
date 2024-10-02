// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom-forward.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_credential_manager.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_feature_manager_impl.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_client_helper.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_store_backend_error.h"
#include "components/prefs/pref_member.h"
#include "components/safe_browsing/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/generated_password_saved_message_delegate.h"
#include "chrome/browser/password_manager/android/password_manager_error_message_delegate.h"
#include "chrome/browser/password_manager/android/password_manager_error_message_helper_bridge_impl.h"
#include "chrome/browser/password_manager/android/save_update_password_message_delegate.h"
#include "components/password_manager/core/browser/credential_cache.h"

class PasswordAccessoryController;
class TouchToFillController;
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/password_manager/multi_profile_credentials_filter.h"
#include "chrome/browser/ui/passwords/account_storage_auth_helper.h"
#else
#include "components/password_manager/core/browser/sync_credentials_filter.h"
#endif

class PasswordGenerationPopupObserver;
class PasswordGenerationPopupControllerImpl;
class Profile;

namespace autofill {
class LogManager;
class RoutingLogManager;

namespace password_generation {
struct PasswordGenerationUIData;
}  // namespace password_generation
}  // namespace autofill

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace device_reauth {
class DeviceAuthenticator;
}

namespace password_manager {
class WebAuthnCredentialsDelegate;
}

// ChromePasswordManagerClient implements the PasswordManagerClient interface.
class ChromePasswordManagerClient
    : public password_manager::PasswordManagerClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromePasswordManagerClient>,
      public autofill::mojom::PasswordGenerationDriver {
 public:
  static void CreateForWebContentsWithAutofillClient(
      content::WebContents* contents,
      autofill::AutofillClient* autofill_client);
  static void BindPasswordGenerationDriver(
      mojo::PendingAssociatedReceiver<autofill::mojom::PasswordGenerationDriver>
          receiver,
      content::RenderFrameHost* rfh);

  ChromePasswordManagerClient(const ChromePasswordManagerClient&) = delete;
  ChromePasswordManagerClient& operator=(const ChromePasswordManagerClient&) =
      delete;

  ~ChromePasswordManagerClient() override;

  // PasswordManagerClient implementation.
  bool IsSavingAndFillingEnabled(const GURL& url) const override;
  bool IsFillingEnabled(const GURL& url) const override;
  bool IsAutoSignInEnabled() const override;
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool is_update) override;
  void PromptUserToMovePasswordToAccount(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move)
      override;
  void ShowManualFallbackForSaving(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool is_update) override;
  void HideManualFallbackForSaving() override;
  void FocusedInputChanged(
      password_manager::PasswordManagerDriver* driver,
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin,
      CredentialsCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  void ShowPasswordManagerErrorMessage(
      password_manager::ErrorMessageFlowType flow_type,
      password_manager::PasswordStoreBackendErrorType error_type) override;

  void ShowTouchToFill(
      password_manager::PasswordManagerDriver* driver,
      autofill::mojom::SubmissionReadinessState submission_readiness) override;
#endif

  // Returns a pointer to the DeviceAuthenticator which is created on demand.
  // This is currently only implemented for Android, Mac and Windows. On all
  // other platforms this will always be null.
  scoped_refptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      override;
  void GeneratePassword(
      autofill::password_generation::PasswordGenerationType type) override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<password_manager::PasswordForm> form) override;
  void NotifySuccessfulLoginWithExistingPassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          submitted_manager) override;
  void NotifyStorePasswordCalled() override;
#if BUILDFLAG(IS_ANDROID)
  void NotifyOnSuccessfulLogin(
      const std::u16string& submitted_username) override;
  void StartSubmissionTrackingAfterTouchToFill(
      const std::u16string& filled_username) override;
  void ResetSubmissionTrackingAfterTouchToFill() override;
#endif
  void UpdateCredentialCache(
      const url::Origin& origin,
      const std::vector<const password_manager::PasswordForm*>& best_matches,
      bool is_blocklisted) override;
  void AutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          saved_form_manager) override;
  void PasswordWasAutofilled(
      const std::vector<const password_manager::PasswordForm*>& best_matches,
      const url::Origin& origin,
      const std::vector<const password_manager::PasswordForm*>*
          federated_matches,
      bool was_autofilled_on_pageload) override;
  void AutofillHttpAuth(
      const password_manager::PasswordForm& preferred_match,
      const password_manager::PasswordFormManagerForUI* form_manager) override;
  void NotifyUserCredentialsWereLeaked(
      password_manager::CredentialLeakType leak_type,
      const GURL& url,
      const std::u16string& username) override;
  void TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint access_point,
      base::OnceCallback<void(ReauthSucceeded)> reauth_callback) override;
  void TriggerSignIn(signin_metrics::AccessPoint access_point) override;
  PrefService* GetPrefs() const override;
  PrefService* GetLocalStatePrefs() const override;
  const syncer::SyncService* GetSyncService() const override;
  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override;
  password_manager::PasswordStoreInterface* GetAccountPasswordStore()
      const override;
  password_manager::PasswordReuseManager* GetPasswordReuseManager()
      const override;
  password_manager::PasswordChangeSuccessTracker*
  GetPasswordChangeSuccessTracker() override;
  password_manager::SyncState GetPasswordSyncState() const override;
  bool WasLastNavigationHTTPError() const override;

  net::CertStatus GetMainFrameCertStatus() const override;
  void PromptUserToEnableAutosignin() override;
  bool IsIncognito() const override;
  profile_metrics::BrowserProfileType GetProfileType() const override;
  const password_manager::PasswordManager* GetPasswordManager() const override;
  using password_manager::PasswordManagerClient::GetPasswordFeatureManager;
  const password_manager::PasswordFeatureManager* GetPasswordFeatureManager()
      const override;
  password_manager::HttpAuthManager* GetHttpAuthManager() override;
  autofill::AutofillDownloadManager* GetAutofillDownloadManager() override;
  bool IsCommittedMainFrameSecure() const override;
  const GURL& GetLastCommittedURL() const override;
  url::Origin GetLastCommittedOrigin() const override;
  const password_manager::CredentialsFilter* GetStoreResultFilter()
      const override;
  autofill::LogManager* GetLogManager() override;
  void AnnotateNavigationEntry(bool has_password_field) override;
  autofill::LanguageCode GetPageLanguage() const override;

  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;

#if defined(ON_FOCUS_PING_ENABLED)
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;
#endif

  // Reporting these events is only supported on desktop platforms.
#if !BUILDFLAG(IS_ANDROID)
  void MaybeReportEnterpriseLoginEvent(
      const GURL& url,
      bool is_federated,
      const url::Origin& federated_origin,
      const std::u16string& login_user_name) const override;
  void MaybeReportEnterprisePasswordBreachEvent(
      const std::vector<std::pair<GURL, std::u16string>>& identities)
      const override;
#endif

  ukm::SourceId GetUkmSourceId() override;
  password_manager::PasswordManagerMetricsRecorder* GetMetricsRecorder()
      override;
  password_manager::PasswordRequirementsService*
  GetPasswordRequirementsService() override;
  favicon::FaviconService* GetFaviconService() override;
  signin::IdentityManager* GetIdentityManager() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::NetworkContext* GetNetworkContext() const override;
  void UpdateFormManagers() override;
  void NavigateToManagePasswordsPage(
      password_manager::ManagePasswordsReferrer referrer) override;

#if BUILDFLAG(IS_ANDROID)
  void NavigateToManagePasskeysPage(
      password_manager::ManagePasswordsReferrer referrer) override;
#endif

  bool IsIsolationForPasswordSitesEnabled() const override;
  bool IsNewTabPage() const override;
  password_manager::FieldInfoManager* GetFieldInfoManager() const override;
  password_manager::WebAuthnCredentialsDelegate*
  GetWebAuthnCredentialsDelegateForDriver(
      password_manager::PasswordManagerDriver* driver) override;
  version_info::Channel GetChannel() const override;
  void RefreshPasswordManagerSettingsIfNeeded() const override;

  // autofill::mojom::PasswordGenerationDriver overrides.
  void AutomaticGenerationAvailable(
      const autofill::password_generation::PasswordGenerationUIData& ui_data)
      override;
  void ShowPasswordEditingPopup(const gfx::RectF& bounds,
                                const autofill::FormData& form_data,
                                autofill::FieldRendererId field_renderer_id,
                                const std::u16string& password_value) override;
  void PasswordGenerationRejectedByTyping() override;
  void PresaveGeneratedPassword(const autofill::FormData& form_data,
                                const std::u16string& password_value) override;
  void PasswordNoLongerGenerated(const autofill::FormData& form_data) override;
  void FrameWasScrolled() override;
  void GenerationElementLostFocus() override;

  // Observer for PasswordGenerationPopup events. Used for testing.
  void SetTestObserver(PasswordGenerationPopupObserver* observer);

  static void BindCredentialManager(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::CredentialManager> receiver);

  // A helper method to determine whether a save/update bubble can be shown
  // on this |url|.
  static bool CanShowBubbleOnURL(const GURL& url);

#if defined(UNIT_TEST)
  bool was_store_ever_called() const { return was_store_ever_called_; }
  bool has_binding_for_credential_manager() const {
    return content_credential_manager_.HasBinding();
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  PasswordAccessoryController* GetOrCreatePasswordAccessory();

  TouchToFillController* GetOrCreateTouchToFillController();

  password_manager::CredentialCache* GetCredentialCacheForTesting() {
    return &credential_cache_;
  }
#endif

 protected:
  // Callable for tests.
  ChromePasswordManagerClient(content::WebContents* web_contents,
                              autofill::AutofillClient* autofill_client);

 private:
  friend class content::WebContentsUserData<ChromePasswordManagerClient>;

  // content::WebContentsObserver overrides.
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

  // Given |bounds| in the renderers coordinate system, return the same bounds
  // in the screens coordinate system.
  gfx::RectF GetBoundsInScreenSpace(const gfx::RectF& bounds);

  // Instructs the client to hide the form filling UI.
  void HideFillingUI();

  // Checks if the current page specified in |url| fulfils the conditions for
  // the password manager to be active on it.
  bool IsPasswordManagementEnabledForCurrentPage(const GURL& url) const;

  // Returns true if this profile has metrics reporting and active sync
  // without custom sync passphrase.
  static bool ShouldAnnotateNavigationEntries(Profile* profile);

  // Called back by the PasswordGenerationAgent when the generation flow is
  // completed. If |ui_data| is non-empty, will create a UI to display the
  // generated password. Otherwise, nothing will happen.
  void GenerationResultAvailable(
      autofill::password_generation::PasswordGenerationType type,
      base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
      const absl::optional<
          autofill::password_generation::PasswordGenerationUIData>& ui_data);

  void ShowPasswordGenerationPopup(
      autofill::password_generation::PasswordGenerationType type,
      password_manager::ContentPasswordManagerDriver* driver,
      const autofill::password_generation::PasswordGenerationUIData& ui_data);

  gfx::RectF TransformToRootCoordinates(
      content::RenderFrameHost* frame_host,
      const gfx::RectF& bounds_in_frame_coordinates);

#if BUILDFLAG(IS_ANDROID)
  void ResetErrorMessageDelegate();
#endif

  const raw_ptr<Profile> profile_;

  password_manager::PasswordManager password_manager_;
  password_manager::PasswordFeatureManagerImpl password_feature_manager_;
  password_manager::HttpAuthManagerImpl httpauth_manager_;

#if BUILDFLAG(IS_ANDROID)
  // Holds and facilitates a credential store for each origin in this tab.
  password_manager::CredentialCache credential_cache_;

  // Controller for the Touch To Fill sheet. Created on demand during the first
  // call to GetOrCreateTouchToFillController().
  std::unique_ptr<TouchToFillController> touch_to_fill_controller_;

  std::unique_ptr<PasswordManagerErrorMessageDelegate>
      password_manager_error_message_delegate_;

  SaveUpdatePasswordMessageDelegate save_update_password_message_delegate_;
  GeneratedPasswordSavedMessageDelegate
      generated_password_saved_message_delegate_;
#endif  // BUILDFLAG(IS_ANDROID)

  raw_ptr<password_manager::ContentPasswordManagerDriverFactory,
          DanglingUntriaged>
      driver_factory_;

  // As a mojo service, will be registered into service registry
  // of the main frame host by ChromeContentBrowserClient
  // once main frame host was created.
  password_manager::ContentCredentialManager content_credential_manager_;

  content::RenderFrameHostReceiverSet<autofill::mojom::PasswordGenerationDriver>
      password_generation_driver_receivers_;

  // Observer for password generation popup.
  raw_ptr<PasswordGenerationPopupObserver> observer_;

  // Controls the popup
  base::WeakPtr<PasswordGenerationPopupControllerImpl> popup_controller_;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // MultiProfileCredentialsFilter requires DICE support.
  const MultiProfileCredentialsFilter credentials_filter_;
  AccountStorageAuthHelper account_storage_auth_helper_;
#else
  const password_manager::SyncCredentialsFilter credentials_filter_;
#endif

  std::unique_ptr<autofill::RoutingLogManager> log_manager_;

  // Recorder of metrics that is associated with the last committed navigation
  // of the WebContents owning this ChromePasswordManagerClient. May be unset at
  // times. Sends statistics on destruction.
  absl::optional<password_manager::PasswordManagerMetricsRecorder>
      metrics_recorder_;

  // Whether navigator.credentials.store() was ever called from this
  // WebContents. Used for testing.
  bool was_store_ever_called_ = false;

  // Helper for performing logic that is common between
  // ChromePasswordManagerClient and IOSChromePasswordManagerClient.
  password_manager::PasswordManagerClientHelper helper_;

#if BUILDFLAG(IS_ANDROID)
  // Username filled by Touch To Fill and the timestamp. Used to collect
  // metrics. TODO(crbug.com/1299394): Remove after the launch.
  absl::optional<std::pair<std::u16string, base::Time>>
      username_filled_by_touch_to_fill_ = absl::nullopt;
#endif  // BUILDFLAG(IS_ANDROID)

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
