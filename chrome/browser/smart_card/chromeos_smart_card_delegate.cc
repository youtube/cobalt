// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/chromeos_smart_card_delegate.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/get_smart_card_context_factory.h"
#include "chrome/browser/smart_card/smart_card_permission_context.h"
#include "chrome/browser/smart_card/smart_card_permission_context_factory.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/render_frame_host.h"

ChromeOsSmartCardDelegate::ChromeOsSmartCardDelegate() = default;
ChromeOsSmartCardDelegate::~ChromeOsSmartCardDelegate() = default;

mojo::PendingRemote<device::mojom::SmartCardContextFactory>
ChromeOsSmartCardDelegate::GetSmartCardContextFactory(
    content::BrowserContext& browser_context) {
  return ::GetSmartCardContextFactory(browser_context);
}

bool ChromeOsSmartCardDelegate::IsPermissionBlocked(
    content::RenderFrameHost& render_frame_host) {
  auto& profile = CHECK_DEREF(
      Profile::FromBrowserContext(render_frame_host.GetBrowserContext()));

  auto& permission_context =
      SmartCardPermissionContextFactory::GetForProfile(profile);

  const url::Origin& origin =
      render_frame_host.GetMainFrame()->GetLastCommittedOrigin();

  return !permission_context.CanRequestObjectPermission(origin) &&
         !permission_context.IsAllowlistedByPolicy(origin);
}

bool ChromeOsSmartCardDelegate::HasReaderPermission(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name) {
  auto& profile = CHECK_DEREF(
      Profile::FromBrowserContext(render_frame_host.GetBrowserContext()));

  auto& permission_context =
      SmartCardPermissionContextFactory::GetForProfile(profile);

  return permission_context.HasReaderPermission(render_frame_host, reader_name);
}

void ChromeOsSmartCardDelegate::RequestReaderPermission(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name,
    RequestReaderPermissionCallback callback) {
  auto& profile = CHECK_DEREF(
      Profile::FromBrowserContext(render_frame_host.GetBrowserContext()));

  auto& permission_context =
      SmartCardPermissionContextFactory::GetForProfile(profile);

  permission_context.RequestReaderPermisssion(render_frame_host, reader_name,
                                              std::move(callback));
}

void ChromeOsSmartCardDelegate::NotifyConnectionUsed(
    content::RenderFrameHost& render_frame_host) {
  CHECK_DEREF(content_settings::PageSpecificContentSettings::GetForFrame(
                  &render_frame_host))
      .OnDeviceUsed(
          content_settings::mojom::ContentSettingsType::SMART_CARD_GUARD);
}

void ChromeOsSmartCardDelegate::NotifyLastConnectionLost(
    content::RenderFrameHost& render_frame_host) {
  CHECK_DEREF(content_settings::PageSpecificContentSettings::GetForFrame(
                  &render_frame_host))
      .OnLastDeviceConnectionLost(
          content_settings::mojom::ContentSettingsType::SMART_CARD_GUARD);
}
