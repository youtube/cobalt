// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "net/base/net_errors.h"
#if defined(STARBOARD)
#include "components/update_client/test_configurator.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#else
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

using base::ContentsEqual;

namespace update_client {

namespace {

#if defined(STARBOARD)
// Intercepts HTTP GET requests sent to "localhost".
typedef net::LocalHostTestURLRequestInterceptor GetInterceptor;
#endif

const char kTestFileName[] = "jebgalgnebhfojomionfpkfelancnnkf.crx";

const char hash_jebg[] =
    "6fc4b93fd11134de1300c2c0bb88c12b644a4ec0fd7c9b12cb7cc067667bde87";

base::FilePath MakeTestFilePath(const char* file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components/test/data/update_client")
      .AppendASCII(file);
}

}  // namespace

class CrxDownloaderTest : public testing::Test {
 public:
  CrxDownloaderTest();
  ~CrxDownloaderTest() override;

  // Overrides from testing::Test.
  void SetUp() override;
  void TearDown() override;

  void Quit();
  void RunThreads();
  void RunThreadsUntilIdle();

  void DownloadComplete(int crx_context, const CrxDownloader::Result& result);

  void DownloadProgress(int crx_context);

#if !defined(STARBOARD)
  int GetInterceptorCount() { return interceptor_count_; }

  void AddResponse(const GURL& url,
                   const base::FilePath& file_path,
                   int net_error);
#endif
 protected:
  std::unique_ptr<CrxDownloader> crx_downloader_;
#if defined(STARBOARD)
  std::unique_ptr<GetInterceptor> get_interceptor_;
#else
  network::TestURLLoaderFactory test_url_loader_factory_;
#endif
  CrxDownloader::DownloadCallback callback_;
  CrxDownloader::ProgressCallback progress_callback_;

  int crx_context_;

  int num_download_complete_calls_;
  CrxDownloader::Result download_complete_result_;

  // These members are updated by DownloadProgress.
  int num_progress_calls_;

#if !defined(STARBOARD)
  // Accumulates the number of loads triggered.
  int interceptor_count_ = 0;
#endif

  // A magic value for the context to be used in the tests.
  static const int kExpectedContext = 0xaabb;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
#if defined(STARBOARD)
  scoped_refptr<net::TestURLRequestContextGetter> context_;
#else
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
#endif
  base::OnceClosure quit_closure_;
};

const int CrxDownloaderTest::kExpectedContext;

CrxDownloaderTest::CrxDownloaderTest()
    : callback_(base::BindOnce(&CrxDownloaderTest::DownloadComplete,
                               base::Unretained(this),
                               kExpectedContext)),
      progress_callback_(base::Bind(&CrxDownloaderTest::DownloadProgress,
                                    base::Unretained(this),
                                    kExpectedContext)),
      crx_context_(0),
      num_download_complete_calls_(0),
      num_progress_calls_(0),
      scoped_task_environment_(
          base::test::ScopedTaskEnvironment::MainThreadType::IO),
#if defined(STARBOARD)
      context_(base::MakeRefCounted<net::TestURLRequestContextGetter>(
          base::ThreadTaskRunnerHandle::Get())) {
}

CrxDownloaderTest::~CrxDownloaderTest() {
  context_ = nullptr;

  // The GetInterceptor requires the message loop to run to destruct correctly.
  get_interceptor_.reset();
  RunThreadsUntilIdle();
}
#else
      test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {
}

CrxDownloaderTest::~CrxDownloaderTest() {}
#endif
void CrxDownloaderTest::SetUp() {
  num_download_complete_calls_ = 0;
  download_complete_result_ = CrxDownloader::Result();
  num_progress_calls_ = 0;

// Do not use the background downloader in these tests.
#if defined(STARBOARD)
  auto config = base::MakeRefCounted<TestConfigurator>();
  crx_downloader_ = CrxDownloader::Create(false, config);
  crx_downloader_->set_progress_callback(progress_callback_);

  get_interceptor_ = std::make_unique<GetInterceptor>(
      base::ThreadTaskRunnerHandle::Get(), base::ThreadTaskRunnerHandle::Get());
#else
  crx_downloader_ = CrxDownloader::Create(
      false, base::MakeRefCounted<NetworkFetcherChromiumFactory>(
                 test_shared_url_loader_factory_));
  crx_downloader_->set_progress_callback(progress_callback_);

  test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
      [&](const network::ResourceRequest& request) { interceptor_count_++; }));
#endif
}

void CrxDownloaderTest::TearDown() {
  crx_downloader_.reset();
}

void CrxDownloaderTest::Quit() {
  if (!quit_closure_.is_null())
    std::move(quit_closure_).Run();
}

void CrxDownloaderTest::DownloadComplete(int crx_context,
                                         const CrxDownloader::Result& result) {
  ++num_download_complete_calls_;
  crx_context_ = crx_context;
  download_complete_result_ = result;
  Quit();
}

void CrxDownloaderTest::DownloadProgress(int crx_context) {
  ++num_progress_calls_;
}
#if !defined(STARBOARD)
void CrxDownloaderTest::AddResponse(const GURL& url,
                                    const base::FilePath& file_path,
                                    int net_error) {
  if (net_error == net::OK) {
    std::string data;
    EXPECT_TRUE(base::ReadFileToString(file_path, &data));
    network::ResourceResponseHead head;
    head.content_length = data.size();
    network::URLLoaderCompletionStatus status(net_error);
    status.decoded_body_length = data.size();
    test_url_loader_factory_.AddResponse(url, head, data, status);
    return;
  }

  EXPECT_NE(net_error, net::OK);
  test_url_loader_factory_.AddResponse(
      url, network::ResourceResponseHead(), std::string(),
      network::URLLoaderCompletionStatus(net_error));
}
#endif
void CrxDownloaderTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();

  // Since some tests need to drain currently enqueued tasks such as network
  // intercepts on the IO thread, run the threads until they are
  // idle. The component updater service won't loop again until the loop count
  // is set and the service is started.
  RunThreadsUntilIdle();
}

void CrxDownloaderTest::RunThreadsUntilIdle() {
  scoped_task_environment_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();
}

#if !defined(IN_MEMORY_UPDATES)
// Tests that starting a download without a url results in an error.
TEST_F(CrxDownloaderTest, NoUrl) {
  std::vector<GURL> urls;
  crx_downloader_->StartDownload(urls, std::string("abcd"),
                                 std::move(callback_));
  RunThreadsUntilIdle();

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(static_cast<int>(CrxDownloaderError::NO_URL),
            download_complete_result_.error);
  EXPECT_TRUE(download_complete_result_.response.empty());
  EXPECT_EQ(0, num_progress_calls_);
}

// Tests that starting a download without providing a hash results in an error.
TEST_F(CrxDownloaderTest, NoHash) {
  std::vector<GURL> urls(1, GURL("http://somehost/somefile"));

  crx_downloader_->StartDownload(urls, std::string(), std::move(callback_));
  RunThreadsUntilIdle();

  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(static_cast<int>(CrxDownloaderError::NO_HASH),
            download_complete_result_.error);
  EXPECT_TRUE(download_complete_result_.response.empty());
  EXPECT_EQ(0, num_progress_calls_);
}

// Tests that downloading from one url is successful.
TEST_F(CrxDownloaderTest, OneUrl) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
#if defined(STARBOARD)
  get_interceptor_->SetResponse(expected_crx_url, test_file);
#else
  AddResponse(expected_crx_url, test_file, net::OK);
#endif
  crx_downloader_->StartDownloadFromUrl(
      expected_crx_url, std::string(hash_jebg), std::move(callback_));
  RunThreads();
#if defined(STARBOARD)
  EXPECT_EQ(1, get_interceptor_->GetHitCount());
#else
  EXPECT_EQ(1, GetInterceptorCount());
#endif
  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  EXPECT_LE(1, num_progress_calls_);
}

// Tests that downloading from one url fails if the actual hash of the file
// does not match the expected hash.
TEST_F(CrxDownloaderTest, OneUrlBadHash) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
#if defined(STARBOARD)
  get_interceptor_->SetResponse(expected_crx_url, test_file);
#else
  AddResponse(expected_crx_url, test_file, net::OK);
#endif
  crx_downloader_->StartDownloadFromUrl(
      expected_crx_url,
      std::string(
          "813c59747e139a608b3b5fc49633affc6db574373f309f156ea6d27229c0b3f9"),
      std::move(callback_));
  RunThreads();
#if defined(STARBOARD)
  EXPECT_EQ(1, get_interceptor_->GetHitCount());
#else
  EXPECT_EQ(1, GetInterceptorCount());
#endif
  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(static_cast<int>(CrxDownloaderError::BAD_HASH),
            download_complete_result_.error);
  EXPECT_TRUE(download_complete_result_.response.empty());

  EXPECT_LE(1, num_progress_calls_);
}

// Tests that specifying two urls has no side effects. Expect a successful
// download, and only one download request be made.
TEST_F(CrxDownloaderTest, TwoUrls) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
#if defined(STARBOARD)
  get_interceptor_->SetResponse(expected_crx_url, test_file);
#else
  AddResponse(expected_crx_url, test_file, net::OK);
#endif
  std::vector<GURL> urls;
  urls.push_back(expected_crx_url);
  urls.push_back(expected_crx_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg),
                                 std::move(callback_));
  RunThreads();

#if defined(STARBOARD)
  EXPECT_EQ(1, get_interceptor_->GetHitCount());
#else
  EXPECT_EQ(1, GetInterceptorCount());
#endif
  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  EXPECT_LE(1, num_progress_calls_);
}

// Tests that the fallback to a valid url is successful.
TEST_F(CrxDownloaderTest, TwoUrls_FirstInvalid) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");
  const GURL no_file_url =
      GURL("http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
#if defined(STARBOARD)
  get_interceptor_->SetResponse(expected_crx_url, test_file);
  get_interceptor_->SetResponse(no_file_url,
                                base::FilePath(FILE_PATH_LITERAL("no-file")));
#else
  AddResponse(expected_crx_url, test_file, net::OK);
  AddResponse(no_file_url, base::FilePath(), net::ERR_FILE_NOT_FOUND);
#endif
  std::vector<GURL> urls;
  urls.push_back(no_file_url);
  urls.push_back(expected_crx_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg),
                                 std::move(callback_));
  RunThreads();

#if defined(STARBOARD)
  EXPECT_EQ(2, get_interceptor_->GetHitCount());
#else
  EXPECT_EQ(2, GetInterceptorCount());
#endif
  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  // Expect at least some progress reported by the loader.
  EXPECT_LE(1, num_progress_calls_);

  const auto download_metrics = crx_downloader_->download_metrics();
  ASSERT_EQ(2u, download_metrics.size());
  EXPECT_EQ(no_file_url, download_metrics[0].url);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[0].error);
  EXPECT_EQ(-1, download_metrics[0].downloaded_bytes);
  EXPECT_EQ(-1, download_metrics[0].total_bytes);
  EXPECT_EQ(expected_crx_url, download_metrics[1].url);
  EXPECT_EQ(0, download_metrics[1].error);
  EXPECT_EQ(1843, download_metrics[1].downloaded_bytes);
  EXPECT_EQ(1843, download_metrics[1].total_bytes);
}

// Tests that the download succeeds if the first url is correct and the
// second bad url does not have a side-effect.
TEST_F(CrxDownloaderTest, TwoUrls_SecondInvalid) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");
  const GURL no_file_url =
      GURL("http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc.crx");

  const base::FilePath test_file(MakeTestFilePath(kTestFileName));
#if defined(STARBOARD)
  get_interceptor_->SetResponse(expected_crx_url, test_file);
  get_interceptor_->SetResponse(no_file_url,
                                base::FilePath(FILE_PATH_LITERAL("no-file")));
#else
  AddResponse(expected_crx_url, test_file, net::OK);
  AddResponse(no_file_url, base::FilePath(), net::ERR_FILE_NOT_FOUND);
#endif

  std::vector<GURL> urls;
  urls.push_back(expected_crx_url);
  urls.push_back(no_file_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg),
                                 std::move(callback_));
  RunThreads();

#if defined(STARBOARD)
  EXPECT_EQ(1, get_interceptor_->GetHitCount());
#else
  EXPECT_EQ(1, GetInterceptorCount());
#endif
  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_EQ(0, download_complete_result_.error);
  EXPECT_TRUE(ContentsEqual(download_complete_result_.response, test_file));

  EXPECT_TRUE(
      DeleteFileAndEmptyParentDirectory(download_complete_result_.response));

  EXPECT_LE(1, num_progress_calls_);

  EXPECT_EQ(1u, crx_downloader_->download_metrics().size());
}

// Tests that the download fails if both urls don't serve content.
TEST_F(CrxDownloaderTest, TwoUrls_BothInvalid) {
  const GURL expected_crx_url =
      GURL("http://localhost/download/jebgalgnebhfojomionfpkfelancnnkf.crx");

#if defined(STARBOARD)
  get_interceptor_->SetResponse(expected_crx_url,
                                base::FilePath(FILE_PATH_LITERAL("no-file")));
#else
  AddResponse(expected_crx_url, base::FilePath(), net::ERR_FILE_NOT_FOUND);
#endif

  std::vector<GURL> urls;
  urls.push_back(expected_crx_url);
  urls.push_back(expected_crx_url);

  crx_downloader_->StartDownload(urls, std::string(hash_jebg),
                                 std::move(callback_));
  RunThreads();

#if defined(STARBOARD)
  EXPECT_EQ(2, get_interceptor_->GetHitCount());
#else
  EXPECT_EQ(2, GetInterceptorCount());
#endif
  EXPECT_EQ(1, num_download_complete_calls_);
  EXPECT_EQ(kExpectedContext, crx_context_);
  EXPECT_NE(0, download_complete_result_.error);
  EXPECT_TRUE(download_complete_result_.response.empty());

  const auto download_metrics = crx_downloader_->download_metrics();
  ASSERT_EQ(2u, download_metrics.size());
  EXPECT_EQ(expected_crx_url, download_metrics[0].url);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[0].error);
  EXPECT_EQ(-1, download_metrics[0].downloaded_bytes);
  EXPECT_EQ(-1, download_metrics[0].total_bytes);
  EXPECT_EQ(expected_crx_url, download_metrics[1].url);
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, download_metrics[1].error);
  EXPECT_EQ(-1, download_metrics[1].downloaded_bytes);
  EXPECT_EQ(-1, download_metrics[1].total_bytes);
}
// TODO(b/290410288): write tests targeting the overload of StartDownload() that
// downloads to a string.
#endif

}  // namespace update_client
