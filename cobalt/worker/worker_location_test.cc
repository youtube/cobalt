// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/worker/worker_location.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

// Note: The following tests duplicate the tests in URLUtilsTest, because
// WorkerLocation implements URLUtils.

TEST(WorkerLocationTest, GettersShouldReturnExpectedFormat) {
  scoped_refptr<WorkerLocation> worker_location(new WorkerLocation(
      GURL("https://user:pass@google.com:99/foo;bar?q=a#ref")));

  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?q=a#ref",
            worker_location->href());
  EXPECT_EQ("https:", worker_location->protocol());
  EXPECT_EQ("google.com:99", worker_location->host());
  EXPECT_EQ("google.com", worker_location->hostname());
  EXPECT_EQ("99", worker_location->port());
  EXPECT_EQ("/foo;bar", worker_location->pathname());
  EXPECT_EQ("#ref", worker_location->hash());
  EXPECT_EQ("?q=a", worker_location->search());
}

TEST(WorkerLocationTest, SetHref) {
  scoped_refptr<WorkerLocation> worker_location(new WorkerLocation(
      GURL("https://user:pass@google.com:99/foo;bar?q=a#ref")));
  worker_location->set_href("http://www.youtube.com");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("http://www.youtube.com/", worker_location->href());
}

TEST(WorkerLocationTest, SetProtocolShouldWorkAsExpected) {
  scoped_refptr<WorkerLocation> worker_location(new WorkerLocation(
      GURL("https://user:pass@google.com:99/foo;bar?q=a#ref")));
  worker_location->set_protocol("http");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref",
            worker_location->href());
}

TEST(WorkerLocationTest, SetHostShouldWorkAsExpected) {
  scoped_refptr<WorkerLocation> worker_location(new WorkerLocation(
      GURL("https://user:pass@google.com:99/foo;bar?q=a#ref")));
  worker_location->set_host("youtube.com");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("https://user:pass@youtube.com:99/foo;bar?q=a#ref",
            worker_location->href());

  worker_location->set_host("google.com:100");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:100/foo;bar?q=a#ref",
            worker_location->href());
}

TEST(WorkerLocationTest, SetHostnameShouldWorkAsExpected) {
  scoped_refptr<WorkerLocation> worker_location(new WorkerLocation(
      GURL("https://user:pass@google.com:99/foo;bar?q=a#ref")));
  worker_location->set_hostname("youtube.com");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("https://user:pass@youtube.com:99/foo;bar?q=a#ref",
            worker_location->href());
}

TEST(WorkerLocationTest, SetPortShouldWorkAsExpected) {
  scoped_refptr<WorkerLocation> worker_location(new WorkerLocation(
      GURL("https://user:pass@google.com:99/foo;bar?q=a#ref")));
  worker_location->set_port("100");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:100/foo;bar?q=a#ref",
            worker_location->href());
}

TEST(WorkerLocationTest, SetPathnameShouldWorkAsExpected) {
  scoped_refptr<WorkerLocation> worker_location(new WorkerLocation(
      GURL("https://user:pass@google.com:99/foo;bar?q=a#ref")));
  worker_location->set_pathname("baz");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/baz?q=a#ref",
            worker_location->href());
}

TEST(WorkerLocationTest, SetHashShouldWorkAsExpected) {
  scoped_refptr<WorkerLocation> worker_location(new WorkerLocation(
      GURL("https://user:pass@google.com:99/foo;bar?q=a#ref")));
  worker_location->set_hash("hash");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?q=a#hash",
            worker_location->href());

  worker_location->set_hash("#hash2");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?q=a#hash2",
            worker_location->href());
}

TEST(WorkerLocationTest, SetSearchShouldWorkAsExpected) {
  scoped_refptr<WorkerLocation> worker_location(new WorkerLocation(
      GURL("https://user:pass@google.com:99/foo;bar?q=a#ref")));
  worker_location->set_search("b=c");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?b=c#ref",
            worker_location->href());

  worker_location->set_search("?d=e");
  EXPECT_TRUE(worker_location->url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?d=e#ref",
            worker_location->href());
}

}  // namespace worker
}  // namespace cobalt
