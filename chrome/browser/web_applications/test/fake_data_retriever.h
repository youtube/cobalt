// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_DATA_RETRIEVER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

namespace web_app {

// All WebAppDataRetriever operations are async, so this class posts tasks
// when running callbacks to simulate async behavior in tests as well.
class FakeDataRetriever : public WebAppDataRetriever {
 public:
  FakeDataRetriever();
  FakeDataRetriever(const FakeDataRetriever&) = delete;
  FakeDataRetriever& operator=(const FakeDataRetriever&) = delete;
  ~FakeDataRetriever() override;

  // WebAppDataRetriever:
  void GetWebAppInstallInfo(content::WebContents* web_contents,
                            GetWebAppInstallInfoCallback callback) override;
  void CheckInstallabilityAndRetrieveManifest(
      content::WebContents* web_contents,
      bool bypass_service_worker_check,
      CheckInstallabilityCallback callback,
      absl::optional<webapps::InstallableParams> params) override;
  void GetIcons(content::WebContents* web_contents,
                base::flat_set<GURL> icon_urls,
                bool skip_page_favicons,
                GetIconsCallback callback) override;

  // Set info to respond on |GetWebAppInstallInfo|.
  void SetRendererWebAppInstallInfo(
      std::unique_ptr<WebAppInstallInfo> web_app_info);
  void SetEmptyRendererWebAppInstallInfo();
  // Set arguments to respond on |CheckInstallabilityAndRetrieveManifest|.
  void SetManifest(blink::mojom::ManifestPtr manifest,
                   webapps::InstallableStatusCode error_code,
                   GURL manifest_url = GURL());
  // Set icons to respond on |GetIcons|.
  void SetIcons(IconsMap icons_map);
  using GetIconsDelegate =
      base::RepeatingCallback<IconsMap(content::WebContents* web_contents,
                                       const base::flat_set<GURL>& icon_urls,
                                       bool skip_page_favicons)>;
  void SetGetIconsDelegate(GetIconsDelegate get_icons_delegate);

  // Sets `IconsDownloadedResult` to respond on `GetIcons`.
  void SetIconsDownloadedResult(IconsDownloadedResult result);
  // Sets `DownloadedIconsHttpResults` to respond on `GetIcons`.
  void SetDownloadedIconsHttpResults(
      DownloadedIconsHttpResults icons_http_results);

  void SetDestructionCallback(base::OnceClosure callback);

  WebAppInstallInfo& web_app_info() { return *web_app_info_; }

  // Builds minimal data for install to succeed. Data includes: empty renderer
  // info, manifest with |url| and |scope|, installability checked as |true|,
  // empty icons.
  void BuildDefaultDataToRetrieve(const GURL& url, const GURL& scope);

 private:
  void ScheduleCompletionCallback();
  void CallCompletionCallback();

  base::OnceClosure completion_callback_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;

  blink::mojom::ManifestPtr manifest_;
  GURL manifest_url_;
  webapps::InstallableStatusCode error_code_ =
      webapps::InstallableStatusCode::NO_MANIFEST;

  IconsMap icons_map_;
  GetIconsDelegate get_icons_delegate_;

  IconsDownloadedResult icons_downloaded_result_ =
      IconsDownloadedResult::kCompleted;
  DownloadedIconsHttpResults icons_http_results_;

  base::OnceClosure destruction_callback_;

  base::WeakPtrFactory<FakeDataRetriever> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_DATA_RETRIEVER_H_
