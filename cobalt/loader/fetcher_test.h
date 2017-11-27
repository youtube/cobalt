// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_FETCHER_TEST_H_
#define COBALT_LOADER_FETCHER_TEST_H_

#include "cobalt/loader/fetcher.h"

#include <string>

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Invoke;
using ::testing::_;

namespace cobalt {
namespace loader {

class FetcherHandlerForTest : public Fetcher::Handler {
 public:
  explicit FetcherHandlerForTest(base::RunLoop* run_loop)
      : fetcher_(NULL), run_loop_(run_loop) {}

  // From Fetcher::Handler.
  void OnReceived(Fetcher* fetcher, const char* data, size_t size) override {
    CheckFetcher(fetcher);
    data_.append(data, size);
  }
  void OnDone(Fetcher* fetcher) override {
    CheckFetcher(fetcher);
    MessageLoop::current()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }
  void OnError(Fetcher* fetcher, const std::string& error) override {
    UNREFERENCED_PARAMETER(error);
    CheckFetcher(fetcher);
    MessageLoop::current()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

  const std::string& data() const { return data_; }
  Fetcher* fetcher() const { return fetcher_; }

 private:
  void CheckFetcher(Fetcher* fetcher) {
    EXPECT_TRUE(fetcher);
    if (fetcher_ == NULL) {
      fetcher_ = fetcher;
      return;
    }
    EXPECT_EQ(fetcher_, fetcher);
  }

  std::string data_;
  Fetcher* fetcher_;
  base::RunLoop* run_loop_;
};

class MockFetcherHandler : public Fetcher::Handler {
 public:
  explicit MockFetcherHandler(base::RunLoop* run_loop) : real_(run_loop) {
    ON_CALL(*this, OnReceived(_, _, _))
        .WillByDefault(Invoke(&real_, &FetcherHandlerForTest::OnReceived));
    ON_CALL(*this, OnDone(_))
        .WillByDefault(Invoke(&real_, &FetcherHandlerForTest::OnDone));
    ON_CALL(*this, OnError(_, _))
        .WillByDefault(Invoke(&real_, &FetcherHandlerForTest::OnError));
  }

  MOCK_METHOD2(OnResponseStarted,
               LoadResponseType(
                   Fetcher*, const scoped_refptr<net::HttpResponseHeaders>&));
  MOCK_METHOD3(OnReceived, void(Fetcher*, const char*, size_t));
  MOCK_METHOD1(OnDone, void(Fetcher*));
  MOCK_METHOD2(OnError, void(Fetcher*, const std::string&));

  const std::string& data() const { return real_.data(); }
  Fetcher* fetcher() const { return real_.fetcher(); }

 private:
  FetcherHandlerForTest real_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FETCHER_TEST_H_
