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

#ifndef COBALT_BROWSER_COBALT_WEBAPPS_CLIENT_H_

#define COBALT_BROWSER_COBALT_WEBAPPS_CLIENT_H_

#include "components/webapps/browser/webapps_client.h"

class CobaltWebappsClient : public webapps::WebappsClient {
 public:
  CobaltWebappsClient();

  ~CobaltWebappsClient() override;

  // webapps::WebappsClient:

  bool IsOriginConsideredSecure(const url::Origin& url) override;

  security_state::SecurityLevel GetSecurityLevelForWebContents(

      content::WebContents* web_contents) override;

  infobars::ContentInfoBarManager* GetInfoBarManagerForWebContents(

      content::WebContents* web_contents) override;

  webapps::WebappInstallSource GetInstallSource(

      content::WebContents* web_contents,

      webapps::InstallTrigger trigger) override;

  webapps::AppBannerManager* GetAppBannerManager(

      content::WebContents* web_contents) override;

#if BUILDFLAG(IS_ANDROID)

  bool IsInstallationInProgress(content::WebContents* web_contents,

                                const GURL& manifest_url,

                                const GURL& manifest_id) override;

  bool CanShowAppBanners(content::WebContents* web_contents) override;

  void OnWebApkInstallInitiatedFromAppMenu(

      content::WebContents* web_contents) override;

  void InstallWebApk(content::WebContents* web_contents,

                     const webapps::AddToHomescreenParams& params) override;

  void InstallShortcut(content::WebContents* web_contents,

                       const webapps::AddToHomescreenParams& params) override;

#endif
};

#endif  // COBALT_BROWSER_COBALT_WEBAPPS_CLIENT_H_
