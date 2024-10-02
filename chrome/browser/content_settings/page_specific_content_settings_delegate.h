// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_

#include "build/build_config.h"
#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/custom_handlers/protocol_handler.h"
#include "content/public/browser/web_contents_observer.h"

namespace chrome {

using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;

class PageSpecificContentSettingsDelegate
    : public content_settings::PageSpecificContentSettings::Delegate,
      public content::WebContentsObserver {
 public:
  explicit PageSpecificContentSettingsDelegate(
      content::WebContents* web_contents);
  ~PageSpecificContentSettingsDelegate() override;
  PageSpecificContentSettingsDelegate(
      const PageSpecificContentSettingsDelegate&) = delete;
  PageSpecificContentSettingsDelegate& operator=(
      const PageSpecificContentSettingsDelegate&) = delete;

  static PageSpecificContentSettingsDelegate* FromWebContents(
      content::WebContents* web_contents);

  // Call to indicate that there is a protocol handler pending user approval.
  void set_pending_protocol_handler(
      const custom_handlers::ProtocolHandler& handler) {
    pending_protocol_handler_ = handler;
  }

  const custom_handlers::ProtocolHandler& pending_protocol_handler() const {
    return pending_protocol_handler_;
  }

  void ClearPendingProtocolHandler() {
    pending_protocol_handler_ =
        custom_handlers::ProtocolHandler::EmptyProtocolHandler();
  }

  // Sets the previous protocol handler which will be replaced by the
  // pending protocol handler.
  void set_previous_protocol_handler(
      const custom_handlers::ProtocolHandler& handler) {
    previous_protocol_handler_ = handler;
  }

  const custom_handlers::ProtocolHandler& previous_protocol_handler() const {
    return previous_protocol_handler_;
  }

  // Set whether the setting for the pending handler is DEFAULT (ignore),
  // ALLOW, or DENY.
  void set_pending_protocol_handler_setting(ContentSetting setting) {
    pending_protocol_handler_setting_ = setting;
  }

  ContentSetting pending_protocol_handler_setting() const {
    return pending_protocol_handler_setting_;
  }

 private:
  // PageSpecificContentSettings::Delegate:
  void UpdateLocationBar() override;
  PrefService* GetPrefs() override;
  HostContentSettingsMap* GetSettingsMap() override;
  std::unique_ptr<BrowsingDataModel::Delegate> CreateBrowsingDataModelDelegate()
      override;
  void SetDefaultRendererContentSettingRules(
      content::RenderFrameHost* rfh,
      RendererContentSettingRules* rules) override;
  std::vector<storage::FileSystemType> GetAdditionalFileSystemTypes() override;
  browsing_data::CookieHelper::IsDeletionDisabledCallback
  GetIsDeletionDisabledCallback() override;
  bool IsMicrophoneCameraStateChanged(
      content_settings::PageSpecificContentSettings::MicrophoneCameraState
          microphone_camera_state,
      const std::string& media_stream_selected_audio_device,
      const std::string& media_stream_selected_video_device) override;
  content_settings::PageSpecificContentSettings::MicrophoneCameraState
  GetMicrophoneCameraState() override;
  content::WebContents* MaybeGetSyncedWebContentsForPictureInPicture(
      content::WebContents* web_contents) override;
  void OnContentAllowed(ContentSettingsType type) override;
  void OnContentBlocked(ContentSettingsType type) override;
  void OnStorageAccessAllowed(StorageType storage_type,
                              const url::Origin& origin,
                              content::Page& page) override;
  void OnCookieAccessAllowed(const net::CookieList& accessed_cookies,
                             content::Page& page) override;
  void OnServiceWorkerAccessAllowed(const url::Origin& origin,
                                    content::Page& page) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // The pending protocol handler, if any. This can be set if
  // registerProtocolHandler was invoked without user gesture.
  // The |IsEmpty| method will be true if no protocol handler is
  // pending registration.
  custom_handlers::ProtocolHandler pending_protocol_handler_ =
      custom_handlers::ProtocolHandler::EmptyProtocolHandler();

  // The previous protocol handler to be replaced by
  // the pending_protocol_handler_, if there is one. Empty if
  // there is no handler which would be replaced.
  custom_handlers::ProtocolHandler previous_protocol_handler_ =
      custom_handlers::ProtocolHandler::EmptyProtocolHandler();

  // The setting on the pending protocol handler registration. Persisted in case
  // the user opens the bubble and makes changes multiple times.
  ContentSetting pending_protocol_handler_setting_ = CONTENT_SETTING_DEFAULT;

  std::unique_ptr<AccessContextAuditService::CookieAccessHelper>
      cookie_access_helper_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
