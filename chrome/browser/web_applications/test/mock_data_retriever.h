// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_DATA_RETRIEVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_DATA_RETRIEVER_H_

#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace web_app {

// All WebAppDataRetriever operations are async, so this class posts tasks
// when running callbacks to simulate async behavior in tests as well.
class MockDataRetriever : public WebAppDataRetriever {
 public:
  MockDataRetriever();
  ~MockDataRetriever() override;

  MOCK_METHOD(void,
              GetWebAppInstallInfo,
              (content::WebContents * web_contents,
               GetWebAppInstallInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              CheckInstallabilityAndRetrieveManifest,
              (content::WebContents * web_contents,
               bool bypass_service_worker_check,
               CheckInstallabilityCallback callback,
               absl::optional<webapps::InstallableParams> params),
              (override));
  MOCK_METHOD(void,
              GetIcons,
              (content::WebContents * web_contents,
               base::flat_set<GURL> icon_urls,
               bool skip_page_favicons,
               GetIconsCallback callback),
              (override));
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_DATA_RETRIEVER_H_
