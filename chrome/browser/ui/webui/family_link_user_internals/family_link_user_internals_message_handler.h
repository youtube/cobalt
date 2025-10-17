// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_MESSAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/web_ui_message_handler.h"

// The implementation for the chrome://family-link-user-internals page.
class FamilyLinkUserInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public SupervisedUserServiceObserver,
      public supervised_user::SupervisedUserURLFilter::Observer,
      public signin::IdentityManager::Observer {
 public:
  enum class WebContentFilters : bool {
    kDisabled = false,
    kEnabled = true,
  };

  FamilyLinkUserInternalsMessageHandler();

  FamilyLinkUserInternalsMessageHandler(
      const FamilyLinkUserInternalsMessageHandler&) = delete;
  FamilyLinkUserInternalsMessageHandler& operator=(
      const FamilyLinkUserInternalsMessageHandler&) = delete;

  ~FamilyLinkUserInternalsMessageHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  // SupervisedUserServiceObserver:
  void OnURLFilterChanged() override;

  // signin::IdentityManager::Observer
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // Uniform handler for all signin::IdentityManager::Observer changes.
  void OnAccountChanged();

  supervised_user::SupervisedUserService* GetSupervisedUserService();

  void HandleRegisterForEvents(const base::Value::List& args);
  void HandleGetBasicInfo(const base::Value::List& args);
  void HandleTryURL(const base::Value::List& args);
  void HandleChangeSearchContentFilters(const base::Value::List& args);
  void HandleChangeBrowserContentFilters(const base::Value::List& args);

  // Sets the browser's state of search and browser content filtering as
  // indicated in this UI.
  void ConfigureSearchContentFilters();
  void ConfigureBrowserContentFilters();

  void SendBasicInfo();
  void SendFamilyLinkUserSettings(const base::Value::Dict& settings);
  void SendWebContentFiltersInfo();

  void OnTryURLResult(
      const std::string& callback_id,
      supervised_user::SupervisedUserURLFilter::Result filtering_result);

  void OnURLChecked(supervised_user::SupervisedUserURLFilter::Result
                        filtering_result) override;

  // Emulates device-level setting that manipulates search or browser content
  // filtering. Available only to non-supervised profiles. Note: if multiple
  // chrome:// pages are open simultaneously, they might override each other.
  // This is safe, but will render web-ui off-sync.
  WebContentFilters search_content_filtering_status_{
      WebContentFilters::kDisabled};
  WebContentFilters browser_content_filtering_status_{
      WebContentFilters::kDisabled};

  base::CallbackListSubscription user_settings_subscription_;

  base::ScopedObservation<supervised_user::SupervisedUserURLFilter,
                          supervised_user::SupervisedUserURLFilter::Observer>
      url_filter_observation_{this};

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<FamilyLinkUserInternalsMessageHandler> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_MESSAGE_HANDLER_H_
