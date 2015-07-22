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

#include "cobalt/loader/file_fetcher.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::_;

namespace cobalt {
namespace loader {
namespace {

class FetcherHandlerForTest : public Fetcher::Handler {
 public:
  explicit FetcherHandlerForTest(base::RunLoop* run_loop)
      : run_loop_(run_loop) {}

  // From Fetcher::Handler.
  void OnReceived(const char* data, size_t size) OVERRIDE {
    data_.append(data, size);
  }
  void OnDone() OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }
  void OnError(const std::string& error) OVERRIDE {
    UNREFERENCED_PARAMETER(error);
    MessageLoop::current()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

  std::string data() { return data_; }

 private:
  std::string data_;
  base::RunLoop* run_loop_;
};

class MockFetcherHandler : public Fetcher::Handler {
 public:
  explicit MockFetcherHandler(base::RunLoop* run_loop) : real_(run_loop) {
    ON_CALL(*this, OnReceived(_, _))
        .WillByDefault(Invoke(&real_, &FetcherHandlerForTest::OnReceived));
    ON_CALL(*this, OnDone())
        .WillByDefault(Invoke(&real_, &FetcherHandlerForTest::OnDone));
    ON_CALL(*this, OnError(_))
        .WillByDefault(Invoke(&real_, &FetcherHandlerForTest::OnError));
    ON_CALL(*this, data())
        .WillByDefault(Invoke(&real_, &FetcherHandlerForTest::data));
  }

  MOCK_METHOD2(OnReceived, void(const char*, size_t));
  MOCK_METHOD0(OnDone, void());
  MOCK_METHOD1(OnError, void(const std::string&));
  MOCK_METHOD0(data, std::string());

 private:
  FetcherHandlerForTest real_;
};

}  // namespace

class FileFetcherTest : public ::testing::Test {
 protected:
  FileFetcherTest();
  ~FileFetcherTest() OVERRIDE {}

  FilePath data_dir_;
  FilePath dir_source_root_;
  MessageLoop message_loop_;
  scoped_ptr<FileFetcher> file_fetcher_;
};

FileFetcherTest::FileFetcherTest() : message_loop_(MessageLoop::TYPE_DEFAULT) {
  data_dir_ = data_dir_.Append(FILE_PATH_LITERAL("cobalt"))
                  .Append(FILE_PATH_LITERAL("loader"))
                  .Append(FILE_PATH_LITERAL("testdata"));
  PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root_);
}

TEST_F(FileFetcherTest, NonExisitingPath) {
  base::RunLoop run_loop;

  MockFetcherHandler fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnReceived(_, _)).Times(0);
  EXPECT_CALL(fetcher_handler_mock, OnDone()).Times(0);
  EXPECT_CALL(fetcher_handler_mock, data()).Times(0);
  EXPECT_CALL(fetcher_handler_mock, OnError(_));

  const FilePath file_path = data_dir_.Append(FILE_PATH_LITERAL("nonexistent"));
  FileFetcher::Options options;
  file_fetcher_ = make_scoped_ptr(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  run_loop.Run();
}

TEST_F(FileFetcherTest, EmptyFile) {
  InSequence dummy;

  base::RunLoop run_loop;

  MockFetcherHandler fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnReceived(_, _)).Times(0);
  EXPECT_CALL(fetcher_handler_mock, OnError(_)).Times(0);
  EXPECT_CALL(fetcher_handler_mock, OnDone());
  EXPECT_CALL(fetcher_handler_mock, data());

  const FilePath file_path = data_dir_.Append(FILE_PATH_LITERAL("empty.txt"));
  FileFetcher::Options options;
  file_fetcher_ = make_scoped_ptr(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  run_loop.Run();

  std::string loaded_text = fetcher_handler_mock.data();
  EXPECT_EQ("", loaded_text);
}

// Typical usage of FileFetcher.
TEST_F(FileFetcherTest, ValidFile) {
  InSequence dummy;

  // Create a RunLoop that controls the current message loop.
  base::RunLoop run_loop;
  MockFetcherHandler fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnError(_)).Times(0);
  EXPECT_CALL(fetcher_handler_mock, OnReceived(_, _)).Times(AtLeast(1));
  EXPECT_CALL(fetcher_handler_mock, OnDone());
  EXPECT_CALL(fetcher_handler_mock, data());

  // Create a File Fetcher.
  const FilePath file_path =
      data_dir_.Append(FILE_PATH_LITERAL("performance-spike.html"));
  FileFetcher::Options options;
  options.buffer_size = 128;
  file_fetcher_ = make_scoped_ptr(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  // Start the message loop, hence the fetching.
  run_loop.Run();

  // Get result.
  std::string loaded_text = fetcher_handler_mock.data();

  std::string expected_text;
  EXPECT_TRUE(file_util::ReadFileToString(dir_source_root_.Append(file_path),
                                          &expected_text));
  EXPECT_EQ(expected_text, loaded_text);
}

}  // namespace loader
}  // namespace cobalt
