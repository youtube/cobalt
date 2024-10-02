// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_CONTENT_BROWSER_CLIENT_H_

#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/public/browser/content_browser_client.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;
class RenderFrameHost;

template <class SuperClass>
class MockAttributionReportingContentBrowserClientBase : public SuperClass {
 public:
  // ContentBrowserClient:
  MOCK_METHOD(bool,
              IsAttributionReportingOperationAllowed,
              (BrowserContext*,
               ContentBrowserClient::AttributionReportingOperation,
               RenderFrameHost*,
               const url::Origin* source_origin,
               const url::Origin* destination_origin,
               const url::Origin* reporting_origin),
              (override));

#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(bool, IsWebAttributionReportingAllowed, (), (override));
#endif
};

using MockAttributionReportingContentBrowserClient =
    MockAttributionReportingContentBrowserClientBase<TestContentBrowserClient>;

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_CONTENT_BROWSER_CLIENT_H_
