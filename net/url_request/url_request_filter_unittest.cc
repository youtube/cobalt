// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_filter.h"

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

URLRequestTestJob* job_a;

URLRequestJob* FactoryA(URLRequest* request, const std::string& scheme) {
  job_a = new URLRequestTestJob(request);
  return job_a;
}

URLRequestTestJob* job_b;

URLRequestJob* FactoryB(URLRequest* request, const std::string& scheme) {
  job_b = new URLRequestTestJob(request);
  return job_b;
}

TEST(URLRequestFilter, BasicMatching) {
  TestDelegate delegate;
  TestURLRequestContext request_context;

  GURL url_1("http://foo.com/");
  TestURLRequest request_1(url_1, &delegate, &request_context);

  GURL url_2("http://bar.com/");
  TestURLRequest request_2(url_2, &delegate, &request_context);

  // Check AddUrlHandler checks for invalid URLs.
  EXPECT_FALSE(URLRequestFilter::GetInstance()->AddUrlHandler(GURL(),
                                                              &FactoryA));

  // Check URL matching.
  URLRequestFilter::GetInstance()->ClearHandlers();
  EXPECT_TRUE(URLRequestFilter::GetInstance()->AddUrlHandler(url_1,
                                                             &FactoryA));
  {
    scoped_refptr<URLRequestJob> found = URLRequestFilter::Factory(
        &request_1, url_1.scheme());
    EXPECT_EQ(job_a, found);
    EXPECT_TRUE(job_a != NULL);
    job_a = NULL;
  }
  EXPECT_EQ(URLRequestFilter::GetInstance()->hit_count(), 1);

  // Check we don't match other URLs.
  EXPECT_TRUE(URLRequestFilter::Factory(&request_2, url_2.scheme()) == NULL);
  EXPECT_EQ(1, URLRequestFilter::GetInstance()->hit_count());

  // Check we can overwrite URL handler.
  EXPECT_TRUE(URLRequestFilter::GetInstance()->AddUrlHandler(url_1,
                                                             &FactoryB));
  {
    scoped_refptr<URLRequestJob> found = URLRequestFilter::Factory(
        &request_1, url_1.scheme());
    EXPECT_EQ(job_b, found);
    EXPECT_TRUE(job_b != NULL);
    job_b = NULL;
  }
  EXPECT_EQ(2, URLRequestFilter::GetInstance()->hit_count());

  // Check we can remove URL matching.
  URLRequestFilter::GetInstance()->RemoveUrlHandler(url_1);
  EXPECT_TRUE(URLRequestFilter::Factory(&request_1, url_1.scheme()) == NULL);
  EXPECT_EQ(URLRequestFilter::GetInstance()->hit_count(), 2);

  // Check hostname matching.
  URLRequestFilter::GetInstance()->ClearHandlers();
  EXPECT_EQ(0, URLRequestFilter::GetInstance()->hit_count());
  URLRequestFilter::GetInstance()->AddHostnameHandler(url_1.scheme(),
                                                      url_1.host(),
                                                      &FactoryB);
  {
    scoped_refptr<URLRequestJob> found = URLRequestFilter::Factory(
        &request_1, url_1.scheme());
    EXPECT_EQ(job_b, found);
    EXPECT_TRUE(job_b != NULL);
    job_b = NULL;
  }
  EXPECT_EQ(1, URLRequestFilter::GetInstance()->hit_count());

  // Check we don't match other hostnames.
  EXPECT_TRUE(URLRequestFilter::Factory(&request_2, url_2.scheme()) == NULL);
  EXPECT_EQ(URLRequestFilter::GetInstance()->hit_count(), 1);

  // Check we can overwrite hostname handler.
  URLRequestFilter::GetInstance()->AddHostnameHandler(url_1.scheme(),
                                                      url_1.host(),
                                                      &FactoryA);
  {
    scoped_refptr<URLRequestJob> found = URLRequestFilter::Factory(
        &request_1, url_1.scheme());
    EXPECT_EQ(job_a, found);
    EXPECT_TRUE(job_a != NULL);
    job_a = NULL;
  }
  EXPECT_EQ(2, URLRequestFilter::GetInstance()->hit_count());

  // Check we can remove hostname matching.
  URLRequestFilter::GetInstance()->RemoveHostnameHandler(url_1.scheme(),
                                                         url_1.host());
  EXPECT_TRUE(URLRequestFilter::Factory(&request_1, url_1.scheme()) == NULL);
  EXPECT_EQ(2, URLRequestFilter::GetInstance()->hit_count());
}

}  // namespace

}  // namespace net
