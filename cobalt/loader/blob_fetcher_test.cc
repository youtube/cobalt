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

#include "cobalt/loader/blob_fetcher.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "cobalt/loader/fetcher_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::_;

namespace cobalt {
namespace loader {
namespace {

typedef std::map<std::string, std::vector<char> > TestRegistry;

bool TestResolver(const TestRegistry& registry, const GURL& url,
                  const char** data, size_t* size) {
  DCHECK(data);
  DCHECK(size);

  *size = 0;
  *data = NULL;

  TestRegistry::const_iterator match = registry.find(url.spec());
  if (match != registry.end()) {
    const std::vector<char>& buffer = match->second;
    *size = buffer.size();

    if (!buffer.empty()) {
      *data = &buffer[0];
    }

    return true;
  } else {
    return false;
  }
}

TEST(BlobFetcherTest, NonExistentBlobURL) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnError(_, _));

  TestRegistry registry;

  std::unique_ptr<BlobFetcher> blob_fetcher = base::WrapUnique(
      new loader::BlobFetcher(GURL("blob:sd98sdfuh8sdh"), &fetcher_handler_mock,
                              base::Bind(&TestResolver, registry)));

  run_loop.Run();

  EXPECT_EQ(blob_fetcher.get(), fetcher_handler_mock.fetcher());
}

TEST(BlobFetcherTest, EmptyBlob) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;
  InSequence dummy;

  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnDone(_));

  const char* url = "blob:28y3-fsdaf-dsfa";

  TestRegistry registry;
  registry[url];

  std::unique_ptr<BlobFetcher> blob_fetcher = base::WrapUnique(
      new loader::BlobFetcher(GURL(url), &fetcher_handler_mock,
                              base::Bind(&TestResolver, registry)));

  run_loop.Run();

  EXPECT_EQ(0, fetcher_handler_mock.data().size());

  EXPECT_EQ(blob_fetcher.get(), fetcher_handler_mock.fetcher());
}

TEST(BlobFetcherTest, ValidBlob) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;
  InSequence dummy;

  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnReceived(_, _, _)).Times(AtLeast(1));
  EXPECT_CALL(fetcher_handler_mock, OnDone(_));

  const char* url = "blob:28y3-fsdaf-dsfa";

  TestRegistry registry;
  std::vector<char>& buffer = registry[url];
  buffer.push_back('a');
  buffer.push_back(0);
  buffer.push_back(7);

  std::unique_ptr<BlobFetcher> blob_fetcher = base::WrapUnique(
      new loader::BlobFetcher(GURL(url), &fetcher_handler_mock,
                              base::Bind(&TestResolver, registry)));

  run_loop.Run();

  const std::string& data = fetcher_handler_mock.data();
  ASSERT_EQ(3, data.size());
  EXPECT_EQ('a', data[0]);
  EXPECT_EQ(0, data[1]);
  EXPECT_EQ(7, data[2]);

  EXPECT_EQ(blob_fetcher.get(), fetcher_handler_mock.fetcher());
}

}  // namespace
}  // namespace loader
}  // namespace cobalt
