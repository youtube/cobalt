// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/net_fetcher.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "cobalt/storage/savegame_fake.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::StrictMock;

namespace cobalt {
namespace loader {

class NetFetcherTest : public ::testing::Test {
 public:
  std::unique_ptr<NetFetcher> net_fetcher_;

 protected:
  NetFetcherTest() {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>();
  }
  ~NetFetcherTest() override {}

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

class FetcherHandlerForTest : public Fetcher::Handler {
 public:
  explicit FetcherHandlerForTest(base::RunLoop* run_loop)
      : fetcher_(nullptr), run_loop_(run_loop) {}

  FetcherHandlerForTest() : fetcher_(nullptr) {}

  // From Fetcher::Handler.
  void OnReceived(Fetcher* fetcher, const char* data, size_t size) override {
    CheckFetcher(fetcher);
    data_.append(data, size);
  }
  void OnDone(Fetcher* fetcher) override {
    CheckFetcher(fetcher);
    if (!run_loop_) return;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop_->QuitClosure());
  }
  void OnError(Fetcher* fetcher, const std::string& error) override {
    CheckFetcher(fetcher);
    if (!run_loop_) return;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop_->QuitClosure());
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

TEST_F(NetFetcherTest, DISABLED_LocalFetch) {
  GURL url("http://localhost:8000/fetch.html");
  base::RunLoop run_loop;

  network::NetworkModule::Options network_module_options;
  network_module_options.ignore_certificate_errors = false;
  network_module_options.https_requirement = network::kHTTPSOptional;
  network_module_options.cors_policy = network::kCORSOptional;

  std::unique_ptr<network::NetworkModule> network_module;
  network_module.reset(new network::NetworkModule(network_module_options));
  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnResponseStarted(_, _)).Times(AtLeast(1));
  EXPECT_CALL(fetcher_handler_mock, OnReceived(_, _, _)).Times(AtLeast(1));
  EXPECT_CALL(fetcher_handler_mock, OnDone(_));

  csp::SecurityCallback security_callback = base::BindRepeating(
      [](const GURL& url, bool did_redirect) -> bool { return true; });
  NetFetcher::Options options;
  auto net_fetcher = std::make_unique<NetFetcher>(
      url, /*main_resource=*/false, security_callback, &fetcher_handler_mock,
      network_module.get(), options, kNoCORSMode, Origin(url));

  run_loop.Run();

  std::string loaded_text = fetcher_handler_mock.data();
  printf("%s\n", loaded_text.c_str());
}

}  // namespace loader
}  // namespace cobalt
