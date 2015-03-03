/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/location.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

TEST(LocationTest, LocationTest) {
  scoped_refptr<Location> location;

  location = make_scoped_refptr(
      new Location(GURL("http://user:pass@google.com:99/foo;bar?q=a#ref")));
  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", location->href());
  EXPECT_EQ("?q=a", location->search());

  location = make_scoped_refptr(
      new Location(GURL("http://user:pass@google.com:99/foo;bar#ref")));
  EXPECT_EQ("http://user:pass@google.com:99/foo;bar#ref", location->href());
  EXPECT_EQ("", location->search());
}

}  // namespace dom
}  // namespace cobalt
