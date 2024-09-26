// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_url_loader_request_interceptor.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/pdf/browser/fake_pdf_stream_delegate.h"
#include "components/pdf/browser/mock_url_loader_client.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace pdf {

namespace {

using ::testing::NiceMock;

class PdfURLLoaderRequestInterceptorTest
    : public content::RenderViewHostTestHarness {
 protected:
  PdfURLLoaderRequestInterceptorTest() {
    resource_request_.mode = network::mojom::RequestMode::kNavigate;
    resource_request_.url = GURL(FakePdfStreamDelegate::kDefaultOriginalUrl);
  }

  std::unique_ptr<PdfURLLoaderRequestInterceptor> CreateInterceptor() {
    return std::make_unique<PdfURLLoaderRequestInterceptor>(
        main_rfh()->GetFrameTreeNodeId(), std::move(stream_delegate_));
  }

  std::unique_ptr<FakePdfStreamDelegate> stream_delegate_ =
      std::make_unique<FakePdfStreamDelegate>();

  network::ResourceRequest resource_request_;
  base::MockCallback<content::URLLoaderRequestInterceptor::LoaderCallback>
      loader_callback_;
};

void RunRequestHandler(
    content::URLLoaderRequestInterceptor::RequestHandler request_handler) {
  base::RunLoop run_loop;

  NiceMock<MockURLLoaderClient> mock_client;
  EXPECT_CALL(mock_client, OnReceiveResponse).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(&mock_client);
  std::move(request_handler)
      .Run({}, {}, client_receiver.BindNewPipeAndPassRemote());

  run_loop.Run();
}

}  // namespace

TEST_F(PdfURLLoaderRequestInterceptorTest, MaybeCreateInterceptor) {
  EXPECT_TRUE(PdfURLLoaderRequestInterceptor::MaybeCreateInterceptor(
      main_rfh()->GetFrameTreeNodeId(), std::move(stream_delegate_)));
}

TEST_F(PdfURLLoaderRequestInterceptorTest, MaybeCreateLoader) {
  EXPECT_CALL(loader_callback_, Run(base::test::IsNotNullCallback()))
      .WillOnce(RunRequestHandler);

  auto interceptor = CreateInterceptor();
  interceptor->MaybeCreateLoader(resource_request_, browser_context(),
                                 loader_callback_.Get());
}

TEST_F(PdfURLLoaderRequestInterceptorTest, MaybeCreateLoaderNotNavigate) {
  EXPECT_CALL(loader_callback_, Run(base::test::IsNullCallback()));

  auto interceptor = CreateInterceptor();
  resource_request_.mode = network::mojom::RequestMode::kCors;
  interceptor->MaybeCreateLoader(resource_request_, browser_context(),
                                 loader_callback_.Get());
}

TEST_F(PdfURLLoaderRequestInterceptorTest, MaybeCreateLoaderDeleteContents) {
  EXPECT_CALL(loader_callback_, Run(base::test::IsNullCallback()));

  auto interceptor = CreateInterceptor();
  DeleteContents();
  interceptor->MaybeCreateLoader(resource_request_, browser_context(),
                                 loader_callback_.Get());
}

TEST_F(PdfURLLoaderRequestInterceptorTest, MaybeCreateLoaderNoStreamInfo) {
  EXPECT_CALL(loader_callback_, Run(base::test::IsNullCallback()));

  stream_delegate_->clear_stream_info();
  auto interceptor = CreateInterceptor();
  interceptor->MaybeCreateLoader(resource_request_, browser_context(),
                                 loader_callback_.Get());
}

TEST_F(PdfURLLoaderRequestInterceptorTest, MaybeCreateLoaderOtherUrl) {
  EXPECT_CALL(loader_callback_, Run(base::test::IsNullCallback()));

  auto interceptor = CreateInterceptor();
  resource_request_.url = GURL("https://example.test");
  interceptor->MaybeCreateLoader(resource_request_, browser_context(),
                                 loader_callback_.Get());
}

}  // namespace pdf
