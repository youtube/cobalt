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

TEST(LocationTest, AssignShouldNotChangeURLPartsOtherThanHash) {
  scoped_refptr<Location> location =
      new Location(GURL("https://www.youtube.com/tv/foo;bar?q=a#ref"));
  location->Assign("https://www.youtube.com/tv/foo;bar?q=a#new-ref");
  EXPECT_EQ("https://www.youtube.com/tv/foo;bar?q=a#new-ref", location->href());

  location->Assign("https://www.google.com/");
  EXPECT_EQ("https://www.youtube.com/tv/foo;bar?q=a#new-ref", location->href());
}

TEST(LocationTest, Href) {
  scoped_refptr<Location> location =
      new Location(GURL("https://www.youtube.com/tv/foo;bar?q=a#ref"));
  EXPECT_EQ("https://www.youtube.com/tv/foo;bar?q=a#ref", location->href());

  location->set_href("https://www.youtube.com/tv/foo;bar?q=a#new-ref");
  EXPECT_EQ("https://www.youtube.com/tv/foo;bar?q=a#new-ref", location->href());
}

TEST(LocationTest, Protocol) {
  scoped_refptr<Location> location =
      new Location(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("https:", location->protocol());
}

TEST(LocationTest, Host) {
  scoped_refptr<Location> location =
      new Location(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("google.com:99", location->host());
}

TEST(LocationTest, Hostname) {
  scoped_refptr<Location> location =
      new Location(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("google.com", location->hostname());
}

TEST(LocationTest, Port) {
  scoped_refptr<Location> location =
      new Location(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("99", location->port());
}

TEST(LocationTest, Hash) {
  scoped_refptr<Location> location =
      new Location(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("#ref", location->hash());

  location->set_hash("new-ref");
  EXPECT_EQ("#new-ref", location->hash());
}

TEST(LocationTest, Search) {
  scoped_refptr<Location> location =
      new Location(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_EQ("?q=a", location->search());
}

}  // namespace dom
}  // namespace cobalt
