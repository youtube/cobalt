// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_TEST_URL_FETCHER_FACTORY_H_
#define NET_URL_REQUEST_TEST_URL_FETCHER_FACTORY_H_

#include <list>
#include <map>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"

namespace net {

// Changes URLFetcher's Factory for the lifetime of the object.
// Note that this scoper cannot be nested (to make it even harder to misuse).
class ScopedURLFetcherFactory : public base::NonThreadSafe {
 public:
  explicit ScopedURLFetcherFactory(URLFetcherFactory* factory);
  virtual ~ScopedURLFetcherFactory();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedURLFetcherFactory);
};

// TestURLFetcher and TestURLFetcherFactory are used for testing consumers of
// URLFetcher. TestURLFetcherFactory is a URLFetcherFactory that creates
// TestURLFetchers. TestURLFetcher::Start is overriden to do nothing. It is
// expected that you'll grab the delegate from the TestURLFetcher and invoke
// the callback method when appropriate. In this way it's easy to mock a
// URLFetcher.
// Typical usage:
//   // TestURLFetcher requires a MessageLoop.
//   MessageLoop message_loop;
//   // And an IO thread to release URLRequestContextGetter in URLFetcher::Core.
//   BrowserThreadImpl io_thread(BrowserThread::IO, &message_loop);
//   // Create factory (it automatically sets itself as URLFetcher's factory).
//   TestURLFetcherFactory factory;
//   // Do something that triggers creation of a URLFetcher.
//   ...
//   TestURLFetcher* fetcher = factory.GetFetcherByID(expected_id);
//   DCHECK(fetcher);
//   // Notify delegate with whatever data you want.
//   fetcher->delegate()->OnURLFetchComplete(...);
//   // Make sure consumer of URLFetcher does the right thing.
//   ...
//
// Note: if you don't know when your request objects will be created you
// might want to use the FakeURLFetcher and FakeURLFetcherFactory classes
// below.

class TestURLFetcherFactory;
class TestURLFetcher : public URLFetcher {
 public:
  // Interface for tests to intercept production code classes using URLFetcher.
  // Allows even-driven mock server classes to analyze the correctness of
  // requests / uploads events and forge responses back at the right moment.
  class DelegateForTests {
   public:
    // Callback issued correspondingly to the call to the |Start()| method.
    virtual void OnRequestStart(int fetcher_id) = 0;

    // Callback issued correspondingly to the call to |AppendChunkToUpload|.
    // Uploaded chunks can be retrieved with the |upload_chunks()| getter.
    virtual void OnChunkUpload(int fetcher_id) = 0;

    // Callback issued correspondingly to the destructor.
    virtual void OnRequestEnd(int fetcher_id) = 0;
  };

  TestURLFetcher(int id,
                 const GURL& url,
                 URLFetcherDelegate* d);
  virtual ~TestURLFetcher();

  // URLFetcher implementation
  virtual void SetUploadData(const std::string& upload_content_type,
                             const std::string& upload_content) override;
  virtual void SetChunkedUpload(
      const std::string& upload_content_type) override;
  // Overriden to cache the chunks uploaded. Caller can read back the uploaded
  // chunks with the upload_chunks() accessor.
  virtual void AppendChunkToUpload(const std::string& data,
                                   bool is_last_chunk) override;
  virtual void SetLoadFlags(int load_flags) override;
  virtual int GetLoadFlags() const override;
  virtual void SetReferrer(const std::string& referrer) override;
  virtual void SetExtraRequestHeaders(
      const std::string& extra_request_headers) override;
  virtual void AddExtraRequestHeader(const std::string& header_line) override;
  virtual void GetExtraRequestHeaders(
      HttpRequestHeaders* headers) const override;
  virtual void SetRequestContext(
      URLRequestContextGetter* request_context_getter) override;
  virtual void SetFirstPartyForCookies(
      const GURL& first_party_for_cookies) override;
  virtual void SetURLRequestUserData(
      const void* key,
      const CreateDataCallback& create_data_callback) override;
  virtual void SetStopOnRedirect(bool stop_on_redirect) override;
  virtual void SetAutomaticallyRetryOn5xx(bool retry) override;
  virtual void SetMaxRetriesOn5xx(int max_retries) override;
  virtual int GetMaxRetriesOn5xx() const override;
  virtual base::TimeDelta GetBackoffDelay() const override;
  virtual void SetAutomaticallyRetryOnNetworkChanges(int max_retries) override;
  virtual void SaveResponseToFileAtPath(
      const FilePath& file_path,
      scoped_refptr<base::TaskRunner> file_task_runner) override;
  virtual void SaveResponseToTemporaryFile(
      scoped_refptr<base::TaskRunner> file_task_runner) override;
#if defined(COBALT)
  virtual void DiscardResponse() override;
#endif
  virtual HttpResponseHeaders* GetResponseHeaders() const override;
  virtual HostPortPair GetSocketAddress() const override;
  virtual bool WasFetchedViaProxy() const override;
  virtual void Start() override;

  // URL we were created with. Because of how we're using URLFetcher GetURL()
  // always returns an empty URL. Chances are you'll want to use
  // GetOriginalURL() in your tests.
  virtual const GURL& GetOriginalURL() const override;
  virtual const GURL& GetURL() const override;
  virtual const URLRequestStatus& GetStatus() const override;
  virtual int GetResponseCode() const override;
  virtual const ResponseCookies& GetCookies() const override;
  virtual bool FileErrorOccurred(
      base::PlatformFileError* out_error_code) const override;
  virtual void ReceivedContentWasMalformed() override;
  // Override response access functions to return fake data.
  virtual bool GetResponseAsString(
      std::string* out_response_string) const override;
  virtual bool GetResponseAsFilePath(
      bool take_ownership, FilePath* out_response_path) const override;

  // Sets owner of this class.  Set it to a non-NULL value if you want
  // to automatically unregister this fetcher from the owning factory
  // upon destruction.
  void set_owner(TestURLFetcherFactory* owner) { owner_ = owner; }

  // Unique ID in our factory.
  int id() const { return id_; }

  // Returns the data uploaded on this URLFetcher.
  const std::string& upload_data() const { return upload_data_; }

  // Returns the chunks of data uploaded on this URLFetcher.
  const std::list<std::string>& upload_chunks() const { return chunks_; }

  // Checks whether the last call to |AppendChunkToUpload(...)| was final.
  bool did_receive_last_chunk() const { return did_receive_last_chunk_; }

  // Returns the delegate installed on the URLFetcher.
  URLFetcherDelegate* delegate() const { return delegate_; }

  void set_url(const GURL& url) { fake_url_ = url; }
  void set_status(const URLRequestStatus& status);
  void set_response_code(int response_code) {
    fake_response_code_ = response_code;
  }
  void set_cookies(const ResponseCookies& c) { fake_cookies_ = c; }
  void set_was_fetched_via_proxy(bool flag);
  void set_response_headers(scoped_refptr<HttpResponseHeaders> headers);
  void set_backoff_delay(base::TimeDelta backoff_delay);
  void SetDelegateForTests(DelegateForTests* delegate_for_tests);

  // Set string data.
  void SetResponseString(const std::string& response);

  // Set File data.
  void SetResponseFilePath(const FilePath& path);

 private:
  enum ResponseDestinationType {
    STRING,  // Default: In a std::string
    TEMP_FILE  // Write to a temp file
  };

  TestURLFetcherFactory* owner_;
  const int id_;
  const GURL original_url_;
  URLFetcherDelegate* delegate_;
  DelegateForTests* delegate_for_tests_;
  std::string upload_data_;
  std::list<std::string> chunks_;
  bool did_receive_last_chunk_;

  // User can use set_* methods to provide values returned by getters.
  // Setting the real values is not possible, because the real class
  // has no setters. The data is a private member of a class defined
  // in a .cc file, so we can't get at it with friendship.
  int fake_load_flags_;
  GURL fake_url_;
  URLRequestStatus fake_status_;
  int fake_response_code_;
  ResponseCookies fake_cookies_;
  ResponseDestinationType fake_response_destination_;
  std::string fake_response_string_;
  FilePath fake_response_file_path_;
  bool fake_was_fetched_via_proxy_;
  scoped_refptr<HttpResponseHeaders> fake_response_headers_;
  HttpRequestHeaders fake_extra_request_headers_;
  int fake_max_retries_;
  base::TimeDelta fake_backoff_delay_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcher);
};

typedef TestURLFetcher::DelegateForTests TestURLFetcherDelegateForTests;

// Simple URLFetcherFactory method that creates TestURLFetchers. All fetchers
// are registered in a map by the id passed to the create method.
// Optionally, a fetcher may be automatically unregistered from the map upon
// its destruction.
class TestURLFetcherFactory : public URLFetcherFactory,
                              public ScopedURLFetcherFactory {
 public:
  TestURLFetcherFactory();
  virtual ~TestURLFetcherFactory();

  virtual URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      URLFetcher::RequestType request_type,
      URLFetcherDelegate* d) override;
  TestURLFetcher* GetFetcherByID(int id) const;
  void RemoveFetcherFromMap(int id);
  void SetDelegateForTests(TestURLFetcherDelegateForTests* delegate_for_tests);
  void set_remove_fetcher_on_delete(bool remove_fetcher_on_delete) {
    remove_fetcher_on_delete_ = remove_fetcher_on_delete;
  }

 private:
  // Maps from id passed to create to the returned URLFetcher.
  typedef std::map<int, TestURLFetcher*> Fetchers;
  Fetchers fetchers_;
  TestURLFetcherDelegateForTests* delegate_for_tests_;
  // Whether to automatically unregister a fetcher from this factory upon its
  // destruction, false by default.
  bool remove_fetcher_on_delete_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcherFactory);
};

// The FakeURLFetcher and FakeURLFetcherFactory classes are similar to the
// ones above but don't require you to know when exactly the URLFetcher objects
// will be created.
//
// These classes let you set pre-baked HTTP responses for particular URLs.
// E.g., if the user requests http://a.com/ then respond with an HTTP/500.
//
// We assume that the thread that is calling Start() on the URLFetcher object
// has a message loop running.
//
// This class is not thread-safe.  You should not call SetFakeResponse or
// ClearFakeResponse at the same time you call CreateURLFetcher.  However, it is
// OK to start URLFetcher objects while setting or clearning fake responses
// since already created URLFetcher objects will not be affected by any changes
// made to the fake responses (once a URLFetcher object is created you cannot
// change its fake response).
//
// Example usage:
//  FakeURLFetcherFactory factory;
//
//  // You know that class SomeService will request url http://a.com/ and you
//  // want to test the service class by returning an error.
//  factory.SetFakeResponse("http://a.com/", "", false);
//  // But if the service requests http://b.com/asdf you want to respond with
//  // a simple html page and an HTTP/200 code.
//  factory.SetFakeResponse("http://b.com/asdf",
//                          "<html><body>hello world</body></html>",
//                          true);
//
//  SomeService service;
//  service.Run();  // Will eventually request these two URLs.

class FakeURLFetcherFactory : public URLFetcherFactory,
                              public ScopedURLFetcherFactory {
 public:
  FakeURLFetcherFactory();
  // FakeURLFetcherFactory that will delegate creating URLFetcher for unknown
  // url to the given factory.
  explicit FakeURLFetcherFactory(URLFetcherFactory* default_factory);
  virtual ~FakeURLFetcherFactory();

  // If no fake response is set for the given URL this method will delegate the
  // call to |default_factory_| if it is not NULL, or return NULL if it is
  // NULL.
  // Otherwise, it will return a URLFetcher object which will respond with the
  // pre-baked response that the client has set by calling SetFakeResponse().
  virtual URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      URLFetcher::RequestType request_type,
      URLFetcherDelegate* d) override;

  // Sets the fake response for a given URL.  If success is true we will serve
  // an HTTP/200 and an HTTP/500 otherwise.  The |response_data| may be empty.
  void SetFakeResponse(const std::string& url,
                       const std::string& response_data,
                       bool success);

  // Clear all the fake responses that were previously set via
  // SetFakeResponse().
  void ClearFakeResponses();

 private:
  typedef std::map<GURL, std::pair<std::string, bool> > FakeResponseMap;
  FakeResponseMap fake_responses_;
  URLFetcherFactory* default_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeURLFetcherFactory);
};

// This is an implementation of URLFetcherFactory that will create a
// URLFetcherImpl. It can be use in conjunction with a FakeURLFetcherFactory in
// integration tests to control the behavior of some requests but execute
// all the other ones.
class URLFetcherImplFactory : public URLFetcherFactory {
 public:
  URLFetcherImplFactory();
  virtual ~URLFetcherImplFactory();

  // This method will create a real URLFetcher.
  virtual URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      URLFetcher::RequestType request_type,
      URLFetcherDelegate* d) override;

};

}  // namespace net

#endif  // NET_URL_REQUEST_TEST_URL_FETCHER_FACTORY_H_
