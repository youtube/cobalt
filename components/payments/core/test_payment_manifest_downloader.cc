// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/test_payment_manifest_downloader.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "components/payments/core/error_logger.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace payments {

TestDownloader::TestDownloader(
    base::WeakPtr<CSPChecker> csp_checker,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : PaymentManifestDownloader(std::make_unique<ErrorLogger>(),
                                csp_checker,
                                url_loader_factory) {}

TestDownloader::~TestDownloader() {}

void TestDownloader::AddTestServerURL(const std::string& prefix,
                                      const GURL& test_server_url) {
  test_server_url_[prefix] = test_server_url;
}

void TestDownloader::ResetTestState() {
  did_complete_download_ = false;
}

GURL TestDownloader::FindTestServerURL(const GURL& url) const {
  // Find the first key in |test_server_url_| that is a prefix of |url|. If
  // found, then replace this prefix in the |url| with the URL of the test
  // server that should be used instead.
  for (const auto& prefix_and_url : test_server_url_) {
    const std::string& prefix = prefix_and_url.first;
    const GURL& test_server_url = prefix_and_url.second;
    if (base::StartsWith(url.spec(), prefix, base::CompareCase::SENSITIVE) &&
        !base::StartsWith(url.spec(), test_server_url.spec(),
                          base::CompareCase::SENSITIVE)) {
      return GURL(test_server_url.spec() + url.spec().substr(prefix.length()));
    }
  }

  return url;
}

void TestDownloader::SetCSPCheckerForTesting(
    base::WeakPtr<CSPChecker> csp_checker) {
  csp_checker_ = csp_checker;
}

void TestDownloader::InitiateDownload(
    const url::Origin& request_initiator,
    const GURL& url,
    const GURL& url_before_redirects,
    bool did_follow_redirect,
    Download::Type download_type,
    int allowed_number_of_redirects,
    PaymentManifestDownloadCallback callback) {
  PaymentManifestDownloader::InitiateDownload(
      request_initiator, FindTestServerURL(url),
      FindTestServerURL(url_before_redirects), did_follow_redirect,
      download_type, allowed_number_of_redirects,
      base::BindOnce(&TestDownloader::OnDownloadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TestDownloader::OnDownloadCompleted(
    PaymentManifestDownloadCallback callback,
    const GURL& url,
    const std::string& contents,
    const std::string& error_message) {
  did_complete_download_ = true;
  std::move(callback).Run(url, contents, error_message);
}

}  // namespace payments
