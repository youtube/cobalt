// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_TEST_URL_FETCHER_FACTORY_H_
#define NET_URL_REQUEST_TEST_URL_FETCHER_FACTORY_H_
#pragma once

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

class TestURLFetcher : public URLFetcher {
 public:
  TestURLFetcher(int id,
                 const GURL& url,
                 URLFetcherDelegate* d);
  virtual ~TestURLFetcher();

  // URLFetcher implementation
  virtual void SetUploadData(const std::string& upload_content_type,
                             const std::string& upload_content) OVERRIDE;
  virtual void SetChunkedUpload(
      const std::string& upload_content_type) OVERRIDE;
  // Overriden to cache the chunks uploaded. Caller can read back the uploaded
  // chunks with the upload_chunks() accessor.
  virtual void AppendChunkToUpload(const std::string& data,
                                   bool is_last_chunk) OVERRIDE;
  virtual void SetLoadFlags(int load_flags) OVERRIDE;
  virtual int GetLoadFlags() const OVERRIDE;
  virtual void SetReferrer(const std::string& referrer) OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const std::string& extra_request_headers) OVERRIDE;
  virtual void AddExtraRequestHeader(const std::string& header_line) OVERRIDE;
  virtual void GetExtraRequestHeaders(
      HttpRequestHeaders* headers) const OVERRIDE;
  virtual void SetRequestContext(
      URLRequestContextGetter* request_context_getter) OVERRIDE;
  virtual void SetFirstPartyForCookies(
      const GURL& first_party_for_cookies) OVERRIDE;
  virtual void SetURLRequestUserData(
      const void* key,
      const CreateDataCallback& create_data_callback) OVERRIDE;
  virtual void SetStopOnRedirect(bool stop_on_redirect) OVERRIDE;
  virtual void SetAutomaticallyRetryOn5xx(bool retry) OVERRIDE;
  virtual void SetMaxRetries(int max_retries) OVERRIDE;
  virtual int GetMaxRetries() const OVERRIDE;
  virtual base::TimeDelta GetBackoffDelay() const OVERRIDE;
  virtual void SaveResponseToFileAtPath(
      const FilePath& file_path,
      scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy) OVERRIDE;
  virtual void SaveResponseToTemporaryFile(
      scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy) OVERRIDE;
  virtual HttpResponseHeaders* GetResponseHeaders() const OVERRIDE;
  virtual HostPortPair GetSocketAddress() const OVERRIDE;
  virtual bool WasFetchedViaProxy() const OVERRIDE;
  virtual void Start() OVERRIDE;

  // URL we were created with. Because of how we're using URLFetcher GetURL()
  // always returns an empty URL. Chances are you'll want to use
  // GetOriginalURL() in your tests.
  virtual const GURL& GetOriginalURL() const OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual const URLRequestStatus& GetStatus() const OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual const ResponseCookies& GetCookies() const OVERRIDE;
  virtual bool FileErrorOccurred(
      base::PlatformFileError* out_error_code) const OVERRIDE;
  virtual void ReceivedContentWasMalformed() OVERRIDE;
  // Override response access functions to return fake data.
  virtual bool GetResponseAsString(
      std::string* out_response_string) const OVERRIDE;
  virtual bool GetResponseAsFilePath(
      bool take_ownership, FilePath* out_response_path) const OVERRIDE;

  // Unique ID in our factory.
  int id() const { return id_; }

  // Returns the data uploaded on this URLFetcher.
  const std::string& upload_data() const { return upload_data_; }

  // Returns the chunks of data uploaded on this URLFetcher.
  const std::list<std::string>& upload_chunks() const { return chunks_; }

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

  // Set string data.
  void SetResponseString(const std::string& response);

  // Set File data.
  void SetResponseFilePath(const FilePath& path);

 private:
  enum ResponseDestinationType {
    STRING,  // Default: In a std::string
    TEMP_FILE  // Write to a temp file
  };

  const int id_;
  const GURL original_url_;
  URLFetcherDelegate* delegate_;
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

// Simple URLFetcherFactory method that creates TestURLFetchers. All fetchers
// are registered in a map by the id passed to the create method.
class TestURLFetcherFactory : public URLFetcherFactory,
                              public ScopedURLFetcherFactory {
 public:
  TestURLFetcherFactory();
  virtual ~TestURLFetcherFactory();

  virtual URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      URLFetcher::RequestType request_type,
      URLFetcherDelegate* d) OVERRIDE;
  TestURLFetcher* GetFetcherByID(int id) const;
  void RemoveFetcherFromMap(int id);

 private:
  // Maps from id passed to create to the returned URLFetcher.
  typedef std::map<int, TestURLFetcher*> Fetchers;
  Fetchers fetchers_;

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
      URLFetcherDelegate* d) OVERRIDE;

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
      URLFetcherDelegate* d) OVERRIDE;

};

}  // namespace net

#endif  // NET_URL_REQUEST_TEST_URL_FETCHER_FACTORY_H_
