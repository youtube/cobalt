// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/tailored_security_url_observer.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

class MockTailoredSecurityService : public TailoredSecurityService {
 public:
  MockTailoredSecurityService() : TailoredSecurityService(nullptr, nullptr) {}
  MOCK_METHOD(void, AddQueryRequest, (), (override));
  MOCK_METHOD(void, RemoveQueryRequest, (), (override));
  MOCK_METHOD(void, MaybeNotifySyncUser, (bool, base::Time), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
};

}  // namespace

using TailoredSecurityUrlObserverTest = ChromeRenderViewHostTestHarness;

TEST_F(TailoredSecurityUrlObserverTest, QueryRequestOnFocus) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityUrlObserver::CreateForWebContents(web_contents(),
                                                    &mock_service);
  TailoredSecurityUrlObserver* url_observer =
      TailoredSecurityUrlObserver::FromWebContents(web_contents());

  EXPECT_CALL(mock_service, AddQueryRequest());
  NavigateAndCommit(GURL("https://google.com"));

  EXPECT_CALL(mock_service, RemoveQueryRequest());
  url_observer->OnWebContentsLostFocus(nullptr);
}

TEST_F(TailoredSecurityUrlObserverTest, QueryRequestOnNavigation) {
  MockTailoredSecurityService mock_service;
  TailoredSecurityUrlObserver::CreateForWebContents(web_contents(),
                                                    &mock_service);
  TailoredSecurityUrlObserver* url_observer =
      TailoredSecurityUrlObserver::FromWebContents(web_contents());

  url_observer->OnWebContentsFocused(nullptr);

  EXPECT_CALL(mock_service, AddQueryRequest());
  NavigateAndCommit(GURL("https://google.com"));

  EXPECT_CALL(mock_service, RemoveQueryRequest());
  NavigateAndCommit(GURL("https://example.com"));
}

}  // namespace safe_browsing
