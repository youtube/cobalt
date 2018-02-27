// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/loader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/loader/file_fetcher.h"
#include "cobalt/loader/text_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::_;

namespace cobalt {
namespace loader {
namespace {

//////////////////////////////////////////////////////////////////////////
// Callbacks
//////////////////////////////////////////////////////////////////////////

class TextDecoderCallback {
 public:
  explicit TextDecoderCallback(base::RunLoop* run_loop) : run_loop_(run_loop) {}

  void OnDone(const Origin&, scoped_ptr<std::string> text) {
    text_ = *text;
    MessageLoop::current()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

  std::string text() { return text_; }

 private:
  base::RunLoop* run_loop_;
  std::string text_;
};

class LoaderCallback {
 public:
  LoaderCallback() : run_loop_(NULL) {}
  explicit LoaderCallback(base::RunLoop* run_loop) : run_loop_(run_loop) {}

  void OnError(const std::string& text) {
    DLOG(ERROR) << text;
    if (run_loop_)
      MessageLoop::current()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

 private:
  base::RunLoop* run_loop_;
};

//////////////////////////////////////////////////////////////////////////
// Mocks & Stubs
//////////////////////////////////////////////////////////////////////////

class MockDecoder : public Decoder {
 public:
  MOCK_METHOD2(DecodeChunk, void(const char*, size_t));
  MOCK_METHOD0(Finish, void());
  MOCK_METHOD0(Suspend, bool());
  MOCK_METHOD1(Resume, void(render_tree::ResourceProvider*));
};

class MockFetcher : public Fetcher {
 public:
  explicit MockFetcher(Handler* handler) : Fetcher(handler) {}

  void FireError(const char* message) { handler()->OnError(this, message); }

  void FireReceived(const char* data, size_t size) {
    handler()->OnReceived(this, data, size);
  }

  void FireDone() { handler()->OnDone(this); }

  static scoped_ptr<Fetcher> Create(Handler* handler) {
    return scoped_ptr<Fetcher>(new MockFetcher(handler));
  }
};

struct MockFetcherFactory {
 public:
  MockFetcherFactory() : count(0) {}
  // Way to access the last fetcher created by the fetcher factory.
  MockFetcher* fetcher;
  int count;

  scoped_ptr<Fetcher> Create(Fetcher::Handler* handler) {
    fetcher = new MockFetcher(handler);
    ++count;
    return scoped_ptr<Fetcher>(fetcher);
  }

  Loader::FetcherCreator GetFetcherCreator() {
    return base::Bind(&MockFetcherFactory::Create, base::Unretained(this));
  }
};

class MockLoaderCallback : public LoaderCallback {
 public:
  MockLoaderCallback() {
    ON_CALL(*this, OnError(_))
        .WillByDefault(Invoke(&real_, &LoaderCallback::OnError));
  }

  MOCK_METHOD1(OnError, void(const std::string&));

 private:
  LoaderCallback real_;
};

}  // namespace

//////////////////////////////////////////////////////////////////////////
// LoaderTest
//////////////////////////////////////////////////////////////////////////

class LoaderTest : public ::testing::Test {
 protected:
  LoaderTest();
  ~LoaderTest() override {}

  FilePath data_dir_;
  MessageLoop message_loop_;
};

LoaderTest::LoaderTest() : message_loop_(MessageLoop::TYPE_DEFAULT) {
  data_dir_ = data_dir_.Append(FILE_PATH_LITERAL("cobalt"))
                  .Append(FILE_PATH_LITERAL("loader"))
                  .Append(FILE_PATH_LITERAL("testdata"));
}

TEST_F(LoaderTest, FetcherError) {
  MockLoaderCallback mock_loader_callback;
  MockFetcherFactory mock_fetcher_factory;
  MockDecoder* mock_decoder = new MockDecoder();  // To be owned by loader.
  EXPECT_CALL(*mock_decoder, DecodeChunk(_, _)).Times(0);
  EXPECT_CALL(*mock_decoder, Finish()).Times(0);
  EXPECT_CALL(mock_loader_callback, OnError(_));

  Loader loader(mock_fetcher_factory.GetFetcherCreator(),
                scoped_ptr<Decoder>(mock_decoder),
                base::Bind(&MockLoaderCallback::OnError,
                           base::Unretained(&mock_loader_callback)));
  mock_fetcher_factory.fetcher->FireError("Fail");
}

TEST_F(LoaderTest, FetcherSuspendAbort) {
  MockLoaderCallback mock_loader_callback;
  MockFetcherFactory mock_fetcher_factory;
  MockDecoder* mock_decoder = new MockDecoder();  // To be owned by loader.
  EXPECT_CALL(*mock_decoder, DecodeChunk(_, _)).Times(0);
  EXPECT_CALL(*mock_decoder, Finish()).Times(0);
  EXPECT_CALL(*mock_decoder, Suspend());
  EXPECT_CALL(*mock_decoder, Resume(_)).Times(0);
  EXPECT_CALL(mock_loader_callback, OnError(_));

  Loader loader(mock_fetcher_factory.GetFetcherCreator(),
                scoped_ptr<Decoder>(mock_decoder),
                base::Bind(&MockLoaderCallback::OnError,
                           base::Unretained(&mock_loader_callback)));
  loader.Suspend();
}

TEST_F(LoaderTest, FetcherSuspendResumeDone) {
  MockLoaderCallback mock_loader_callback;
  MockFetcherFactory mock_fetcher_factory;
  MockDecoder* mock_decoder = new MockDecoder();  // To be owned by loader.
  ON_CALL(*mock_decoder, Suspend()).WillByDefault(testing::Return(true));

  EXPECT_CALL(*mock_decoder, Suspend());
  EXPECT_CALL(*mock_decoder, Resume(_));

  EXPECT_CALL(*mock_decoder, DecodeChunk(_, _)).Times(0);
  EXPECT_CALL(mock_loader_callback, OnError(_)).Times(0);
  EXPECT_CALL(*mock_decoder, Finish());

  Loader loader(mock_fetcher_factory.GetFetcherCreator(),
                scoped_ptr<Decoder>(mock_decoder),
                base::Bind(&MockLoaderCallback::OnError,
                           base::Unretained(&mock_loader_callback)));
  loader.Suspend();
  loader.Resume(NULL);

  // The fetcher should have been torn down and recreated.
  EXPECT_EQ(2, mock_fetcher_factory.count);
  mock_fetcher_factory.fetcher->FireDone();
}

TEST_F(LoaderTest, FetcherReceiveDone) {
  InSequence dummy;

  MockLoaderCallback mock_loader_callback;
  MockFetcherFactory mock_fetcher_factory;
  MockDecoder* mock_decoder = new MockDecoder();  // To be owned by loader.
  EXPECT_CALL(mock_loader_callback, OnError(_)).Times(0);
  EXPECT_CALL(*mock_decoder, DecodeChunk(_, _));
  EXPECT_CALL(*mock_decoder, Finish());

  Loader loader(mock_fetcher_factory.GetFetcherCreator(),
                scoped_ptr<Decoder>(mock_decoder),
                base::Bind(&MockLoaderCallback::OnError,
                           base::Unretained(&mock_loader_callback)));
  mock_fetcher_factory.fetcher->FireReceived(NULL, 0);
  mock_fetcher_factory.fetcher->FireDone();
}

// Typical usage of Loader.
TEST_F(LoaderTest, ValidFileEndToEndTest) {
  // Create a RunLoop that helps us use the MessageLoop, which is in the test
  // fixture object.
  base::RunLoop run_loop;

  // Create a loader, using a FileFetcher that loads from disk, and a
  // TextDecoder that sees the received bytes as plain text.
  const FilePath file_path = data_dir_.Append("performance-spike.html");
  FileFetcher::Options fetcher_options;
  TextDecoderCallback text_decoder_callback(&run_loop);
  LoaderCallback loader_callback(&run_loop);
  Loader loader(
      base::Bind(&FileFetcher::Create, file_path, fetcher_options),
      scoped_ptr<Decoder>(new TextDecoder(
          base::Bind(&TextDecoderCallback::OnDone,
                     base::Unretained(&text_decoder_callback)))),
      base::Bind(&LoaderCallback::OnError, base::Unretained(&loader_callback)));

  // When the message loop runs, the loader will start loading. It'll quit when
  // loading is finished.
  run_loop.Run();

  // Get the loaded result from the decoder callback.
  std::string loaded_text = text_decoder_callback.text();

  // Compare the result with that of other API's.
  std::string expected_text;
  FilePath dir_test_data;
  EXPECT_TRUE(PathService::Get(base::DIR_TEST_DATA, &dir_test_data));
  EXPECT_TRUE(file_util::ReadFileToString(dir_test_data.Append(file_path),
                                          &expected_text));
  EXPECT_EQ(expected_text, loaded_text);
}

}  // namespace loader
}  // namespace cobalt
