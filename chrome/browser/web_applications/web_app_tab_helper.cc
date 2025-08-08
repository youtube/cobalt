// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_tab_helper.h"

#include <memory>
#include <string>

#include "base/check_is_test.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace web_app {

// static
void WebAppTabHelper::Create(tabs::TabInterface* tab,
                             content::WebContents* contents) {
  // In the event when a tab is moved from a normal browser window to an app
  // window, or vise versa, we want to keep the state on WebAppTabHelper.
  auto* tab_helper = WebAppTabHelper::FromWebContents(contents);
  if (tab->GetContents() == contents && tab_helper) {
    tab_helper->SubscribeToTabState(tab);
    return;
  }

  // If on the other hand this is a tab-discard, we let the old tab's
  // WebAppTabHelper be destroyed at its normal timing. This is because the
  // current implementation of WebAppMetrics relies on the assumption that
  // discarded WebContents are still usable.
  // This will become a moot point once https://crbug.com/347770670 is fixed, as
  // discarding will no longer change the WebContents.

  auto helper = std::make_unique<WebAppTabHelper>(tab, contents);
  helper->SubscribeToTabState(tab);
  contents->SetUserData(UserDataKey(), std::move(helper));
}

// static
const webapps::AppId* WebAppTabHelper::GetAppId(
    const content::WebContents* web_contents) {
  auto* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->app_id_.has_value()
             ? &tab_helper->app_id_.value()
             : nullptr;
}

#if BUILDFLAG(IS_MAC)
std::optional<webapps::AppId>
WebAppTabHelper::GetAppIdForNotificationAttribution(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          features::kAppShimNotificationAttribution)) {
    return std::nullopt;
  }
  const webapps::AppId* app_id = GetAppId(web_contents);
  if (!app_id) {
    return std::nullopt;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  WebAppProvider* web_app_provider = WebAppProvider::GetForWebApps(profile);
  if (!web_app_provider ||
      !web_app_provider->registrar_unsafe().IsInstallState(
          *app_id, {proto::INSTALLED_WITH_OS_INTEGRATION})) {
    return std::nullopt;
  }
  // Default apps are locally installed but unless an app shim has been created
  // for them should not get attributed notifications.
  if (!AppShimRegistry::Get()->IsAppInstalledInProfile(*app_id,
                                                       profile->GetPath())) {
    return std::nullopt;
  }
  return *app_id;
}
#endif

WebAppTabHelper::~WebAppTabHelper() = default;

const base::UnguessableToken& WebAppTabHelper::GetAudioFocusGroupIdForTesting()
    const {
  return audio_focus_group_id_;
}

WebAppLaunchQueue& WebAppTabHelper::EnsureLaunchQueue() {
  if (!launch_queue_) {
    launch_queue_ = std::make_unique<WebAppLaunchQueue>(
        web_contents(), provider_->registrar_unsafe());
  }
  return *launch_queue_;
}

void WebAppTabHelper::SetState(std::optional<webapps::AppId> app_id,
                               bool is_in_app_window) {
  // Empty string should not be used to indicate "no app ID".
  DCHECK(!app_id || !app_id->empty());

  // If the app_id is changing, then it should exist in the database.
  DCHECK(app_id_ == app_id || !app_id ||
         provider_->registrar_unsafe().IsInstalled(*app_id) ||
         provider_->registrar_unsafe().IsUninstalling(*app_id));
  if (app_id_ == app_id && is_in_app_window == is_in_app_window_) {
    return;
  }

  std::optional<webapps::AppId> previous_app_id = std::move(app_id_);
  app_id_ = std::move(app_id);

  is_in_app_window_ = is_in_app_window;

  if (previous_app_id != app_id_) {
    OnAssociatedAppChanged(previous_app_id, app_id_);
  }
  UpdateAudioFocusGroupId();
}

void WebAppTabHelper::SetAppId(std::optional<webapps::AppId> app_id) {
  SetState(app_id, is_in_app_window());
}

void WebAppTabHelper::SetIsInAppWindow(bool is_in_app_window) {
  SetState(app_id(), is_in_app_window);
}

void WebAppTabHelper::SetCallbackToRunOnTabChanges(base::OnceClosure callback) {
  on_tab_details_changed_callback_ = std::move(callback);
}

void WebAppTabHelper::OnTabBackgrounded(tabs::TabInterface*) {
  MaybeNotifyTabChanged();
}

void WebAppTabHelper::OnTabDetached(tabs::TabInterface* tab_interface,
                                    tabs::TabInterface::DetachReason) {
  MaybeNotifyTabChanged();
}

void WebAppTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    const GURL& url = navigation_handle->GetURL();
    SetAppId(FindAppWithUrlInScope(url));
  }

  // If navigating to a Web App (including navigation in sub frames), let
  // `WebAppUiManager` observers perform tab-secific setup for navigations in
  // Web Apps.
  if (app_id_.has_value()) {
    provider_->ui_manager().NotifyReadyToCommitNavigation(app_id_.value(),
                                                          navigation_handle);
  }
}

void WebAppTabHelper::PrimaryPageChanged(content::Page& page) {
  // This method is invoked whenever primary page of a WebContents
  // (WebContents::GetPrimaryPage()) changes to `page`. This happens in one of
  // the following cases:
  // 1) when the current RenderFrameHost in the primary main frame changes after
  //    a navigation.
  // 2) when the current RenderFrameHost in the primary main frame is
  //    reinitialized after a crash.
  // 3) when a cross-document navigation commits in the current RenderFrameHost
  //    of the primary main frame.
  //
  // The new primary page might either be a brand new one (if the committed
  // navigation created a new document in the primary main frame) or an existing
  // one (back-forward cache restore or prerendering activation).
  //
  // This notification is not dispatched for changes of pages in the non-primary
  // frame trees (prerendering, fenced frames) and when the primary page is
  // destroyed (e.g., when closing a tab).
  //
  // See the declaration of WebContentsObserver::PrimaryPageChanged for more
  // information.
  provider_->manifest_update_manager().MaybeUpdate(
      page.GetMainDocument().GetLastCommittedURL(), app_id_, web_contents());

  ReinstallPlaceholderAppIfNecessary(
      page.GetMainDocument().GetLastCommittedURL());
}

WebAppTabHelper::WebAppTabHelper(tabs::TabInterface* tab,
                                 content::WebContents* contents)
    : content::WebContentsUserData<WebAppTabHelper>(*contents),
      content::WebContentsObserver(contents),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(
          tab->GetBrowserWindowInterface()->GetProfile())) {
  observation_.Observe(&provider_->install_manager());
  SetState(FindAppWithUrlInScope(contents->GetLastCommittedURL()),
           /*is_in_app_window=*/false);
}

void WebAppTabHelper::OnWebAppInstalled(
    const webapps::AppId& installed_app_id) {
  // Check if current web_contents url is in scope for the newly installed app.
  std::optional<webapps::AppId> app_id =
      FindAppWithUrlInScope(web_contents()->GetLastCommittedURL());
  if (app_id == installed_app_id) {
    SetAppId(app_id);
  }
}

void WebAppTabHelper::OnWebAppWillBeUninstalled(
    const webapps::AppId& uninstalled_app_id) {
  if (app_id_ == uninstalled_app_id) {
    SetAppId(std::nullopt);
  }
}

void WebAppTabHelper::OnWebAppInstallManagerDestroyed() {
  observation_.Reset();
  SetAppId(std::nullopt);
}

void WebAppTabHelper::OnAssociatedAppChanged(
    const std::optional<webapps::AppId>& previous_app_id,
    const std::optional<webapps::AppId>& new_app_id) {
  // Tag WebContents for Task Manager.
  // cases to consider:
  // 1. non-app -> app (association added)
  // 2. non-app -> non-app
  // 3. app -> app (association changed)
  // 4. app -> non-app (association removed)

  if (new_app_id.has_value() && !new_app_id->empty()) {
    // case 1 & 3:
    // WebContents could already be tagged with TabContentsTag or WebAppTag,
    // therefore we want to clear it.
    task_manager::WebContentsTags::ClearTag(web_contents());
    task_manager::WebContentsTags::CreateForWebApp(
        web_contents(), new_app_id.value(),
        provider_->registrar_unsafe().IsIsolated(new_app_id.value()));
  } else {
    // case 4:
    if (previous_app_id.has_value() && !previous_app_id->empty()) {
      // remove WebAppTag, add TabContentsTag.
      task_manager::WebContentsTags::ClearTag(web_contents());
      task_manager::WebContentsTags::CreateForTabContents(web_contents());
    }
    // case 2: do nothing
  }
}

void WebAppTabHelper::UpdateAudioFocusGroupId() {
  if (app_id_.has_value() && is_in_app_window_) {
    audio_focus_group_id_ =
        provider_->audio_focus_id_map().CreateOrGetIdForApp(app_id_.value());
  } else {
    audio_focus_group_id_ = base::UnguessableToken::Null();
  }

  // There is no need to trigger creation of a MediaSession if we'd merely be
  // resetting the audo focus group id as that is the default state. Skipping
  // creating a MediaSession when not needed also helps with some (unit) tests
  // where creating a MediaSession can trigger other subsystems in ways that
  // the test might not be setup for (for example lack of a real io thread for
  // the mdns service).
  if (audio_focus_group_id_ == base::UnguessableToken::Null() &&
      !content::MediaSession::GetIfExists(web_contents())) {
    return;
  }
  content::MediaSession::Get(web_contents())
      ->SetAudioFocusGroupId(audio_focus_group_id_);
}

void WebAppTabHelper::ReinstallPlaceholderAppIfNecessary(const GURL& url) {
  provider_->policy_manager().ReinstallPlaceholderAppIfNecessary(
      url, base::DoNothing());
}

std::optional<webapps::AppId> WebAppTabHelper::FindAppWithUrlInScope(
    const GURL& url) const {
  return provider_->registrar_unsafe().FindAppWithUrlInScope(url);
}

void WebAppTabHelper::SubscribeToTabState(tabs::TabInterface* tab_interface) {
  tab_subscriptions_.clear();
  CHECK(tab_interface);
  tab_subscriptions_.push_back(
      tab_interface->RegisterWillEnterBackground(base::BindRepeating(
          &WebAppTabHelper::OnTabBackgrounded, weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(
      tab_interface->RegisterWillDetach(base::BindRepeating(
          &WebAppTabHelper::OnTabDetached, weak_factory_.GetWeakPtr())));
}

void WebAppTabHelper::MaybeNotifyTabChanged() {
  if (on_tab_details_changed_callback_) {
    std::move(on_tab_details_changed_callback_).Run();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppTabHelper);

}  // namespace web_app
