// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_BROWSER_STATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_BROWSER_STATE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "ios/web/public/browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace net {
class CookieStore;
}  // namespace net

namespace web {
class FakeBrowserState final : public BrowserState {
 public:
  static const char kCorsExemptTestHeaderName[];

  FakeBrowserState();
  ~FakeBrowserState() override;

  // BrowserState:
  bool IsOffTheRecord() const override;
  base::FilePath GetStatePath() const override;
  net::URLRequestContextGetter* GetRequestContext() override;
  void UpdateCorsExemptHeader(
      network::mojom::NetworkContextParams* params) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;

  // Sets a SharedURLLoaderFactory for test.
  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  // Makes `IsOffTheRecord` return the given flag value.
  void SetOffTheRecord(bool flag);

  // This must be called before the first GetRequestContext() call.
  void SetCookieStore(std::unique_ptr<net::CookieStore> cookie_store);

 private:
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  bool is_off_the_record_;

  // A SharedURLLoaderFactory for test.
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  std::unique_ptr<net::CookieStore> cookie_store_;
};
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_BROWSER_STATE_H_
