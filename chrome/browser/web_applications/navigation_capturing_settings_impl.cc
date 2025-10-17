// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_settings_impl.h"

#include "chrome/browser/web_applications/navigation_capturing_settings.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

std::unique_ptr<NavigationCapturingSettings>
NavigationCapturingSettings::Create(Profile& profile) {
  return std::make_unique<NavigationCapturingSettingsImpl>(profile);
}

NavigationCapturingSettingsImpl::NavigationCapturingSettingsImpl(
    Profile& profile)
    : profile_(profile) {}

NavigationCapturingSettingsImpl::~NavigationCapturingSettingsImpl() = default;

std::optional<webapps::AppId>
NavigationCapturingSettingsImpl::GetCapturingWebAppForUrl(const GURL& url) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(&profile_.get());
  return provider->registrar_unsafe().FindAppThatCapturesLinksInScope(url);
}

}  // namespace web_app
