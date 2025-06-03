// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/safe_browsing/phishy_interaction_tracker.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager.h"
#include "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager_client.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace safe_browsing {
class PasswordProtectionService;
}

// ChromePasswordReuseDetectionManagerClient is instantiated once per
// WebContents. It manages password reuse detection.
class ChromePasswordReuseDetectionManagerClient
    : public safe_browsing::PasswordReuseDetectionManagerClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          ChromePasswordReuseDetectionManagerClient>,
      public content::RenderWidgetHost::InputEventObserver {
 public:
  ChromePasswordReuseDetectionManagerClient(
      const ChromePasswordReuseDetectionManagerClient&) = delete;
  ChromePasswordReuseDetectionManagerClient& operator=(
      const ChromePasswordReuseDetectionManagerClient&) = delete;

  ~ChromePasswordReuseDetectionManagerClient() override;

  static void CreateForWebContents(content::WebContents* contents);

  const GURL& GetLastCommittedURL() const;

  // TODO(https://crbug.com/1419602): This function is overridden in unit tests.
  // This will be removed after the unit tests refactoring.
  virtual safe_browsing::PasswordProtectionService*
  GetPasswordProtectionService() const;

  // PasswordReuseDetectionManagerClient implementation.
  void MaybeLogPasswordReuseDetectedEvent() override;
  autofill::LogManager* GetLogManager() override;
  password_manager::PasswordReuseManager* GetPasswordReuseManager()
      const override;
  bool IsHistorySyncAccountEmail(const std::string& username) override;
  bool IsPasswordFieldDetectedOnPage() override;
  void CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType reused_password_type,
      const std::string& username,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists,
      uint64_t reused_password_hash,
      const std::string& domain) override;

#if BUILDFLAG(IS_ANDROID)
  // Notifies `PasswordReuseDetectionManager` about passwords selected from
  // AllPasswordsBottomSheet.
  void OnPasswordSelected(const std::u16string& text) override;

  // content::RenderWidgetHost::InputEventObserver overrides. Notifies
  // OnKeyPressed events.
  void OnImeTextCommittedEvent(const std::u16string& text_str) override;
  void OnImeSetComposingTextEvent(const std::u16string& text_str) override;
  void OnImeFinishComposingTextEvent() override;
#endif

 protected:
  explicit ChromePasswordReuseDetectionManagerClient(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      ChromePasswordReuseDetectionManagerClient>;

  // content::WebContentsObserver overrides.
  void PrimaryPageChanged(content::Page& page) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void OnPaste() override;

  // content::RenderWidgetHost::InputEventObserver overrides.
  void OnInputEvent(const blink::WebInputEvent&) override;

  safe_browsing::PasswordReuseDetectionManager
      password_reuse_detection_manager_;
  const raw_ptr<Profile> profile_;

  std::unique_ptr<autofill::RoutingLogManager> log_manager_;

  safe_browsing::PhishyInteractionTracker phishy_interaction_tracker_;

#if BUILDFLAG(IS_ANDROID)
  // Last composing text from ime, this is updated when ime set composing text
  // event is triggered. It is sent to password reuse detection manager and
  // reset when ime finish composing text event is triggered.
  std::u16string last_composing_text_;
#endif  // BUILDFLAG(IS_ANDROID)

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_
