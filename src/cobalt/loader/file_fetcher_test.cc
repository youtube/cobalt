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

#include <string>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::_;

namespace cobalt {
namespace loader {
namespace {

class FetcherHandlerForTest : public Fetcher::Handler {
 public:
  explicit FetcherHandlerForTest(base::RunLoop* run_loop)
      : fetcher_(NULL), run_loop_(run_loop) {}

  // From Fetcher::Handler.
  void OnReceived(Fetcher* fetcher, const char* data, size_t size) OVERRIDE {
    CheckFetcher(fetcher);
    data_.append(data, size);
  }
  void OnDone(Fetcher* fetcher) OVERRIDE {
    CheckFetcher(fetcher);
    MessageLoop::current()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }
  void OnError(Fetcher* fetcher, const std::string& error) OVERRIDE {
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
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root_));
}

TEST_F(FileFetcherTest, NonExisitingPath) {
  base::RunLoop run_loop;

  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnError(_, _));

  const FilePath file_path = data_dir_.Append(FILE_PATH_LITERAL("nonexistent"));
  FileFetcher::Options options;
  file_fetcher_ = make_scoped_ptr(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  run_loop.Run();

  EXPECT_EQ(file_fetcher_.get(), fetcher_handler_mock.fetcher());
}

TEST_F(FileFetcherTest, EmptyFile) {
  InSequence dummy;

  base::RunLoop run_loop;

  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnDone(_));

  const FilePath file_path = data_dir_.Append(FILE_PATH_LITERAL("empty.txt"));
  FileFetcher::Options options;
  file_fetcher_ = make_scoped_ptr(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  run_loop.Run();

  std::string loaded_text = fetcher_handler_mock.data();
  EXPECT_EQ("", loaded_text);

  EXPECT_EQ(file_fetcher_.get(), fetcher_handler_mock.fetcher());
}

// Typical usage of FileFetcher.
TEST_F(FileFetcherTest, ValidFile) {
  InSequence dummy;

  // Create a RunLoop that controls the current message loop.
  base::RunLoop run_loop;
  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnReceived(_, _, _)).Times(AtLeast(1));
  EXPECT_CALL(fetcher_handler_mock, OnDone(_));

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

  EXPECT_EQ(file_fetcher_.get(), fetcher_handler_mock.fetcher());
}

// Use FileFetcher with an offset.
TEST_F(FileFetcherTest, ReadWithOffset) {
  const uint32 kStartOffset = 15;
  InSequence dummy;

  // Create a RunLoop that controls the current message loop.
  base::RunLoop run_loop;
  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnReceived(_, _, _)).Times(AtLeast(1));
  EXPECT_CALL(fetcher_handler_mock, OnDone(_));

  // Create a File Fetcher.
  const FilePath file_path =
      data_dir_.Append(FILE_PATH_LITERAL("performance-spike.html"));
  FileFetcher::Options options;
  options.buffer_size = 128;
  options.start_offset = kStartOffset;
  file_fetcher_ = make_scoped_ptr(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  // Start the message loop, hence the fetching.
  run_loop.Run();

  // Get result.
  std::string loaded_text = fetcher_handler_mock.data();

  std::string expected_text;
  EXPECT_TRUE(file_util::ReadFileToString(dir_source_root_.Append(file_path),
                                          &expected_text));
  expected_text = expected_text.substr(kStartOffset);
  EXPECT_EQ(expected_text, loaded_text);

  EXPECT_EQ(file_fetcher_.get(), fetcher_handler_mock.fetcher());
}

// Use FileFetcher with an offset and explicit bytes to read.
TEST_F(FileFetcherTest, ReadWithOffsetAndSize) {
  const uint32 kStartOffset = 15;
  const uint32 kBytesToRead = 147;
  InSequence dummy;

  // Create a RunLoop that controls the current message loop.
  base::RunLoop run_loop;
  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnReceived(_, _, _)).Times(2);
  EXPECT_CALL(fetcher_handler_mock, OnDone(_));

  // Create a File Fetcher.
  const FilePath file_path =
      data_dir_.Append(FILE_PATH_LITERAL("performance-spike.html"));
  FileFetcher::Options options;
  options.buffer_size = 128;
  options.start_offset = kStartOffset;
  options.bytes_to_read = kBytesToRead;
  file_fetcher_ = make_scoped_ptr(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  // Start the message loop, hence the fetching.
  run_loop.Run();

  // Get result.
  std::string loaded_text = fetcher_handler_mock.data();

  std::string expected_text;
  EXPECT_TRUE(file_util::ReadFileToString(dir_source_root_.Append(file_path),
                                          &expected_text));
  expected_text = expected_text.substr(kStartOffset, kBytesToRead);
  EXPECT_EQ(expected_text, loaded_text);

  EXPECT_EQ(file_fetcher_.get(), fetcher_handler_mock.fetcher());
}

}  // namespace loader
}  // namespace cobalt
