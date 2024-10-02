// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_search_api/url_checker.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace safe_search_api {

namespace {

constexpr size_t kCacheSize = 2;

const char* kURLs[] = {
    "http://www.randomsite1.com", "http://www.randomsite2.com",
    "http://www.randomsite3.com", "http://www.randomsite4.com",
    "http://www.randomsite5.com", "http://www.randomsite6.com",
    "http://www.randomsite7.com", "http://www.randomsite8.com",
    "http://www.randomsite9.com",
};

ClientClassification ToAPIClassification(Classification classification,
                                         bool uncertain) {
  if (uncertain) {
    return ClientClassification::kUnknown;
  }
  switch (classification) {
    case Classification::SAFE:
      return ClientClassification::kAllowed;
    case Classification::UNSAFE:
      return ClientClassification::kRestricted;
  }
}

}  // namespace

class SafeSearchURLCheckerTest : public testing::Test {
 public:
  SafeSearchURLCheckerTest() : next_url_(0) {
    std::unique_ptr<FakeURLCheckerClient> fake_client =
        std::make_unique<FakeURLCheckerClient>();
    fake_client_ = fake_client.get();
    checker_ = std::make_unique<URLChecker>(std::move(fake_client), kCacheSize);
  }

  MOCK_METHOD3(OnCheckDone,
               void(const GURL& url,
                    Classification classification,
                    bool uncertain));

 protected:
  GURL GetNewURL() {
    CHECK(next_url_ < std::size(kURLs));
    return GURL(kURLs[next_url_++]);
  }

  // Returns true if the result was returned synchronously (cache hit).
  bool CheckURL(const GURL& url) {
    bool cached = checker_->CheckURL(
        url, base::BindOnce(&SafeSearchURLCheckerTest::OnCheckDone,
                            base::Unretained(this)));
    return cached;
  }

  bool SendResponse(const GURL& url,
                    Classification classification,
                    bool uncertain) {
    bool result = CheckURL(url);
    fake_client_->RunCallback(ToAPIClassification(classification, uncertain));
    return result;
  }

  size_t next_url_;
  raw_ptr<FakeURLCheckerClient> fake_client_;
  std::unique_ptr<URLChecker> checker_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SafeSearchURLCheckerTest, Simple) {
  {
    GURL url(GetNewURL());
    EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, false));
    ASSERT_FALSE(SendResponse(url, Classification::SAFE, false));
  }
  {
    GURL url(GetNewURL());
    EXPECT_CALL(*this, OnCheckDone(url, Classification::UNSAFE, false));
    ASSERT_FALSE(SendResponse(url, Classification::UNSAFE, false));
  }
  {
    GURL url(GetNewURL());
    EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, true));
    ASSERT_FALSE(SendResponse(url, Classification::SAFE, true));
  }
}

TEST_F(SafeSearchURLCheckerTest, Cache) {
  // One more URL than fit in the cache.
  ASSERT_EQ(2u, kCacheSize);
  GURL url1(GetNewURL());
  GURL url2(GetNewURL());
  GURL url3(GetNewURL());

  // Populate the cache.
  EXPECT_CALL(*this, OnCheckDone(url1, Classification::SAFE, false));
  ASSERT_FALSE(SendResponse(url1, Classification::SAFE, false));
  EXPECT_CALL(*this, OnCheckDone(url2, Classification::SAFE, false));
  ASSERT_FALSE(SendResponse(url2, Classification::SAFE, false));

  // Now we should get results synchronously, without a request to the api.
  EXPECT_CALL(*this, OnCheckDone(url2, Classification::SAFE, false));
  ASSERT_TRUE(CheckURL(url2));
  EXPECT_CALL(*this, OnCheckDone(url1, Classification::SAFE, false));
  ASSERT_TRUE(CheckURL(url1));

  // Now |url2| is the LRU and should be evicted on the next check.
  EXPECT_CALL(*this, OnCheckDone(url3, Classification::SAFE, false));
  ASSERT_FALSE(SendResponse(url3, Classification::SAFE, false));

  EXPECT_CALL(*this, OnCheckDone(url2, Classification::SAFE, false));
  ASSERT_FALSE(SendResponse(url2, Classification::SAFE, false));
}

TEST_F(SafeSearchURLCheckerTest, CoalesceRequestsToSameURL) {
  GURL url(GetNewURL());
  // Start two checks for the same URL.
  ASSERT_FALSE(CheckURL(url));
  ASSERT_FALSE(CheckURL(url));
  // A single response should answer both of those checks
  EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, false)).Times(2);
  fake_client_->RunCallback(ToAPIClassification(Classification::SAFE, false));
}

TEST_F(SafeSearchURLCheckerTest, CacheTimeout) {
  GURL url(GetNewURL());

  checker_->SetCacheTimeoutForTesting(base::Seconds(0));

  EXPECT_CALL(*this, OnCheckDone(url, Classification::SAFE, false));
  ASSERT_FALSE(SendResponse(url, Classification::SAFE, false));

  // Since the cache timeout is zero, the cache entry should be invalidated
  // immediately.
  EXPECT_CALL(*this, OnCheckDone(url, Classification::UNSAFE, false));
  ASSERT_FALSE(SendResponse(url, Classification::UNSAFE, false));
}

TEST_F(SafeSearchURLCheckerTest, DestroyURLCheckerBeforeCallback) {
  GURL url(GetNewURL());
  EXPECT_CALL(*this, OnCheckDone(_, _, _)).Times(0);

  // Start a URL check.
  ASSERT_FALSE(CheckURL(url));
  fake_client_->RunCallbackAsync(
      ToAPIClassification(Classification::SAFE, false));

  // Reset the URLChecker before the callback occurs.
  checker_.reset();

  // The callback should now be invalid.
  task_environment_.RunUntilIdle();
}

}  // namespace safe_search_api
