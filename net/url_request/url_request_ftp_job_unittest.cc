// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_auth_cache.h"
#include "net/ftp/ftp_transaction.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_ftp_job.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::_;

namespace net {

class MockFtpTransactionFactory : public FtpTransactionFactory {
 public:
  MOCK_METHOD0(CreateTransaction, FtpTransaction*());
  MOCK_METHOD1(Suspend, void(bool suspend));
};

class MockURLRequestDelegate : public URLRequest::Delegate {
 public:
  MOCK_METHOD3(OnReceivedRedirect, void(URLRequest* request,
                                        const GURL& new_url,
                                        bool* defer_redirect));
  MOCK_METHOD2(OnAuthRequired, void(URLRequest* request,
                                    AuthChallengeInfo* auth_info));
  MOCK_METHOD2(OnCertificateRequested,
               void(URLRequest* request,
                    SSLCertRequestInfo* cert_request_info));
  MOCK_METHOD3(OnSSLCertificateError, void(URLRequest* request,
                                           const SSLInfo& ssl_info,
                                           bool fatal));
  MOCK_METHOD1(OnResponseStarted, void(URLRequest* request));
  MOCK_METHOD2(OnReadCompleted, void(URLRequest* request, int bytes_read));
};

ACTION_P(HandleOnResponseStarted, expected_status) {
  EXPECT_EQ(expected_status, arg0->status().status());
}

TEST(FtpProtocolHandlerTest, CreateTransactionFails) {
  testing::InSequence in_sequence_;

  ::testing::StrictMock<MockFtpTransactionFactory> ftp_transaction_factory;
  ::testing::StrictMock<MockURLRequestDelegate> delegate;
  FtpAuthCache ftp_auth_cache;

  GURL url("ftp://example.com");
  URLRequestContext context;
  URLRequest url_request(url, &delegate, &context);

  FtpProtocolHandler ftp_protocol_handler(
      NULL, &ftp_transaction_factory, &ftp_auth_cache);

  scoped_refptr<URLRequestJob> ftp_job(
      ftp_protocol_handler.MaybeCreateJob(&url_request));
  ASSERT_TRUE(ftp_job.get());

  EXPECT_CALL(ftp_transaction_factory, CreateTransaction())
      .WillOnce(Return(static_cast<FtpTransaction*>(NULL)));
  ftp_job->Start();
  EXPECT_CALL(delegate, OnResponseStarted(_))
      .WillOnce(HandleOnResponseStarted(URLRequestStatus::FAILED));
  MessageLoop::current()->RunAllPending();
  EXPECT_FALSE(url_request.is_pending());
}

}  // namespace net
