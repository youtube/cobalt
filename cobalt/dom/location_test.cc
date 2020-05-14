// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/dom/location.h"

#include "base/memory/ref_counted.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace cobalt {
namespace dom {

// Base class to assist in mocking Location's use of its navigation and
// security callbacks.
class LocationCallbackInterface {
 public:
  virtual bool CanLoad(const GURL& url, bool did_redirect) = 0;
  virtual void FireHashChangeEvent() = 0;
  virtual void Navigate(const GURL& url) = 0;
};

class MockSecurityCallback : public LocationCallbackInterface {
 public:
  MOCK_METHOD2(CanLoad, bool(const GURL&, bool));
  MOCK_METHOD0(FireHashChangeEvent, void());
  MOCK_METHOD1(Navigate, void(const GURL&));
  csp::SecurityCallback LoadCb() {
    return base::Bind(&MockSecurityCallback::CanLoad, base::Unretained(this));
  }
  base::Closure HashChangeCb() {
    return base::Bind(&MockSecurityCallback::FireHashChangeEvent,
                      base::Unretained(this));
  }
  base::Callback<void(const GURL&)> NavigateCb() {
    return base::Bind(&MockSecurityCallback::Navigate, base::Unretained(this));
  }
};

// Tests modifying the location with both a "strict" and a "permissive" policy.
// If param is false, we return false from the security callback, and the
// navigate callback should *not* be called.
// If param is true, we return true from the security callback, and expect the
// navigate callback to be called with our expected new URL.
class LocationTestWithParams : public testing::TestWithParam<bool> {
 protected:
  void Init(const GURL& url, const base::Closure& hashchange_callback,
            const base::Callback<void(const GURL&)>& navigation_callback,
            const csp::SecurityCallback& security_callback) {
    location_ = new Location(url, hashchange_callback, navigation_callback,
                             security_callback);
  }

  scoped_refptr<Location> location_;
};

TEST_P(LocationTestWithParams, SetHash) {
  StrictMock<MockSecurityCallback> security_mock;
  GURL url("https://www.example.com/foo");
  GURL new_url("https://www.example.com/foo#bar");
  bool permissive = GetParam();
  Init(url, security_mock.HashChangeCb(), security_mock.NavigateCb(),
       security_mock.LoadCb());

  EXPECT_CALL(security_mock, CanLoad(new_url, _)).WillOnce(Return(permissive));
  if (permissive) {
    EXPECT_CALL(security_mock, FireHashChangeEvent());
  }
  location_->set_hash("bar");
}

TEST_P(LocationTestWithParams, SetHost) {
  StrictMock<MockSecurityCallback> security_mock;
  GURL url("https://www.example.com:1234");
  GURL new_url("https://www.google.com:5678");
  bool permissive = GetParam();
  Init(url, security_mock.HashChangeCb(), security_mock.NavigateCb(),
       security_mock.LoadCb());

  EXPECT_CALL(security_mock, CanLoad(new_url, _)).WillOnce(Return(permissive));
  if (permissive) {
    EXPECT_CALL(security_mock, Navigate(new_url));
  }
  location_->set_host("www.google.com:5678");
}

TEST_P(LocationTestWithParams, SetHostname) {
  StrictMock<MockSecurityCallback> security_mock;
  GURL url("https://www.example.com");
  GURL new_url("https://www.google.com");
  bool permissive = GetParam();
  Init(url, security_mock.HashChangeCb(), security_mock.NavigateCb(),
       security_mock.LoadCb());

  EXPECT_CALL(security_mock, CanLoad(new_url, _)).WillOnce(Return(permissive));
  if (permissive) {
    EXPECT_CALL(security_mock, Navigate(new_url));
  }
  location_->set_hostname("www.google.com");
}

TEST_P(LocationTestWithParams, SetHref) {
  StrictMock<MockSecurityCallback> security_mock;
  GURL url("https://www.example.com");
  GURL new_url("http://www.google.com:1234/path");
  bool permissive = GetParam();
  Init(url, security_mock.HashChangeCb(), security_mock.NavigateCb(),
       security_mock.LoadCb());

  EXPECT_CALL(security_mock, CanLoad(new_url, _)).WillOnce(Return(permissive));
  if (permissive) {
    EXPECT_CALL(security_mock, Navigate(new_url));
  }
  location_->set_href(new_url.spec());
}

TEST_P(LocationTestWithParams, SetProtocol) {
  StrictMock<MockSecurityCallback> security_mock;
  GURL url("https://www.example.com");
  GURL new_url("http://www.example.com");
  bool permissive = GetParam();
  Init(url, security_mock.HashChangeCb(), security_mock.NavigateCb(),
       security_mock.LoadCb());

  EXPECT_CALL(security_mock, CanLoad(new_url, _)).WillOnce(Return(permissive));
  if (permissive) {
    EXPECT_CALL(security_mock, Navigate(new_url));
  }
  location_->set_protocol("http");
}

INSTANTIATE_TEST_CASE_P(SecurityTests, LocationTestWithParams,
                        ::testing::Bool());

}  // namespace dom
}  // namespace cobalt
