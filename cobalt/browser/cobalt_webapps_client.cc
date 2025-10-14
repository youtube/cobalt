// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/cobalt_webapps_client.h"

#include "components/security_state/core/security_state.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

CobaltWebappsClient::CobaltWebappsClient() {}

CobaltWebappsClient::~CobaltWebappsClient() {}

bool CobaltWebappsClient::IsOriginConsideredSecure(const url::Origin& url) {
  return true;
}

security_state::SecurityLevel
CobaltWebappsClient::GetSecurityLevelForWebContents(
    content::WebContents* web_contents) {
  if (web_contents->GetLastCommittedURL().SchemeIs(url::kHttpsScheme)) {
    return security_state::SECURE;
  }
  return security_state::NONE;
}

infobars::ContentInfoBarManager*
CobaltWebappsClient::GetInfoBarManagerForWebContents(
    content::WebContents* web_contents) {
  return nullptr;
}

webapps::WebappInstallSource CobaltWebappsClient::GetInstallSource(
    content::WebContents* web_contents,
    webapps::InstallTrigger trigger) {
  return webapps::WebappInstallSource::DEVTOOLS;
}

webapps::AppBannerManager* CobaltWebappsClient::GetAppBannerManager(
    content::WebContents* web_contents) {
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
bool CobaltWebappsClient::IsInstallationInProgress(
    content::WebContents* web_contents,
    const GURL& manifest_url,
    const GURL& manifest_id) {
  return false;
}

bool CobaltWebappsClient::CanShowAppBanners(
    content::WebContents* web_contents) {
  return true;
}

void CobaltWebappsClient::OnWebApkInstallInitiatedFromAppMenu(
    content::WebContents* web_contents) {}

void CobaltWebappsClient::InstallWebApk(
    content::WebContents* web_contents,
    const webapps::AddToHomescreenParams& params) {}

void CobaltWebappsClient::InstallShortcut(
    content::WebContents* web_contents,
    const webapps::AddToHomescreenParams& params) {}
#endif
