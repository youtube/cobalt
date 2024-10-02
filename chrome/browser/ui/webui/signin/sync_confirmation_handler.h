// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_HANDLER_H_

#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace signin {
class IdentityManager;
}

// WebUI message handler for the sync confirmation dialog. IdentityManager calls
// in this class use signin::ConsentLevel::kSignin because the user hasn't
// consented to sync yet.
class SyncConfirmationHandler : public content::WebUIMessageHandler,
                                public signin::IdentityManager::Observer,
                                public BrowserListObserver {
 public:
  // Creates a SyncConfirmationHandler for the |profile|. All strings in the
  // corresponding Web UI should be represented in |string_to_grd_id_map| and
  // mapped to their GRD IDs. If |browser| is provided, its signin view
  // controller will be notified of the rendered size of the web page.
  SyncConfirmationHandler(
      Profile* profile,
      const std::unordered_map<std::string, int>& string_to_grd_id_map,
      Browser* browser = nullptr);

  SyncConfirmationHandler(const SyncConfirmationHandler&) = delete;
  SyncConfirmationHandler& operator=(const SyncConfirmationHandler&) = delete;

  ~SyncConfirmationHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 protected:
  // Handles "confirm" message from the page. No arguments.
  // This message is sent when the user confirms that they want complete sign in
  // with default sync settings.
  virtual void HandleConfirm(const base::Value::List& args);

  // Handles "undo" message from the page. No arguments.
  // This message is sent when the user clicks "undo" on the sync confirmation
  // dialog, which aborts signin and prevents sync from starting.
  virtual void HandleUndo(const base::Value::List& args);

  // Handles "goToSettings" message from the page. No arguments.
  // This message is sent when the user clicks on the "Settings" link in the
  // sync confirmation dialog, which completes sign in but takes the user to the
  // sync settings page for configuration before starting sync.
  virtual void HandleGoToSettings(const base::Value::List& args);

  // Handles the web ui message sent when the html content is done being laid
  // out and it's time to resize the native view hosting it to fit. |args| is
  // a single integer value for the height the native view should resize to.
  virtual void HandleInitializedWithSize(const base::Value::List& args);

  // Handles the "accountInfoRequest" message sent after the
  // "account-info-changed" WebUIListener was added. This method calls
  // |SetAccountInfo| with the signed-in user's picture url.
  virtual void HandleAccountInfoRequest(const base::Value::List& args);

  // Records the user's consent to sync. Called from |HandleConfirm| and
  // |HandleGoToSettings|, and expects two parameters to be passed through
  // these methods from the WebUI:
  // 1. List of strings (names of the string resources constituting the consent
  //                     description as per WebUIDataSource)
  // 2. Strings (name of the string resource of the consent confirmation)
  // This message is sent when the user interacts with the dialog in a positive
  // manner, i.e. clicks on the confirmation button or the settings link.
  virtual void RecordConsent(const base::Value::List& args);

  // Sets the account image shown in the dialog based on |info|, which is
  // expected to be valid.
  virtual void SetAccountInfo(const AccountInfo& info);

  // Closes the modal signin window and calls
  // LoginUIService::SyncConfirmationUIClosed with |result|. |result| indicates
  // the option chosen by the user in the confirmation UI.
  void CloseModalSigninWindow(
      LoginUIService::SyncConfirmationUIClosedResult result);

 private:
  raw_ptr<Profile> profile_;

  // Records whether the user clicked on Undo, Ok, or Settings.
  bool did_user_explicitly_interact_ = false;

  // Mapping between strings displayed in the UI corresponding to this handler
  // and their respective GRD IDs.
  std::unordered_map<std::string, int> string_to_grd_id_map_;

  // Weak reference to the browser that showed the sync confirmation dialog (if
  // such a dialog exists).
  raw_ptr<Browser> browser_;

  raw_ptr<signin::IdentityManager> identity_manager_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_HANDLER_H_
