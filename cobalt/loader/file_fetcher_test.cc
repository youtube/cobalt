// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/file_fetcher.h"

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "cobalt/loader/fetcher_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace cobalt {
namespace loader {

class FileFetcherTest : public ::testing::Test {
 protected:
  FileFetcherTest();
  ~FileFetcherTest() override {}

  base::FilePath data_dir_;
  base::FilePath dir_test_data_;
  base::MessageLoop message_loop_;
  std::unique_ptr<FileFetcher> file_fetcher_;
};

FileFetcherTest::FileFetcherTest()
    : message_loop_(base::MessageLoop::TYPE_DEFAULT) {
  data_dir_ = data_dir_.Append(FILE_PATH_LITERAL("cobalt"))
                  .Append(FILE_PATH_LITERAL("loader"))
                  .Append(FILE_PATH_LITERAL("testdata"));
  CHECK(base::PathService::Get(base::DIR_TEST_DATA, &dir_test_data_));
}

TEST_F(FileFetcherTest, NonExistingPath) {
  base::RunLoop run_loop;

  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnError(_, _));

  const base::FilePath file_path =
      data_dir_.Append(FILE_PATH_LITERAL("nonexistent"));
  FileFetcher::Options options;
  file_fetcher_ = base::WrapUnique(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  run_loop.Run();

  EXPECT_EQ(file_fetcher_.get(), fetcher_handler_mock.fetcher());
}

TEST_F(FileFetcherTest, EmptyFile) {
  InSequence dummy;

  base::RunLoop run_loop;

  StrictMock<MockFetcherHandler> fetcher_handler_mock(&run_loop);
  EXPECT_CALL(fetcher_handler_mock, OnDone(_));

  const base::FilePath file_path =
      data_dir_.Append(FILE_PATH_LITERAL("empty.txt"));
  FileFetcher::Options options;
  file_fetcher_ = base::WrapUnique(
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
  const base::FilePath file_path =
      data_dir_.Append(FILE_PATH_LITERAL("performance-spike.html"));
  FileFetcher::Options options;
  options.buffer_size = 128;
  file_fetcher_ = base::WrapUnique(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  // Start the message loop, hence the fetching.
  run_loop.Run();

  // Get result.
  std::string loaded_text = fetcher_handler_mock.data();

  std::string expected_text;
  EXPECT_TRUE(
      base::ReadFileToString(dir_test_data_.Append(file_path), &expected_text));
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
  const base::FilePath file_path =
      data_dir_.Append(FILE_PATH_LITERAL("performance-spike.html"));
  FileFetcher::Options options;
  options.buffer_size = 128;
  options.start_offset = kStartOffset;
  file_fetcher_ = base::WrapUnique(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  // Start the message loop, hence the fetching.
  run_loop.Run();

  // Get result.
  std::string loaded_text = fetcher_handler_mock.data();

  std::string expected_text;
  EXPECT_TRUE(
      base::ReadFileToString(dir_test_data_.Append(file_path), &expected_text));
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
  const base::FilePath file_path =
      data_dir_.Append(FILE_PATH_LITERAL("performance-spike.html"));
  FileFetcher::Options options;
  options.buffer_size = 128;
  options.start_offset = kStartOffset;
  options.bytes_to_read = kBytesToRead;
  file_fetcher_ = base::WrapUnique(
      new FileFetcher(file_path, &fetcher_handler_mock, options));

  // Start the message loop, hence the fetching.
  run_loop.Run();

  // Get result.
  std::string loaded_text = fetcher_handler_mock.data();

  std::string expected_text;
  EXPECT_TRUE(
      base::ReadFileToString(dir_test_data_.Append(file_path), &expected_text));
  expected_text = expected_text.substr(kStartOffset, kBytesToRead);
  EXPECT_EQ(expected_text, loaded_text);

  EXPECT_EQ(file_fetcher_.get(), fetcher_handler_mock.fetcher());
}

}  // namespace loader
}  // namespace cobalt
