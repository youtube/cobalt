// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_impl.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_response_headers.h"
#include "net/test/test_server.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS) || defined(OS_IOS)
#include "net/ocsp/nss_ocsp.h"
#endif

namespace net {

using base::Time;
using base::TimeDelta;

// TODO(eroman): Add a regression test for http://crbug.com/40505.

namespace {

// TODO(akalin): Move all the test data to somewhere under net/.
const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");
const char kTestServerFilePrefix[] = "files/";

class ThrottlingTestURLRequestContext : public TestURLRequestContext {
 public:
  ThrottlingTestURLRequestContext() : TestURLRequestContext(true) {
    set_throttler_manager(&throttler_manager_);
    Init();
    DCHECK(throttler_manager() != NULL);
  }

 private:
  URLRequestThrottlerManager throttler_manager_;
};

class ThrottlingTestURLRequestContextGetter
    : public TestURLRequestContextGetter {
 public:
  ThrottlingTestURLRequestContextGetter(
      base::MessageLoopProxy* io_message_loop_proxy,
      TestURLRequestContext* request_context)
      : TestURLRequestContextGetter(io_message_loop_proxy),
        context_(request_context) {
  }

  // TestURLRequestContextGetter:
  virtual TestURLRequestContext* GetURLRequestContext() override {
    return context_;
  }

 protected:
  virtual ~ThrottlingTestURLRequestContextGetter() {}

  TestURLRequestContext* const context_;
};

}  // namespace

class URLFetcherTest : public testing::Test,
                       public URLFetcherDelegate {
 public:
  URLFetcherTest()
      : fetcher_(NULL),
        context_(NULL) {
  }

  static int GetNumFetcherCores() {
    return URLFetcherImpl::GetNumFetcherCores();
  }

  // Creates a URLFetcher, using the program's main thread to do IO.
  virtual void CreateFetcher(const GURL& url);

  // URLFetcherDelegate:
  // Subclasses that override this should either call this function or
  // CleanupAfterFetchComplete() at the end of their processing, depending on
  // whether they want to check for a non-empty HTTP 200 response or not.
  virtual void OnURLFetchComplete(const URLFetcher* source) override;

  // Deletes |fetcher| and terminates the message loop.
  void CleanupAfterFetchComplete();

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy() {
    return io_message_loop_proxy_;
  }

  TestURLRequestContext* request_context() {
    return context_.get();
  }

 protected:
  // testing::Test:
  virtual void SetUp() override {
    testing::Test::SetUp();

    context_.reset(new ThrottlingTestURLRequestContext());
    io_message_loop_proxy_ = base::MessageLoopProxy::current();

#if defined(USE_NSS) || defined(OS_IOS)
    crypto::EnsureNSSInit();
    EnsureNSSHttpIOInit();
#endif
  }

  virtual void TearDown() override {
#if defined(USE_NSS) || defined(OS_IOS)
    ShutdownNSSHttpIO();
#endif
  }

  // URLFetcher is designed to run on the main UI thread, but in our tests
  // we assume that the current thread is the IO thread where the URLFetcher
  // dispatches its requests to.  When we wish to simulate being used from
  // a UI thread, we dispatch a worker thread to do so.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  URLFetcherImpl* fetcher_;
  scoped_ptr<TestURLRequestContext> context_;
};

// A test fixture that uses a MockHostResolver, so that name resolutions can
// be manipulated by the tests to keep connections in the resolving state.
class URLFetcherMockDnsTest : public URLFetcherTest {
 public:
  // testing::Test:
  virtual void SetUp() override;

  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;

 protected:
  GURL test_url_;
  scoped_ptr<TestServer> test_server_;
  MockHostResolver resolver_;
  scoped_ptr<URLFetcher> completed_fetcher_;
};

void URLFetcherTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));
  fetcher_->Start();
}

void URLFetcherTest::OnURLFetchComplete(const URLFetcher* source) {
  EXPECT_TRUE(source->GetStatus().is_success());
  EXPECT_EQ(200, source->GetResponseCode());  // HTTP OK

  std::string data;
  EXPECT_TRUE(source->GetResponseAsString(&data));
  EXPECT_FALSE(data.empty());

  CleanupAfterFetchComplete();
}

void URLFetcherTest::CleanupAfterFetchComplete() {
  delete fetcher_;  // Have to delete this here and not in the destructor,
                    // because the destructor won't necessarily run on the
                    // same thread that CreateFetcher() did.

  io_message_loop_proxy()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  // If the current message loop is not the IO loop, it will be shut down when
  // the main loop returns and this thread subsequently goes out of scope.
}

void URLFetcherMockDnsTest::SetUp() {
  URLFetcherTest::SetUp();

  resolver_.set_ondemand_mode(true);
  resolver_.rules()->AddRule("example.com", "127.0.0.1");

  context_.reset(new TestURLRequestContext(true));
  context_->set_host_resolver(&resolver_);
  context_->Init();

  test_server_.reset(new TestServer(TestServer::TYPE_HTTP,
                                    TestServer::kLocalhost,
                                    FilePath(kDocRoot)));
  ASSERT_TRUE(test_server_->Start());

  // test_server_.GetURL() returns a URL with 127.0.0.1 (kLocalhost), that is
  // immediately resolved by the MockHostResolver. Use a hostname instead to
  // trigger an async resolve.
  test_url_ = GURL(
      base::StringPrintf("http://example.com:%d/defaultresponse",
      test_server_->host_port_pair().port()));
  ASSERT_TRUE(test_url_.is_valid());
}

void URLFetcherMockDnsTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));
}

void URLFetcherMockDnsTest::OnURLFetchComplete(const URLFetcher* source) {
  io_message_loop_proxy()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  ASSERT_EQ(fetcher_, source);
  EXPECT_EQ(test_url_, source->GetOriginalURL());
  completed_fetcher_.reset(fetcher_);
}

namespace {

// Version of URLFetcherTest that does a POST instead
class URLFetcherPostTest : public URLFetcherTest {
 public:
  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;
};

// Version of URLFetcherTest that does a POST instead with empty upload body
class URLFetcherEmptyPostTest : public URLFetcherTest {
 public:
  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;
};

// Version of URLFetcherTest that tests download progress reports.
class URLFetcherDownloadProgressTest : public URLFetcherTest {
 public:
  URLFetcherDownloadProgressTest()
      : previous_progress_(0),
        expected_total_(0) {
  }

  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchDownloadProgress(const URLFetcher* source,
                                          int64 current,
                                          int64 total) override;

 protected:
  // Download progress returned by the previous callback.
  int64 previous_progress_;
  // Size of the file being downloaded, known in advance (provided by each test
  // case).
  int64 expected_total_;
};

// Version of URLFetcherTest that tests progress reports at cancellation.
class URLFetcherDownloadProgressCancelTest : public URLFetcherTest {
 public:
  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;
  virtual void OnURLFetchDownloadProgress(const URLFetcher* source,
                                          int64 current,
                                          int64 total) override;
 protected:
  bool cancelled_;
};

// Version of URLFetcherTest that tests upload progress reports.
class URLFetcherUploadProgressTest : public URLFetcherTest {
 public:
  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchUploadProgress(const URLFetcher* source,
                                        int64 current,
                                        int64 total) override;
 protected:
  int64 previous_progress_;
  std::string chunk_;
  int64 number_of_chunks_added_;
};

// Version of URLFetcherTest that tests headers.
class URLFetcherHeadersTest : public URLFetcherTest {
 public:
  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;
};

// Version of URLFetcherTest that tests SocketAddress.
class URLFetcherSocketAddressTest : public URLFetcherTest {
 public:
  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;
 protected:
  std::string expected_host_;
  uint16 expected_port_;
};

// Version of URLFetcherTest that tests stopping on a redirect.
class URLFetcherStopOnRedirectTest : public URLFetcherTest {
 public:
  URLFetcherStopOnRedirectTest();
  virtual ~URLFetcherStopOnRedirectTest();

  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;

 protected:
  // The URL we should be redirected to.
  static const char* kRedirectTarget;

  bool callback_called_;  // Set to true in OnURLFetchComplete().
};

// Version of URLFetcherTest that tests overload protection.
class URLFetcherProtectTest : public URLFetcherTest {
 public:
  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;
 private:
  Time start_time_;
};

// Version of URLFetcherTest that tests overload protection, when responses
// passed through.
class URLFetcherProtectTestPassedThrough : public URLFetcherTest {
 public:
  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;
 private:
  Time start_time_;
};

// Version of URLFetcherTest that tests bad HTTPS requests.
class URLFetcherBadHTTPSTest : public URLFetcherTest {
 public:
  URLFetcherBadHTTPSTest();

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;

 private:
  FilePath cert_dir_;
};

// Version of URLFetcherTest that tests request cancellation on shutdown.
class URLFetcherCancelTest : public URLFetcherTest {
 public:
  // URLFetcherTest:
  virtual void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;

  void CancelRequest();
};

// Version of TestURLRequestContext that posts a Quit task to the IO
// thread once it is deleted.
class CancelTestURLRequestContext : public ThrottlingTestURLRequestContext {
 public:
  explicit CancelTestURLRequestContext() {
  }

 private:
  virtual ~CancelTestURLRequestContext() {
    // The d'tor should execute in the IO thread. Post the quit task to the
    // current thread.
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }
};

class CancelTestURLRequestContextGetter
    : public TestURLRequestContextGetter {
 public:
  CancelTestURLRequestContextGetter(
      base::MessageLoopProxy* io_message_loop_proxy,
      const GURL& throttle_for_url)
      : TestURLRequestContextGetter(io_message_loop_proxy),
        io_message_loop_proxy_(io_message_loop_proxy),
        context_created_(false, false),
        throttle_for_url_(throttle_for_url) {
  }

  // TestURLRequestContextGetter:
  virtual TestURLRequestContext* GetURLRequestContext() override {
    if (!context_.get()) {
      context_.reset(new CancelTestURLRequestContext());
      DCHECK(context_->throttler_manager());

      // Registers an entry for test url. The backoff time is calculated by:
      //     new_backoff = 2.0 * old_backoff + 0
      // The initial backoff is 2 seconds and maximum backoff is 4 seconds.
      // Maximum retries allowed is set to 2.
      scoped_refptr<URLRequestThrottlerEntry> entry(
          new URLRequestThrottlerEntry(
              context_->throttler_manager(),
              "", 200, 3, 2000, 2.0, 0.0, 4000));
      context_->throttler_manager()->OverrideEntryForTests(
          throttle_for_url_, entry);

      context_created_.Signal();
    }
    return context_.get();
  }

  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return io_message_loop_proxy_;
  }

  void WaitForContextCreation() {
    context_created_.Wait();
  }

 protected:
  virtual ~CancelTestURLRequestContextGetter() {}

 private:
  scoped_ptr<TestURLRequestContext> context_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  base::WaitableEvent context_created_;
  GURL throttle_for_url_;
};

// Version of URLFetcherTest that tests retying the same request twice.
class URLFetcherMultipleAttemptTest : public URLFetcherTest {
 public:
  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;
 private:
  std::string data_;
};

class URLFetcherFileTest : public URLFetcherTest {
 public:
  URLFetcherFileTest() : take_ownership_of_file_(false),
                         expected_file_error_(base::PLATFORM_FILE_OK) {}

  void CreateFetcherForFile(const GURL& url, const FilePath& file_path);
  void CreateFetcherForTempFile(const GURL& url);

  // URLFetcherDelegate:
  virtual void OnURLFetchComplete(const URLFetcher* source) override;

 protected:
  FilePath expected_file_;
  FilePath file_path_;

  // Set by the test. Used in OnURLFetchComplete() to decide if
  // the URLFetcher should own the temp file, so that we can test
  // disowning prevents the file from being deleted.
  bool take_ownership_of_file_;

  // Expected file error code for the test.
  // PLATFORM_FILE_OK when expecting success.
  base::PlatformFileError expected_file_error_;
};

void URLFetcherPostTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::POST, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));
  fetcher_->SetUploadData("application/x-www-form-urlencoded",
                          "bobsyeruncle");
  fetcher_->Start();
}

void URLFetcherPostTest::OnURLFetchComplete(const URLFetcher* source) {
  std::string data;
  EXPECT_TRUE(source->GetResponseAsString(&data));
  EXPECT_EQ(std::string("bobsyeruncle"), data);
  URLFetcherTest::OnURLFetchComplete(source);
}

void URLFetcherEmptyPostTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::POST, this);
  fetcher_->SetRequestContext(new TestURLRequestContextGetter(
      io_message_loop_proxy()));
  fetcher_->SetUploadData("text/plain", "");
  fetcher_->Start();
}

void URLFetcherEmptyPostTest::OnURLFetchComplete(const URLFetcher* source) {
  EXPECT_TRUE(source->GetStatus().is_success());
  EXPECT_EQ(200, source->GetResponseCode());  // HTTP OK

  std::string data;
  EXPECT_TRUE(source->GetResponseAsString(&data));
  EXPECT_TRUE(data.empty());

  CleanupAfterFetchComplete();
  // Do not call the super class method URLFetcherTest::OnURLFetchComplete,
  // since it expects a non-empty response.
}

void URLFetcherDownloadProgressTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));
  fetcher_->Start();
}

void URLFetcherDownloadProgressTest::OnURLFetchDownloadProgress(
    const URLFetcher* source, int64 progress, int64 total) {
  // Increasing between 0 and total.
  EXPECT_LE(0, progress);
  EXPECT_GE(total, progress);
  EXPECT_LE(previous_progress_, progress);
  EXPECT_EQ(expected_total_, total);
  previous_progress_ = progress;
}

void URLFetcherDownloadProgressCancelTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));
  cancelled_ = false;
  fetcher_->Start();
}

void URLFetcherDownloadProgressCancelTest::OnURLFetchDownloadProgress(
    const URLFetcher* source, int64 current, int64 total) {
  EXPECT_FALSE(cancelled_);
  if (!cancelled_) {
    cancelled_ = true;
    CleanupAfterFetchComplete();
  }
}

void URLFetcherDownloadProgressCancelTest::OnURLFetchComplete(
    const URLFetcher* source) {
  // Should have been cancelled.
  ADD_FAILURE();
  CleanupAfterFetchComplete();
}

void URLFetcherUploadProgressTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::POST, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));
  previous_progress_ = 0;
  // Large enough data to require more than one read from UploadDataStream.
  chunk_.assign(1<<16, 'a');
  // Use chunked upload to wait for a timer event of progress notification.
  fetcher_->SetChunkedUpload("application/x-www-form-urlencoded");
  fetcher_->Start();
  number_of_chunks_added_ = 1;
  fetcher_->AppendChunkToUpload(chunk_, false);
}

void URLFetcherUploadProgressTest::OnURLFetchUploadProgress(
    const URLFetcher* source, int64 current, int64 total) {
  // Increasing between 0 and total.
  EXPECT_LE(0, current);
  EXPECT_GE(static_cast<int64>(chunk_.size()) * number_of_chunks_added_,
            current);
  EXPECT_LE(previous_progress_, current);
  previous_progress_ = current;
  EXPECT_EQ(-1, total);

  if (number_of_chunks_added_ < 2) {
    number_of_chunks_added_ += 1;
    fetcher_->AppendChunkToUpload(chunk_, true);
  }
}

void URLFetcherHeadersTest::OnURLFetchComplete(
    const URLFetcher* source) {
  std::string header;
  EXPECT_TRUE(source->GetResponseHeaders()->GetNormalizedHeader("cache-control",
                                                                &header));
  EXPECT_EQ("private", header);
  URLFetcherTest::OnURLFetchComplete(source);
}

void URLFetcherSocketAddressTest::OnURLFetchComplete(
    const URLFetcher* source) {
  EXPECT_EQ("127.0.0.1", source->GetSocketAddress().host());
  EXPECT_EQ(expected_port_, source->GetSocketAddress().port());
  URLFetcherTest::OnURLFetchComplete(source);
}

// static
const char* URLFetcherStopOnRedirectTest::kRedirectTarget =
    "http://redirect.target.com";

URLFetcherStopOnRedirectTest::URLFetcherStopOnRedirectTest()
    : callback_called_(false) {
}

URLFetcherStopOnRedirectTest::~URLFetcherStopOnRedirectTest() {
}

void URLFetcherStopOnRedirectTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));
  fetcher_->SetStopOnRedirect(true);
  fetcher_->Start();
}

void URLFetcherStopOnRedirectTest::OnURLFetchComplete(
    const URLFetcher* source) {
  callback_called_ = true;
  EXPECT_EQ(GURL(kRedirectTarget), source->GetURL());
  EXPECT_EQ(URLRequestStatus::CANCELED, source->GetStatus().status());
  EXPECT_EQ(ERR_ABORTED, source->GetStatus().error());
  EXPECT_EQ(301, source->GetResponseCode());
  CleanupAfterFetchComplete();
}

void URLFetcherProtectTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));
  start_time_ = Time::Now();
  fetcher_->SetMaxRetriesOn5xx(11);
  fetcher_->Start();
}

void URLFetcherProtectTest::OnURLFetchComplete(const URLFetcher* source) {
  const TimeDelta one_second = TimeDelta::FromMilliseconds(1000);
  if (source->GetResponseCode() >= 500) {
    // Now running ServerUnavailable test.
    // It takes more than 1 second to finish all 11 requests.
    EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
    EXPECT_TRUE(source->GetStatus().is_success());
    std::string data;
    EXPECT_TRUE(source->GetResponseAsString(&data));
    EXPECT_FALSE(data.empty());
    CleanupAfterFetchComplete();
  } else {
    // Now running Overload test.
    static int count = 0;
    count++;
    if (count < 20) {
      fetcher_->SetRequestContext(
          new ThrottlingTestURLRequestContextGetter(
              io_message_loop_proxy(), request_context()));
      fetcher_->Start();
    } else {
      // We have already sent 20 requests continuously. And we expect that
      // it takes more than 1 second due to the overload protection settings.
      EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
      URLFetcherTest::OnURLFetchComplete(source);
    }
  }
}

void URLFetcherProtectTestPassedThrough::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));
  fetcher_->SetAutomaticallyRetryOn5xx(false);
  start_time_ = Time::Now();
  fetcher_->SetMaxRetriesOn5xx(11);
  fetcher_->Start();
}

void URLFetcherProtectTestPassedThrough::OnURLFetchComplete(
    const URLFetcher* source) {
  const TimeDelta one_minute = TimeDelta::FromMilliseconds(60000);
  if (source->GetResponseCode() >= 500) {
    // Now running ServerUnavailable test.
    // It should get here on the first attempt, so almost immediately and
    // *not* to attempt to execute all 11 requests (2.5 minutes).
    EXPECT_TRUE(Time::Now() - start_time_ < one_minute);
    EXPECT_TRUE(source->GetStatus().is_success());
    // Check that suggested back off time is bigger than 0.
    EXPECT_GT(fetcher_->GetBackoffDelay().InMicroseconds(), 0);
    std::string data;
    EXPECT_TRUE(source->GetResponseAsString(&data));
    EXPECT_FALSE(data.empty());
  } else {
    // We should not get here!
    ADD_FAILURE();
  }

  CleanupAfterFetchComplete();
}


URLFetcherBadHTTPSTest::URLFetcherBadHTTPSTest() {
  PathService::Get(base::DIR_SOURCE_ROOT, &cert_dir_);
  cert_dir_ = cert_dir_.AppendASCII("chrome");
  cert_dir_ = cert_dir_.AppendASCII("test");
  cert_dir_ = cert_dir_.AppendASCII("data");
  cert_dir_ = cert_dir_.AppendASCII("ssl");
  cert_dir_ = cert_dir_.AppendASCII("certificates");
}

// The "server certificate expired" error should result in automatic
// cancellation of the request by
// URLRequest::Delegate::OnSSLCertificateError.
void URLFetcherBadHTTPSTest::OnURLFetchComplete(
    const URLFetcher* source) {
  // This part is different from URLFetcherTest::OnURLFetchComplete
  // because this test expects the request to be cancelled.
  EXPECT_EQ(URLRequestStatus::CANCELED, source->GetStatus().status());
  EXPECT_EQ(ERR_ABORTED, source->GetStatus().error());
  EXPECT_EQ(-1, source->GetResponseCode());
  EXPECT_TRUE(source->GetCookies().empty());
  std::string data;
  EXPECT_TRUE(source->GetResponseAsString(&data));
  EXPECT_TRUE(data.empty());
  CleanupAfterFetchComplete();
}

void URLFetcherCancelTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  CancelTestURLRequestContextGetter* context_getter =
      new CancelTestURLRequestContextGetter(io_message_loop_proxy(),
                                            url);
  fetcher_->SetRequestContext(context_getter);
  fetcher_->SetMaxRetriesOn5xx(2);
  fetcher_->Start();
  // We need to wait for the creation of the URLRequestContext, since we
  // rely on it being destroyed as a signal to end the test.
  context_getter->WaitForContextCreation();
  CancelRequest();
}

void URLFetcherCancelTest::OnURLFetchComplete(
    const URLFetcher* source) {
  // We should have cancelled the request before completion.
  ADD_FAILURE();
  CleanupAfterFetchComplete();
}

void URLFetcherCancelTest::CancelRequest() {
  delete fetcher_;
  // The URLFetcher's test context will post a Quit task once it is
  // deleted. So if this test simply hangs, it means cancellation
  // did not work.
}

void URLFetcherMultipleAttemptTest::OnURLFetchComplete(
    const URLFetcher* source) {
  EXPECT_TRUE(source->GetStatus().is_success());
  EXPECT_EQ(200, source->GetResponseCode());  // HTTP OK
  std::string data;
  EXPECT_TRUE(source->GetResponseAsString(&data));
  EXPECT_FALSE(data.empty());
  if (!data.empty() && data_.empty()) {
    data_ = data;
    fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
        io_message_loop_proxy(), request_context()));
    fetcher_->Start();
  } else {
    EXPECT_EQ(data, data_);
    CleanupAfterFetchComplete();
  }
}

void URLFetcherFileTest::CreateFetcherForFile(const GURL& url,
                                              const FilePath& file_path) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));

  // Use the IO message loop to do the file operations in this test.
  fetcher_->SaveResponseToFileAtPath(file_path, io_message_loop_proxy());
  fetcher_->Start();
}

void URLFetcherFileTest::CreateFetcherForTempFile(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy(), request_context()));

  // Use the IO message loop to do the file operations in this test.
  fetcher_->SaveResponseToTemporaryFile(io_message_loop_proxy());
  fetcher_->Start();
}

void URLFetcherFileTest::OnURLFetchComplete(const URLFetcher* source) {
  if (expected_file_error_ == base::PLATFORM_FILE_OK) {
    EXPECT_TRUE(source->GetStatus().is_success());
    EXPECT_EQ(source->GetResponseCode(), 200);

    base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
    EXPECT_FALSE(fetcher_->FileErrorOccurred(&error_code));

    EXPECT_TRUE(source->GetResponseAsFilePath(
        take_ownership_of_file_, &file_path_));

    EXPECT_TRUE(file_util::ContentsEqual(expected_file_, file_path_));
  } else {
    base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
    EXPECT_TRUE(fetcher_->FileErrorOccurred(&error_code));
    EXPECT_EQ(expected_file_error_, error_code);
  }
  CleanupAfterFetchComplete();
}

TEST_F(URLFetcherTest, SameThreadsTest) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Create the fetcher on the main thread.  Since IO will happen on the main
  // thread, this will test URLFetcher's ability to do everything on one
  // thread.
  CreateFetcher(test_server.GetURL("defaultresponse"));

  MessageLoop::current()->Run();
}

#if defined(OS_MACOSX)
// SIGSEGV on Mac: http://crbug.com/60426
TEST_F(URLFetcherTest, DISABLED_DifferentThreadsTest) {
#else
TEST_F(URLFetcherTest, DifferentThreadsTest) {
#endif
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Create a separate thread that will create the URLFetcher.  The current
  // (main) thread will do the IO, and when the fetch is complete it will
  // terminate the main thread's message loop; then the other thread's
  // message loop will be shut down automatically as the thread goes out of
  // scope.
  base::Thread t("URLFetcher test thread");
  ASSERT_TRUE(t.Start());
  t.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&URLFetcherTest::CreateFetcher,
                 base::Unretained(this),
                 test_server.GetURL("defaultresponse")));

  MessageLoop::current()->Run();
}

void CancelAllOnIO() {
  EXPECT_EQ(1, URLFetcherTest::GetNumFetcherCores());
  URLFetcherImpl::CancelAll();
  EXPECT_EQ(0, URLFetcherTest::GetNumFetcherCores());
}

// Tests to make sure CancelAll() will successfully cancel existing URLFetchers.
TEST_F(URLFetcherTest, CancelAll) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());
  EXPECT_EQ(0, GetNumFetcherCores());

  CreateFetcher(test_server.GetURL("defaultresponse"));
  io_message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CancelAllOnIO),
      MessageLoop::QuitClosure());
  MessageLoop::current()->Run();
  EXPECT_EQ(0, GetNumFetcherCores());
  delete fetcher_;
}

TEST_F(URLFetcherMockDnsTest, DontRetryOnNetworkChangedByDefault) {
  EXPECT_EQ(0, GetNumFetcherCores());
  EXPECT_FALSE(resolver_.has_pending_requests());

  // This posts a task to start the fetcher.
  CreateFetcher(test_url_);
  fetcher_->Start();
  EXPECT_EQ(0, GetNumFetcherCores());
  MessageLoop::current()->RunUntilIdle();

  // The fetcher is now running, but is pending the host resolve.
  EXPECT_EQ(1, GetNumFetcherCores());
  EXPECT_TRUE(resolver_.has_pending_requests());
  ASSERT_FALSE(completed_fetcher_);

  // A network change notification aborts the connect job.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, GetNumFetcherCores());
  EXPECT_FALSE(resolver_.has_pending_requests());
  ASSERT_TRUE(completed_fetcher_);

  // And the owner of the fetcher gets the ERR_NETWORK_CHANGED error.
  EXPECT_EQ(ERR_NETWORK_CHANGED, completed_fetcher_->GetStatus().error());
}

TEST_F(URLFetcherMockDnsTest, RetryOnNetworkChangedAndFail) {
  EXPECT_EQ(0, GetNumFetcherCores());
  EXPECT_FALSE(resolver_.has_pending_requests());

  // This posts a task to start the fetcher.
  CreateFetcher(test_url_);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
  fetcher_->Start();
  EXPECT_EQ(0, GetNumFetcherCores());
  MessageLoop::current()->RunUntilIdle();

  // The fetcher is now running, but is pending the host resolve.
  EXPECT_EQ(1, GetNumFetcherCores());
  EXPECT_TRUE(resolver_.has_pending_requests());
  ASSERT_FALSE(completed_fetcher_);

  // Make it fail 3 times.
  for (int i = 0; i < 3; ++i) {
    // A network change notification aborts the connect job.
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    MessageLoop::current()->RunUntilIdle();

    // But the fetcher retries automatically.
    EXPECT_EQ(1, GetNumFetcherCores());
    EXPECT_TRUE(resolver_.has_pending_requests());
    ASSERT_FALSE(completed_fetcher_);
  }

  // A 4th failure doesn't trigger another retry, and propagates the error
  // to the owner of the fetcher.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, GetNumFetcherCores());
  EXPECT_FALSE(resolver_.has_pending_requests());
  ASSERT_TRUE(completed_fetcher_);

  // And the owner of the fetcher gets the ERR_NETWORK_CHANGED error.
  EXPECT_EQ(ERR_NETWORK_CHANGED, completed_fetcher_->GetStatus().error());
}

TEST_F(URLFetcherMockDnsTest, RetryOnNetworkChangedAndSucceed) {
  EXPECT_EQ(0, GetNumFetcherCores());
  EXPECT_FALSE(resolver_.has_pending_requests());

  // This posts a task to start the fetcher.
  CreateFetcher(test_url_);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
  fetcher_->Start();
  EXPECT_EQ(0, GetNumFetcherCores());
  MessageLoop::current()->RunUntilIdle();

  // The fetcher is now running, but is pending the host resolve.
  EXPECT_EQ(1, GetNumFetcherCores());
  EXPECT_TRUE(resolver_.has_pending_requests());
  ASSERT_FALSE(completed_fetcher_);

  // Make it fail 3 times.
  for (int i = 0; i < 3; ++i) {
    // A network change notification aborts the connect job.
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    MessageLoop::current()->RunUntilIdle();

    // But the fetcher retries automatically.
    EXPECT_EQ(1, GetNumFetcherCores());
    EXPECT_TRUE(resolver_.has_pending_requests());
    ASSERT_FALSE(completed_fetcher_);
  }

  // Now let it succeed by resolving the pending request.
  resolver_.ResolveAllPending();
  MessageLoop::current()->Run();

  // URLFetcherMockDnsTest::OnURLFetchComplete() will quit the loop.
  EXPECT_EQ(0, GetNumFetcherCores());
  EXPECT_FALSE(resolver_.has_pending_requests());
  ASSERT_TRUE(completed_fetcher_);

  // This time the request succeeded.
  EXPECT_EQ(OK, completed_fetcher_->GetStatus().error());
  EXPECT_EQ(200, completed_fetcher_->GetResponseCode());
}

#if defined(OS_MACOSX)
// SIGSEGV on Mac: http://crbug.com/60426
TEST_F(URLFetcherPostTest, DISABLED_Basic) {
#else
TEST_F(URLFetcherPostTest, Basic) {
#endif
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  CreateFetcher(test_server.GetURL("echo"));
  MessageLoop::current()->Run();
}

TEST_F(URLFetcherEmptyPostTest, Basic) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  CreateFetcher(test_server.GetURL("echo"));
  MessageLoop::current()->Run();
}

#if defined(OS_MACOSX)
// SIGSEGV on Mac: http://crbug.com/60426
TEST_F(URLFetcherUploadProgressTest, DISABLED_Basic) {
#else
TEST_F(URLFetcherUploadProgressTest, Basic) {
#endif
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  CreateFetcher(test_server.GetURL("echo"));
  MessageLoop::current()->Run();
}

TEST_F(URLFetcherDownloadProgressTest, Basic) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  static const char kFileToFetch[] = "animate1.gif";
  // Hardcoded file size - it cannot be easily fetched when a remote http server
  // is used for testing.
  static const int64 kFileSize = 19021;

  expected_total_ = kFileSize;

  CreateFetcher(test_server.GetURL(
      std::string(kTestServerFilePrefix) + kFileToFetch));

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherDownloadProgressCancelTest, CancelWhileProgressReport) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  static const char kFileToFetch[] = "animate1.gif";
  CreateFetcher(test_server.GetURL(
      std::string(kTestServerFilePrefix) + kFileToFetch));

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherHeadersTest, Headers) {
  TestServer test_server(
      TestServer::TYPE_HTTP,
      TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(test_server.Start());

  CreateFetcher(test_server.GetURL("files/with-headers.html"));
  MessageLoop::current()->Run();
  // The actual tests are in the URLFetcherHeadersTest fixture.
}

TEST_F(URLFetcherSocketAddressTest, SocketAddress) {
  TestServer test_server(
      TestServer::TYPE_HTTP,
      TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(test_server.Start());
  expected_port_ = test_server.host_port_pair().port();

  // Reusing "with-headers.html" but doesn't really matter.
  CreateFetcher(test_server.GetURL("files/with-headers.html"));
  MessageLoop::current()->Run();
  // The actual tests are in the URLFetcherSocketAddressTest fixture.
}

TEST_F(URLFetcherStopOnRedirectTest, StopOnRedirect) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  CreateFetcher(
      test_server.GetURL(std::string("server-redirect?") + kRedirectTarget));
  MessageLoop::current()->Run();
  EXPECT_TRUE(callback_called_);
}

TEST_F(URLFetcherProtectTest, Overload) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("defaultresponse"));

  // Registers an entry for test url. It only allows 3 requests to be sent
  // in 200 milliseconds.
  scoped_refptr<URLRequestThrottlerEntry> entry(
      new URLRequestThrottlerEntry(
          request_context()->throttler_manager(),
          "", 200, 3, 1, 2.0, 0.0, 256));
  request_context()->throttler_manager()->OverrideEntryForTests(url, entry);

  CreateFetcher(url);

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherProtectTest, ServerUnavailable) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("files/server-unavailable.html"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 256 milliseconds.
  // Maximum retries allowed is set to 11.
  scoped_refptr<URLRequestThrottlerEntry> entry(
      new URLRequestThrottlerEntry(
          request_context()->throttler_manager(),
          "", 200, 3, 1, 2.0, 0.0, 256));
  request_context()->throttler_manager()->OverrideEntryForTests(url, entry);

  CreateFetcher(url);

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherProtectTestPassedThrough, ServerUnavailablePropagateResponse) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("files/server-unavailable.html"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 150000 milliseconds.
  // Maximum retries allowed is set to 11.
  scoped_refptr<URLRequestThrottlerEntry> entry(
      new URLRequestThrottlerEntry(
          request_context()->throttler_manager(),
          "", 200, 3, 100, 2.0, 0.0, 150000));
  // Total time if *not* for not doing automatic backoff would be 150s.
  // In reality it should be "as soon as server responds".
  request_context()->throttler_manager()->OverrideEntryForTests(url, entry);

  CreateFetcher(url);

  MessageLoop::current()->Run();
}

#if defined(OS_MACOSX)
// SIGSEGV on Mac: http://crbug.com/60426
TEST_F(URLFetcherBadHTTPSTest, DISABLED_BadHTTPSTest) {
#else
TEST_F(URLFetcherBadHTTPSTest, BadHTTPSTest) {
#endif
  TestServer::SSLOptions ssl_options(
      TestServer::SSLOptions::CERT_EXPIRED);
  TestServer test_server(TestServer::TYPE_HTTPS,
                         ssl_options,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  CreateFetcher(test_server.GetURL("defaultresponse"));
  MessageLoop::current()->Run();
}

#if defined(OS_MACOSX)
// SIGSEGV on Mac: http://crbug.com/60426
TEST_F(URLFetcherCancelTest, DISABLED_ReleasesContext) {
#else
TEST_F(URLFetcherCancelTest, ReleasesContext) {
#endif
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("files/server-unavailable.html"));

  // Create a separate thread that will create the URLFetcher.  The current
  // (main) thread will do the IO, and when the fetch is complete it will
  // terminate the main thread's message loop; then the other thread's
  // message loop will be shut down automatically as the thread goes out of
  // scope.
  base::Thread t("URLFetcher test thread");
  ASSERT_TRUE(t.Start());
  t.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&URLFetcherCancelTest::CreateFetcher,
                 base::Unretained(this), url));

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherCancelTest, CancelWhileDelayedStartTaskPending) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("files/server-unavailable.html"));

  // Register an entry for test url.
  // Using a sliding window of 4 seconds, and max of 1 request, under a fast
  // run we expect to have a 4 second delay when posting the Start task.
  scoped_refptr<URLRequestThrottlerEntry> entry(
      new URLRequestThrottlerEntry(
          request_context()->throttler_manager(),
          "", 4000, 1, 2000, 2.0, 0.0, 4000));
  request_context()->throttler_manager()->OverrideEntryForTests(url, entry);
  // Fake that a request has just started.
  entry->ReserveSendingTimeForNextRequest(base::TimeTicks());

  // The next request we try to send will be delayed by ~4 seconds.
  // The slower the test runs, the less the delay will be (since it takes the
  // time difference from now).

  base::Thread t("URLFetcher test thread");
  ASSERT_TRUE(t.Start());
  t.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&URLFetcherTest::CreateFetcher, base::Unretained(this), url));

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherMultipleAttemptTest, SameData) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Create the fetcher on the main thread.  Since IO will happen on the main
  // thread, this will test URLFetcher's ability to do everything on one
  // thread.
  CreateFetcher(test_server.GetURL("defaultresponse"));

  MessageLoop::current()->Run();
}

TEST_F(URLFetcherFileTest, SmallGet) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Get a small file.
  static const char kFileToFetch[] = "simple.html";
  expected_file_ = test_server.GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForFile(
      test_server.GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      temp_dir.path().AppendASCII(kFileToFetch));

  MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

  ASSERT_FALSE(file_util::PathExists(file_path_))
      << file_path_.value() << " not removed.";
}

TEST_F(URLFetcherFileTest, LargeGet) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  static const char kFileToFetch[] = "animate1.gif";
  expected_file_ = test_server.GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForFile(
      test_server.GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      temp_dir.path().AppendASCII(kFileToFetch));

  MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().
}

TEST_F(URLFetcherFileTest, CanTakeOwnershipOfFile) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Get a small file.
  static const char kFileToFetch[] = "simple.html";
  expected_file_ = test_server.GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForFile(
      test_server.GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      temp_dir.path().AppendASCII(kFileToFetch));

  MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

  MessageLoop::current()->RunUntilIdle();
  ASSERT_FALSE(file_util::PathExists(file_path_))
      << file_path_.value() << " not removed.";
}


TEST_F(URLFetcherFileTest, OverwriteExistingFile) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a file before trying to fetch.
  static const char kFileToFetch[] = "simple.html";
  static const char kData[] = "abcdefghijklmnopqrstuvwxyz";
  file_path_ = temp_dir.path().AppendASCII(kFileToFetch);
  const int data_size = arraysize(kData);
  ASSERT_EQ(file_util::WriteFile(file_path_, kData, data_size), data_size);
  ASSERT_TRUE(file_util::PathExists(file_path_));
  expected_file_ = test_server.GetDocumentRoot().AppendASCII(kFileToFetch);
  ASSERT_FALSE(file_util::ContentsEqual(file_path_, expected_file_));

  // Get a small file.
  CreateFetcherForFile(
      test_server.GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      file_path_);

  MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().
}

TEST_F(URLFetcherFileTest, TryToOverwriteDirectory) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a directory before trying to fetch.
  static const char kFileToFetch[] = "simple.html";
  file_path_ = temp_dir.path().AppendASCII(kFileToFetch);
  ASSERT_TRUE(file_util::CreateDirectory(file_path_));
  ASSERT_TRUE(file_util::PathExists(file_path_));

  // Get a small file.
  expected_file_error_ = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
  expected_file_ = test_server.GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForFile(
      test_server.GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      file_path_);

  MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

  MessageLoop::current()->RunUntilIdle();
}

TEST_F(URLFetcherFileTest, SmallGetToTempFile) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Get a small file.
  static const char kFileToFetch[] = "simple.html";
  expected_file_ = test_server.GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForTempFile(
      test_server.GetURL(std::string(kTestServerFilePrefix) + kFileToFetch));

  MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

  ASSERT_FALSE(file_util::PathExists(file_path_))
      << file_path_.value() << " not removed.";
}

TEST_F(URLFetcherFileTest, LargeGetToTempFile) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  static const char kFileToFetch[] = "animate1.gif";
  expected_file_ = test_server.GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForTempFile(test_server.GetURL(
      std::string(kTestServerFilePrefix) + kFileToFetch));

  MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().
}

TEST_F(URLFetcherFileTest, CanTakeOwnershipOfTempFile) {
  TestServer test_server(TestServer::TYPE_HTTP,
                         TestServer::kLocalhost,
                         FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Get a small file.
  static const char kFileToFetch[] = "simple.html";
  expected_file_ = test_server.GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForTempFile(test_server.GetURL(
      std::string(kTestServerFilePrefix) + kFileToFetch));

  MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

  MessageLoop::current()->RunUntilIdle();
  ASSERT_FALSE(file_util::PathExists(file_path_))
      << file_path_.value() << " not removed.";
}

}  // namespace

}  // namespace net
