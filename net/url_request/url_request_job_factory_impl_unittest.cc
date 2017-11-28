// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory_impl.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class MockURLRequestJob : public URLRequestJob {
 public:
  MockURLRequestJob(URLRequest* request,
                    NetworkDelegate* network_delegate,
                    const URLRequestStatus& status)
      : URLRequestJob(request, network_delegate),
        status_(status),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

  virtual void Start() override {
    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockURLRequestJob::StartAsync,
                   weak_factory_.GetWeakPtr()));
  }

 protected:
  virtual ~MockURLRequestJob() {}

 private:
  void StartAsync() {
    SetStatus(status_);
    NotifyHeadersComplete();
  }

  URLRequestStatus status_;
  base::WeakPtrFactory<MockURLRequestJob> weak_factory_;
};

class DummyProtocolHandler : public URLRequestJobFactory::ProtocolHandler {
 public:
  virtual URLRequestJob* MaybeCreateJob(
      URLRequest* request, NetworkDelegate* network_delegate) const override {
    return new MockURLRequestJob(
        request,
        network_delegate,
        URLRequestStatus(URLRequestStatus::SUCCESS, OK));
  }
};

class DummyInterceptor : public URLRequestJobFactory::Interceptor {
 public:
  DummyInterceptor()
      : did_intercept_(false),
        handle_all_protocols_(false) {
  }

  virtual URLRequestJob* MaybeIntercept(
      URLRequest* request, NetworkDelegate* network_delegate) const override {
    did_intercept_ = true;
    return new MockURLRequestJob(
        request,
        network_delegate,
        URLRequestStatus(URLRequestStatus::FAILED, ERR_FAILED));
  }

  virtual URLRequestJob* MaybeInterceptRedirect(
      const GURL&                       /* location */,
      URLRequest*                       /* request */,
      NetworkDelegate* network_delegate /* network delegate */) const override {
    return NULL;
  }

  virtual URLRequestJob* MaybeInterceptResponse(
      URLRequest*                       /* request */,
      NetworkDelegate* network_delegate /* network delegate */) const override {
    return NULL;
  }

  virtual bool WillHandleProtocol(
      const std::string& /* protocol */) const override {
    return handle_all_protocols_;
  }

  mutable bool did_intercept_;
  mutable bool handle_all_protocols_;
};

TEST(URLRequestJobFactoryTest, NoProtocolHandler) {
  TestDelegate delegate;
  TestURLRequestContext request_context;
  TestURLRequest request(GURL("foo://bar"), &delegate, &request_context);
  request.Start();

  MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, request.status().error());
}

TEST(URLRequestJobFactoryTest, BasicProtocolHandler) {
  TestDelegate delegate;
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  job_factory.SetProtocolHandler("foo", new DummyProtocolHandler);
  TestURLRequest request(GURL("foo://bar"), &delegate, &request_context);
  request.Start();

  MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::SUCCESS, request.status().status());
  EXPECT_EQ(OK, request.status().error());
}

TEST(URLRequestJobFactoryTest, DeleteProtocolHandler) {
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  job_factory.SetProtocolHandler("foo", new DummyProtocolHandler);
  job_factory.SetProtocolHandler("foo", NULL);
}

TEST(URLRequestJobFactoryTest, BasicInterceptor) {
  TestDelegate delegate;
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  job_factory.AddInterceptor(new DummyInterceptor);
  TestURLRequest request(GURL("http://bar"), &delegate, &request_context);
  request.Start();

  MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(ERR_FAILED, request.status().error());
}

TEST(URLRequestJobFactoryTest, InterceptorNeedsValidSchemeStill) {
  TestDelegate delegate;
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  job_factory.AddInterceptor(new DummyInterceptor);
  TestURLRequest request(GURL("foo://bar"), &delegate, &request_context);
  request.Start();

  MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, request.status().error());
}

TEST(URLRequestJobFactoryTest, InterceptorOverridesProtocolHandler) {
  TestDelegate delegate;
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  job_factory.SetProtocolHandler("foo", new DummyProtocolHandler);
  job_factory.AddInterceptor(new DummyInterceptor);
  TestURLRequest request(GURL("foo://bar"), &delegate, &request_context);
  request.Start();

  MessageLoop::current()->Run();
  EXPECT_EQ(URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(ERR_FAILED, request.status().error());
}

TEST(URLRequestJobFactoryTest, InterceptorDoesntInterceptUnknownProtocols) {
  TestDelegate delegate;
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  DummyInterceptor* interceptor = new DummyInterceptor;
  job_factory.AddInterceptor(interceptor);
  TestURLRequest request(GURL("foo://bar"), &delegate, &request_context);
  request.Start();

  MessageLoop::current()->Run();
  EXPECT_FALSE(interceptor->did_intercept_);
}

TEST(URLRequestJobFactoryTest, InterceptorInterceptsHandledUnknownProtocols) {
  TestDelegate delegate;
  URLRequestJobFactoryImpl job_factory;
  TestURLRequestContext request_context;
  request_context.set_job_factory(&job_factory);
  DummyInterceptor* interceptor = new DummyInterceptor;
  interceptor->handle_all_protocols_ = true;
  job_factory.AddInterceptor(interceptor);
  TestURLRequest request(GURL("foo://bar"), &delegate, &request_context);
  request.Start();

  MessageLoop::current()->Run();
  EXPECT_TRUE(interceptor->did_intercept_);
  EXPECT_EQ(URLRequestStatus::FAILED, request.status().status());
  EXPECT_EQ(ERR_FAILED, request.status().error());
}

TEST(URLRequestJobFactoryTest, InterceptorAffectsIsHandledProtocol) {
  DummyInterceptor* interceptor = new DummyInterceptor;
  URLRequestJobFactoryImpl job_factory;
  job_factory.AddInterceptor(interceptor);
  EXPECT_FALSE(interceptor->WillHandleProtocol("anything"));
  EXPECT_FALSE(job_factory.IsHandledProtocol("anything"));
  interceptor->handle_all_protocols_ = true;
  EXPECT_TRUE(interceptor->WillHandleProtocol("anything"));
  EXPECT_TRUE(job_factory.IsHandledProtocol("anything"));
}

}  // namespace

}  // namespace net
