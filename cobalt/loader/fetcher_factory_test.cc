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

#include <string>

#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/file_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace loader {
namespace {

class StubFetcherHandler : public Fetcher::Handler {
 public:
  // From Fetcher::Handler.
  void OnReceived(const char* data, size_t size) OVERRIDE {}
  void OnDone() OVERRIDE {}
  void OnError(const std::string& error) OVERRIDE {}
};

}  // namespace

class FetcherFactoryTest : public ::testing::Test {
 protected:
  FetcherFactoryTest()
      : message_loop_(MessageLoop::TYPE_DEFAULT),
        fetcher_factory_(NULL) {}
  ~FetcherFactoryTest() OVERRIDE {}

  MessageLoop message_loop_;
  StubFetcherHandler stub_fetcher_handler_;
  FetcherFactory fetcher_factory_;
  scoped_ptr<Fetcher> fetcher_;
};

TEST_F(FetcherFactoryTest, InvalidURL) {
  fetcher_ = fetcher_factory_.CreateFetcher(GURL("invalid-url"),
                                            &stub_fetcher_handler_);
  EXPECT_FALSE(fetcher_.get());
}

TEST_F(FetcherFactoryTest, EmptyURL) {
  fetcher_ =
      fetcher_factory_.CreateFetcher(GURL("file:///"), &stub_fetcher_handler_);
  EXPECT_FALSE(fetcher_.get());
}

TEST_F(FetcherFactoryTest, MultipleCreations) {
  fetcher_ = fetcher_factory_.CreateFetcher(GURL("file:///nonempty-url-1"),
                                            &stub_fetcher_handler_);
  EXPECT_NE(reinterpret_cast<FileFetcher*>(NULL),
            dynamic_cast<FileFetcher*>(fetcher_.get()));

  fetcher_ = fetcher_factory_.CreateFetcher(GURL("file:///nonempty-url-2"),
                                            &stub_fetcher_handler_);
  EXPECT_NE(reinterpret_cast<FileFetcher*>(NULL),
            dynamic_cast<FileFetcher*>(fetcher_.get()));
}

}  // namespace loader
}  // namespace cobalt
