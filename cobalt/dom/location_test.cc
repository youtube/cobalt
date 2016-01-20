/*
* Copyright 2015 Google Inc. All Rights Reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     https://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "cobalt/dom/location.h"

#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

namespace {
void DummyNavigateCallback(const GURL& /*url*/) {}
bool DummySecurityCallback(const GURL& /*url*/, bool /*did_redirect*/) {
  return true;
}
}

class LocationTest : public ::testing::Test {
 protected:
  void Init(const GURL& url) {
    location_ = new Location(url, base::Bind(&DummyNavigateCallback),
                             base::Bind(&DummySecurityCallback));
  }
  scoped_refptr<Location> location_;
};

TEST_F(LocationTest, AssignShouldNotChangeURLPartsOtherThanHash) {
  Init(GURL("https://www.youtube.com/tv/foo;bar?q=a#ref"));
  location_->Assign("https://www.youtube.com/tv/foo;bar?q=a#new-ref");
  EXPECT_EQ("https://www.youtube.com/tv/foo;bar?q=a#new-ref",
            location_->href());

  location_->Assign("https://www.google.com/");
  EXPECT_EQ("https://www.youtube.com/tv/foo;bar?q=a#new-ref",
            location_->href());
}

TEST_F(LocationTest, Href) {
  Init(GURL("https://www.youtube.com/tv/foo;bar?q=a#ref"));
  EXPECT_EQ("https://www.youtube.com/tv/foo;bar?q=a#ref", location_->href());

  location_->set_href("https://www.youtube.com/tv/foo;bar?q=a#new-ref");
  EXPECT_EQ("https://www.youtube.com/tv/foo;bar?q=a#new-ref",
            location_->href());
}

TEST_F(LocationTest, Protocol) {
  Init(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("https:", location_->protocol());
}

TEST_F(LocationTest, Host) {
  Init(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("google.com:99", location_->host());
}

TEST_F(LocationTest, Hostname) {
  Init(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("google.com", location_->hostname());
}

TEST_F(LocationTest, Port) {
  Init(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("99", location_->port());
}

TEST_F(LocationTest, Hash) {
  Init(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("#ref", location_->hash());

  location_->set_hash("new-ref");
  EXPECT_EQ("#new-ref", location_->hash());
}

TEST_F(LocationTest, Search) {
  Init(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("?q=a", location_->search());
}

}  // namespace dom
}  // namespace cobalt
