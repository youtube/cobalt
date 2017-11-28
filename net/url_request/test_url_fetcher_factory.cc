// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/test_url_fetcher_factory.h"

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_impl.h"
#include "net/url_request/url_request_status.h"

namespace net {

ScopedURLFetcherFactory::ScopedURLFetcherFactory(
    URLFetcherFactory* factory) {
  DCHECK(!URLFetcherImpl::factory());
  URLFetcherImpl::set_factory(factory);
}

ScopedURLFetcherFactory::~ScopedURLFetcherFactory() {
  DCHECK(URLFetcherImpl::factory());
  URLFetcherImpl::set_factory(NULL);
}

TestURLFetcher::TestURLFetcher(int id,
                               const GURL& url,
                               URLFetcherDelegate* d)
    : owner_(NULL),
      id_(id),
      original_url_(url),
      delegate_(d),
      delegate_for_tests_(NULL),
      did_receive_last_chunk_(false),
      fake_load_flags_(0),
      fake_response_code_(-1),
      fake_response_destination_(STRING),
      fake_was_fetched_via_proxy_(false),
      fake_max_retries_(0) {
}

TestURLFetcher::~TestURLFetcher() {
  if (delegate_for_tests_)
    delegate_for_tests_->OnRequestEnd(id_);
  if (owner_)
    owner_->RemoveFetcherFromMap(id_);
}

void TestURLFetcher::SetUploadData(const std::string& upload_content_type,
                                   const std::string& upload_content) {
  upload_data_ = upload_content;
}

void TestURLFetcher::SetChunkedUpload(const std::string& upload_content_type) {
}

void TestURLFetcher::AppendChunkToUpload(const std::string& data,
                                         bool is_last_chunk) {
  DCHECK(!did_receive_last_chunk_);
  did_receive_last_chunk_ = is_last_chunk;
  chunks_.push_back(data);
  if (delegate_for_tests_)
    delegate_for_tests_->OnChunkUpload(id_);
}

void TestURLFetcher::SetLoadFlags(int load_flags) {
  fake_load_flags_= load_flags;
}

int TestURLFetcher::GetLoadFlags() const {
  return fake_load_flags_;
}

void TestURLFetcher::SetReferrer(const std::string& referrer) {
}

void TestURLFetcher::SetExtraRequestHeaders(
    const std::string& extra_request_headers) {
  fake_extra_request_headers_.Clear();
  fake_extra_request_headers_.AddHeadersFromString(extra_request_headers);
}

void TestURLFetcher::AddExtraRequestHeader(const std::string& header_line) {
  fake_extra_request_headers_.AddHeaderFromString(header_line);
}

void TestURLFetcher::GetExtraRequestHeaders(
    HttpRequestHeaders* headers) const {
  *headers = fake_extra_request_headers_;
}

void TestURLFetcher::SetRequestContext(
    URLRequestContextGetter* request_context_getter) {
}

void TestURLFetcher::SetFirstPartyForCookies(
    const GURL& first_party_for_cookies) {
}

void TestURLFetcher::SetURLRequestUserData(
    const void* key,
    const CreateDataCallback& create_data_callback) {
}

void TestURLFetcher::SetStopOnRedirect(bool stop_on_redirect) {
}

void TestURLFetcher::SetAutomaticallyRetryOn5xx(bool retry) {
}

void TestURLFetcher::SetMaxRetriesOn5xx(int max_retries) {
  fake_max_retries_ = max_retries;
}

int TestURLFetcher::GetMaxRetriesOn5xx() const {
  return fake_max_retries_;
}

base::TimeDelta TestURLFetcher::GetBackoffDelay() const {
  return fake_backoff_delay_;
}

void TestURLFetcher::SetAutomaticallyRetryOnNetworkChanges(int max_retries) {
}

void TestURLFetcher::SaveResponseToFileAtPath(
    const FilePath& file_path,
    scoped_refptr<base::TaskRunner> file_task_runner) {
}

void TestURLFetcher::SaveResponseToTemporaryFile(
    scoped_refptr<base::TaskRunner> file_task_runner) {
}

#if defined(COBALT)
void TestURLFetcher::DiscardResponse() {
}
#endif

HttpResponseHeaders* TestURLFetcher::GetResponseHeaders() const {
  return fake_response_headers_;
}

HostPortPair TestURLFetcher::GetSocketAddress() const {
  NOTIMPLEMENTED();
  return HostPortPair();
}

bool TestURLFetcher::WasFetchedViaProxy() const {
  return fake_was_fetched_via_proxy_;
}

void TestURLFetcher::Start() {
  // Overriden to do nothing. It is assumed the caller will notify the delegate.
  if (delegate_for_tests_)
    delegate_for_tests_->OnRequestStart(id_);
}

const GURL& TestURLFetcher::GetOriginalURL() const {
  return original_url_;
}

const GURL& TestURLFetcher::GetURL() const {
  return fake_url_;
}

const URLRequestStatus& TestURLFetcher::GetStatus() const {
  return fake_status_;
}

int TestURLFetcher::GetResponseCode() const {
  return fake_response_code_;
}

const ResponseCookies& TestURLFetcher::GetCookies() const {
  return fake_cookies_;
}

bool TestURLFetcher::FileErrorOccurred(
    base::PlatformFileError* out_error_code) const {
  NOTIMPLEMENTED();
  return false;
}

void TestURLFetcher::ReceivedContentWasMalformed() {
}

bool TestURLFetcher::GetResponseAsString(
    std::string* out_response_string) const {
  if (fake_response_destination_ != STRING)
    return false;

  *out_response_string = fake_response_string_;
  return true;
}

bool TestURLFetcher::GetResponseAsFilePath(
    bool take_ownership, FilePath* out_response_path) const {
  if (fake_response_destination_ != TEMP_FILE)
    return false;

  *out_response_path = fake_response_file_path_;
  return true;
}

void TestURLFetcher::set_status(const URLRequestStatus& status) {
  fake_status_ = status;
}

void TestURLFetcher::set_was_fetched_via_proxy(bool flag) {
  fake_was_fetched_via_proxy_ = flag;
}

void TestURLFetcher::set_response_headers(
    scoped_refptr<HttpResponseHeaders> headers) {
  fake_response_headers_ = headers;
}

void TestURLFetcher::set_backoff_delay(base::TimeDelta backoff_delay) {
  fake_backoff_delay_ = backoff_delay;
}

void TestURLFetcher::SetDelegateForTests(DelegateForTests* delegate_for_tests) {
  delegate_for_tests_ = delegate_for_tests;
}

void TestURLFetcher::SetResponseString(const std::string& response) {
  fake_response_destination_ = STRING;
  fake_response_string_ = response;
}

void TestURLFetcher::SetResponseFilePath(const FilePath& path) {
  fake_response_destination_ = TEMP_FILE;
  fake_response_file_path_ = path;
}

TestURLFetcherFactory::TestURLFetcherFactory()
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      delegate_for_tests_(NULL),
      remove_fetcher_on_delete_(false) {
}

TestURLFetcherFactory::~TestURLFetcherFactory() {}

URLFetcher* TestURLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d) {
  TestURLFetcher* fetcher = new TestURLFetcher(id, url, d);
  if (remove_fetcher_on_delete_)
    fetcher->set_owner(this);
  fetcher->SetDelegateForTests(delegate_for_tests_);
  fetchers_[id] = fetcher;
  return fetcher;
}

TestURLFetcher* TestURLFetcherFactory::GetFetcherByID(int id) const {
  Fetchers::const_iterator i = fetchers_.find(id);
  return i == fetchers_.end() ? NULL : i->second;
}

void TestURLFetcherFactory::RemoveFetcherFromMap(int id) {
  Fetchers::iterator i = fetchers_.find(id);
  DCHECK(i != fetchers_.end());
  fetchers_.erase(i);
}

void TestURLFetcherFactory::SetDelegateForTests(
    TestURLFetcherDelegateForTests* delegate_for_tests) {
  delegate_for_tests_ = delegate_for_tests;
}

// This class is used by the FakeURLFetcherFactory below.
class FakeURLFetcher : public TestURLFetcher {
 public:
  // Normal URL fetcher constructor but also takes in a pre-baked response.
  FakeURLFetcher(const GURL& url,
                 URLFetcherDelegate* d,
                 const std::string& response_data, bool success)
      : TestURLFetcher(0, url, d),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
    set_status(URLRequestStatus(
        success ? URLRequestStatus::SUCCESS : URLRequestStatus::FAILED,
        0));
    set_response_code(success ? 200 : 500);
    SetResponseString(response_data);
  }

  // Start the request.  This will call the given delegate asynchronously
  // with the pre-baked response as parameter.
  virtual void Start() override {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FakeURLFetcher::RunDelegate, weak_factory_.GetWeakPtr()));
  }

  virtual const GURL& GetURL() const override {
    return TestURLFetcher::GetOriginalURL();
  }

 private:
  virtual ~FakeURLFetcher() {
  }

  // This is the method which actually calls the delegate that is passed in the
  // constructor.
  void RunDelegate() {
    delegate()->OnURLFetchComplete(this);
  }

  base::WeakPtrFactory<FakeURLFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeURLFetcher);
};

FakeURLFetcherFactory::FakeURLFetcherFactory()
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      default_factory_(NULL) {
}

FakeURLFetcherFactory::FakeURLFetcherFactory(
    URLFetcherFactory* default_factory)
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      default_factory_(default_factory) {
}

FakeURLFetcherFactory::~FakeURLFetcherFactory() {}

URLFetcher* FakeURLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d) {
  FakeResponseMap::const_iterator it = fake_responses_.find(url);
  if (it == fake_responses_.end()) {
    if (default_factory_ == NULL) {
      // If we don't have a baked response for that URL we return NULL.
      DLOG(ERROR) << "No baked response for URL: " << url.spec();
      return NULL;
    } else {
      return default_factory_->CreateURLFetcher(id, url, request_type, d);
    }
  }
  return new FakeURLFetcher(url, d, it->second.first, it->second.second);
}

void FakeURLFetcherFactory::SetFakeResponse(const std::string& url,
                                            const std::string& response_data,
                                            bool success) {
  // Overwrite existing URL if it already exists.
  fake_responses_[GURL(url)] = std::make_pair(response_data, success);
}

void FakeURLFetcherFactory::ClearFakeResponses() {
  fake_responses_.clear();
}

URLFetcherImplFactory::URLFetcherImplFactory() {}

URLFetcherImplFactory::~URLFetcherImplFactory() {}

URLFetcher* URLFetcherImplFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d) {
  return new URLFetcherImpl(url, request_type, d);
}

}  // namespace net
