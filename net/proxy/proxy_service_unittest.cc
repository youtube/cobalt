// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_script_fetcher.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockProxyConfigService: public net::ProxyConfigService {
 public:
  MockProxyConfigService() {}  // Direct connect.
  explicit MockProxyConfigService(const net::ProxyConfig& pc) : config(pc) {}
  explicit MockProxyConfigService(const std::string& pac_url) {
    config.pac_url = GURL(pac_url);
  }

  virtual int GetProxyConfig(net::ProxyConfig* results) {
    *results = config;
    return net::OK;
  }

  net::ProxyConfig config;
};

// Asynchronous mock proxy resolver. All requests complete asynchronously,
// user must call Request::CompleteNow() on a pending request to signal it.
class MockAsyncProxyResolverBase : public net::ProxyResolver {
 public:
  class Request : public base::RefCounted<Request> {
   public:
    Request(MockAsyncProxyResolverBase* resolver,
            const GURL& url,
            net::ProxyInfo* results,
            net::CompletionCallback* callback)
        : resolver_(resolver),
          url_(url),
          results_(results),
          callback_(callback),
          origin_loop_(MessageLoop::current()) {
    }

    const GURL& url() const { return url_; }
    net::ProxyInfo* results() const { return results_; }
    net::CompletionCallback* callback() const { return callback_; }

    void CompleteNow(int rv) {
      net::CompletionCallback* callback = callback_;

      // May delete |this|.
      resolver_->RemovePendingRequest(this);

      callback->Run(rv);
    }

   private:
    MockAsyncProxyResolverBase* resolver_;
    const GURL url_;
    net::ProxyInfo* results_;
    net::CompletionCallback* callback_;
    MessageLoop* origin_loop_;
  };

  typedef std::vector<scoped_refptr<Request> > RequestsList;

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             net::ProxyInfo* results,
                             net::CompletionCallback* callback,
                             RequestHandle* request_handle) {
    scoped_refptr<Request> request = new Request(this, url, results, callback);
    pending_requests_.push_back(request);

    if (request_handle)
      *request_handle = reinterpret_cast<RequestHandle>(request.get());

    // Test code completes the request by calling request->CompleteNow().
    return net::ERR_IO_PENDING;
  }

  virtual void CancelRequest(RequestHandle request_handle) {
    scoped_refptr<Request> request = reinterpret_cast<Request*>(request_handle);
    cancelled_requests_.push_back(request);
    RemovePendingRequest(request);
  }

  virtual void SetPacScriptByUrlInternal(const GURL& pac_url) {
    pac_url_ = pac_url;
  }

  virtual void SetPacScriptByDataInternal(const std::string& bytes) {
    pac_bytes_ = bytes;
  }

  const RequestsList& pending_requests() const {
    return pending_requests_;
  }

  const RequestsList& cancelled_requests() const {
    return cancelled_requests_;
  }

  const GURL& pac_url() const {
    return pac_url_;
  }

  const std::string& pac_bytes() const {
    return pac_bytes_;
  }

  void RemovePendingRequest(Request* request) {
    RequestsList::iterator it = std::find(
        pending_requests_.begin(), pending_requests_.end(), request);
    DCHECK(it != pending_requests_.end());
    pending_requests_.erase(it);
  }

 protected:
  explicit MockAsyncProxyResolverBase(bool expects_pac_bytes)
      : net::ProxyResolver(expects_pac_bytes) {}

 private:
  RequestsList pending_requests_;
  RequestsList cancelled_requests_;
  GURL pac_url_;
  std::string pac_bytes_;
};

class MockAsyncProxyResolver : public MockAsyncProxyResolverBase {
 public:
  MockAsyncProxyResolver()
      : MockAsyncProxyResolverBase(false /*expects_pac_bytes*/) {}
};

class MockAsyncProxyResolverExpectsBytes : public MockAsyncProxyResolverBase {
 public:
  MockAsyncProxyResolverExpectsBytes()
      : MockAsyncProxyResolverBase(true /*expects_pac_bytes*/) {}
};

}  // namespace

// A mock ProxyScriptFetcher. No result will be returned to the fetch client
// until we call NotifyFetchCompletion() to set the results.
class MockProxyScriptFetcher : public net::ProxyScriptFetcher {
 public:
  MockProxyScriptFetcher()
      : pending_request_callback_(NULL), pending_request_bytes_(NULL) {
  }

  // net::ProxyScriptFetcher implementation.
  virtual void Fetch(const GURL& url,
                     std::string* bytes,
                     net::CompletionCallback* callback) {
    DCHECK(!has_pending_request());

    // Save the caller's information, and have them wait.
    pending_request_url_ = url;
    pending_request_callback_ = callback;
    pending_request_bytes_ = bytes;
  }

  void NotifyFetchCompletion(int result, const std::string& bytes) {
    DCHECK(has_pending_request());
    *pending_request_bytes_ = bytes;
    pending_request_callback_->Run(result);
  }

  virtual void Cancel() {}

  const GURL& pending_request_url() const {
    return pending_request_url_;
  }

  bool has_pending_request() const {
    return pending_request_callback_ != NULL;
  }

 private:
  GURL pending_request_url_;
  net::CompletionCallback* pending_request_callback_;
  std::string* pending_request_bytes_;
};

// Template specialization so MockProxyScriptFetcher does not have to be
// refcounted.
template<>
void RunnableMethodTraits<MockProxyScriptFetcher>::RetainCallee(
    MockProxyScriptFetcher* remover) {}
template<>
void RunnableMethodTraits<MockProxyScriptFetcher>::ReleaseCallee(
    MockProxyScriptFetcher* remover) {}

TEST(ProxyServiceTest, Direct) {
  MockAsyncProxyResolver* resolver = new MockAsyncProxyResolver;
  net::ProxyService service(new MockProxyConfigService, resolver);

  GURL url("http://www.google.com/");

  net::ProxyInfo info;
  TestCompletionCallback callback;
  int rv = service.ResolveProxy(url, &info, &callback, NULL);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(resolver->pending_requests().empty());

  EXPECT_TRUE(info.is_direct());
}

TEST(ProxyServiceTest, PAC) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver* resolver = new MockAsyncProxyResolver;

  net::ProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  net::ProxyInfo info;
  TestCompletionCallback callback;
  int rv = service.ResolveProxy(url, &info, &callback, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  EXPECT_EQ(GURL("http://foopy/proxy.pac"), resolver->pac_url());
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  // Set the result in proxy resolver.
  resolver->pending_requests()[0]->results()->UseNamedProxy("foopy");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  EXPECT_EQ(net::OK, callback.WaitForResult());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy:80", info.proxy_server().ToURI());
}

TEST(ProxyServiceTest, PAC_FailoverToDirect) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");
  MockAsyncProxyResolver* resolver = new MockAsyncProxyResolver;

  net::ProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  net::ProxyInfo info;
  TestCompletionCallback callback1;
  int rv = service.ResolveProxy(url, &info, &callback1, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  EXPECT_EQ(GURL("http://foopy/proxy.pac"), resolver->pac_url());
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  // Set the result in proxy resolver.
  resolver->pending_requests()[0]->results()->UseNamedProxy("foopy:8080");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  EXPECT_EQ(net::OK, callback1.WaitForResult());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy:8080", info.proxy_server().ToURI());

  // Now, imagine that connecting to foopy:8080 fails.
  TestCompletionCallback callback2;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback2, NULL);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(info.is_direct());
}

TEST(ProxyServiceTest, ProxyResolverFails) {
  // Test what happens when the ProxyResolver fails (this could represent
  // a failure to download the PAC script in the case of ProxyResolvers which
  // do the fetch internally.)

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver* resolver = new MockAsyncProxyResolver;

  net::ProxyService service(config_service, resolver);

  // Start first resolve request.
  GURL url("http://www.google.com/");
  net::ProxyInfo info;
  TestCompletionCallback callback1;
  int rv = service.ResolveProxy(url, &info, &callback1, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  EXPECT_EQ(GURL("http://foopy/proxy.pac"), resolver->pac_url());
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  // Fail the first resolve request in MockAsyncProxyResolver.
  resolver->pending_requests()[0]->CompleteNow(net::ERR_FAILED);

  EXPECT_EQ(net::ERR_FAILED, callback1.WaitForResult());

  // The second resolve request will automatically select direct connect,
  // because it has cached the configuration as being bad.
  TestCompletionCallback callback2;
  rv = service.ResolveProxy(url, &info, &callback2, NULL);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(info.is_direct());
  EXPECT_TRUE(resolver->pending_requests().empty());

  // But, if that fails, then we should give the proxy config another shot
  // since we have never tried it with this URL before.
  TestCompletionCallback callback3;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback3, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

    // Set the result in proxy resolver.
  resolver->pending_requests()[0]->results()->UseNamedProxy("foopy_valid:8080");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  EXPECT_EQ(net::OK, callback3.WaitForResult());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy_valid:8080", info.proxy_server().ToURI());
}

TEST(ProxyServiceTest, ProxyFallback) {
  // Test what happens when we specify multiple proxy servers and some of them
  // are bad.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver* resolver = new MockAsyncProxyResolver;

  net::ProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  net::ProxyInfo info;
  TestCompletionCallback callback1;
  int rv = service.ResolveProxy(url, &info, &callback1, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  EXPECT_EQ(GURL("http://foopy/proxy.pac"), resolver->pac_url());
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  // Set the result in proxy resolver.
  resolver->pending_requests()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // The first item is valid.
  EXPECT_EQ(net::OK, callback1.WaitForResult());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake an error on the proxy.
  TestCompletionCallback callback2;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback2, NULL);
  EXPECT_EQ(net::OK, rv);

  // The second proxy should be specified.
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  TestCompletionCallback callback3;
  rv = service.ResolveProxy(url, &info, &callback3, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  // Set the result in proxy resolver -- the second result is already known
  // to be bad.
  resolver->pending_requests()[0]->results()->UseNamedProxy(
      "foopy3:7070;foopy1:8080;foopy2:9090");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  EXPECT_EQ(net::OK, callback3.WaitForResult());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy3:7070", info.proxy_server().ToURI());

  // We fake another error. It should now try the third one.
  TestCompletionCallback callback4;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback4, NULL);
  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  // Fake another error, the last proxy is gone, the list should now be empty.
  TestCompletionCallback callback5;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback5, NULL);
  EXPECT_EQ(net::OK, rv);  // We try direct.
  EXPECT_TRUE(info.is_direct());

  // If it fails again, we don't have anything else to try.
  TestCompletionCallback callback6;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback6, NULL);
  EXPECT_EQ(net::ERR_FAILED, rv);

  // TODO(nsylvain): Test that the proxy can be retried after the delay.
}

TEST(ProxyServiceTest, ProxyFallback_NewSettings) {
  // Test proxy failover when new settings are available.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver* resolver = new MockAsyncProxyResolver;

  net::ProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  net::ProxyInfo info;
  TestCompletionCallback callback1;
  int rv = service.ResolveProxy(url, &info, &callback1, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  EXPECT_EQ(GURL("http://foopy/proxy.pac"), resolver->pac_url());
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  // Set the result in proxy resolver.
  resolver->pending_requests()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // The first item is valid.
  EXPECT_EQ(net::OK, callback1.WaitForResult());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake an error on the proxy, and also a new configuration on the proxy.
  config_service->config = net::ProxyConfig();
  config_service->config.pac_url = GURL("http://foopy-new/proxy.pac");

  TestCompletionCallback callback2;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback2, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  EXPECT_EQ(GURL("http://foopy-new/proxy.pac"), resolver->pac_url());
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  resolver->pending_requests()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // The first proxy is still there since the configuration changed.
  EXPECT_EQ(net::OK, callback2.WaitForResult());
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // We fake another error. It should now ignore the first one.
  TestCompletionCallback callback3;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback3, NULL);
  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  // We simulate a new configuration.
  config_service->config = net::ProxyConfig();
  config_service->config.pac_url = GURL("http://foopy-new2/proxy.pac");

  // We fake another error. It should go back to the first proxy.
  TestCompletionCallback callback4;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback4, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  EXPECT_EQ(GURL("http://foopy-new2/proxy.pac"), resolver->pac_url());
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  resolver->pending_requests()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  EXPECT_EQ(net::OK, callback4.WaitForResult());
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());
}

TEST(ProxyServiceTest, ProxyFallback_BadConfig) {
  // Test proxy failover when the configuration is bad.

  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver* resolver = new MockAsyncProxyResolver;

  net::ProxyService service(config_service, resolver);

  GURL url("http://www.google.com/");

  // Get the proxy information.
  net::ProxyInfo info;
  TestCompletionCallback callback1;
  int rv = service.ResolveProxy(url, &info, &callback1, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  EXPECT_EQ(GURL("http://foopy/proxy.pac"), resolver->pac_url());
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  resolver->pending_requests()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // The first item is valid.
  EXPECT_EQ(net::OK, callback1.WaitForResult());
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  // Fake a proxy error.
  TestCompletionCallback callback2;
  rv = service.ReconsiderProxyAfterError(url, &info, &callback2, NULL);
  EXPECT_EQ(net::OK, rv);

  // The first proxy is ignored, and the second one is selected.
  EXPECT_FALSE(info.is_direct());
  EXPECT_EQ("foopy2:9090", info.proxy_server().ToURI());

  // Fake a PAC failure.
  net::ProxyInfo info2;
  TestCompletionCallback callback3;
  rv = service.ResolveProxy(url, &info2, &callback3, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  resolver->pending_requests()[0]->CompleteNow(net::ERR_FAILED);

  // No proxy servers are returned. It's a direct connection.
  EXPECT_EQ(net::ERR_FAILED, callback3.WaitForResult());
  EXPECT_TRUE(info2.is_direct());

  // The PAC will now be fixed and will return a proxy server.
  // It should also clear the list of bad proxies.

  // Try to resolve, it will still return "direct" because we have no reason
  // to check the config since everything works.
  net::ProxyInfo info3;
  TestCompletionCallback callback4;
  rv = service.ResolveProxy(url, &info3, &callback4, NULL);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(info3.is_direct());

  // But if the direct connection fails, we check if the ProxyInfo tried to
  // resolve the proxy before, and if not (like in this case), we give the
  // PAC another try.
  TestCompletionCallback callback5;
  rv = service.ReconsiderProxyAfterError(url, &info3, &callback5, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(url, resolver->pending_requests()[0]->url());

  resolver->pending_requests()[0]->results()->UseNamedProxy(
      "foopy1:8080;foopy2:9090");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // The first proxy is still there since the list of bad proxies got cleared.
  EXPECT_EQ(net::OK, callback5.WaitForResult());
  EXPECT_FALSE(info3.is_direct());
  EXPECT_EQ("foopy1:8080", info3.proxy_server().ToURI());
}

TEST(ProxyServiceTest, ProxyBypassList) {
  // Test what happens when a proxy bypass list is specified.

  net::ProxyInfo info;
  net::ProxyConfig config;
  config.proxy_rules.ParseFromString("foopy1:8080;foopy2:9090");
  config.auto_detect = false;
  config.proxy_bypass_local_names = true;

  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver());
    GURL url("http://www.google.com/");
    // Get the proxy information.
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(info.is_direct());
  }

  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver());
    GURL test_url("local");
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(info.is_direct());
  }

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.org");
  config.proxy_bypass_local_names = true;
  {
    net::ProxyService service(new MockProxyConfigService(config),
                             new MockAsyncProxyResolver);
    GURL test_url("http://www.webkit.org");
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(info.is_direct());
  }

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.org");
  config.proxy_bypass.push_back("7*");
  config.proxy_bypass_local_names = true;
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("http://74.125.19.147");
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(info.is_direct());
  }

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.org");
  config.proxy_bypass_local_names = true;
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("http://www.msn.com");
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(info.is_direct());
  }

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.MSN.COM");
  config.proxy_bypass_local_names = true;
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("http://www.msnbc.msn.com");
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(info.is_direct());
  }

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.msn.com");
  config.proxy_bypass_local_names = true;
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("HTTP://WWW.MSNBC.MSN.COM");
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(info.is_direct());
  }
}

TEST(ProxyServiceTest, ProxyBypassListWithPorts) {
  // Test port specification in bypass list entries.
  net::ProxyInfo info;
  net::ProxyConfig config;
  config.proxy_rules.ParseFromString("foopy1:8080;foopy2:9090");
  config.auto_detect = false;
  config.proxy_bypass_local_names = false;

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.example.com:99");
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    {
      GURL test_url("http://www.example.com:99");
      TestCompletionCallback callback;
      int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
      EXPECT_EQ(net::OK, rv);
      EXPECT_TRUE(info.is_direct());
    }
    {
      GURL test_url("http://www.example.com:100");
      TestCompletionCallback callback;
      int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
      EXPECT_EQ(net::OK, rv);
      EXPECT_FALSE(info.is_direct());
    }
    {
      GURL test_url("http://www.example.com");
      TestCompletionCallback callback;
      int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
      EXPECT_EQ(net::OK, rv);
      EXPECT_FALSE(info.is_direct());
    }
  }

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.example.com:80");
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("http://www.example.com");
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(info.is_direct());
  }

  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("*.example.com");
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("http://www.example.com:99");
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(info.is_direct());
  }

  // IPv6 with port.
  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("[3ffe:2a00:100:7031::1]:99");
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    {
      GURL test_url("http://[3ffe:2a00:100:7031::1]:99/");
      TestCompletionCallback callback;
      int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
      EXPECT_EQ(net::OK, rv);
      EXPECT_TRUE(info.is_direct());
    }
    {
      GURL test_url("http://[3ffe:2a00:100:7031::1]/");
      TestCompletionCallback callback;
      int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
      EXPECT_EQ(net::OK, rv);
      EXPECT_FALSE(info.is_direct());
    }
  }

  // IPv6 without port. The bypass entry ought to work without the
  // brackets, but the bypass matching logic in ProxyService is
  // currently limited.
  config.proxy_bypass.clear();
  config.proxy_bypass.push_back("[3ffe:2a00:100:7031::1]");
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    {
      GURL test_url("http://[3ffe:2a00:100:7031::1]:99/");
      TestCompletionCallback callback;
      int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
      EXPECT_EQ(net::OK, rv);
      EXPECT_TRUE(info.is_direct());
    }
    {
      GURL test_url("http://[3ffe:2a00:100:7031::1]/");
      TestCompletionCallback callback;
      int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
      EXPECT_EQ(net::OK, rv);
      EXPECT_TRUE(info.is_direct());
    }
  }
}

TEST(ProxyServiceTest, PerProtocolProxyTests) {
  net::ProxyConfig config;
  config.proxy_rules.ParseFromString("http=foopy1:8080;https=foopy2:8080");
  config.auto_detect = false;
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("http://www.msn.com");
    net::ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());
  }
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("ftp://ftp.google.com");
    net::ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(info.is_direct());
    EXPECT_EQ("direct://", info.proxy_server().ToURI());
  }
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("https://webbranch.techcu.com");
    net::ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("foopy2:8080", info.proxy_server().ToURI());
  }
  {
    config.proxy_rules.ParseFromString("foopy1:8080");
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("www.microsoft.com");
    net::ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());
  }
}

// If only HTTP and a SOCKS proxy are specified, check if ftp/https queries
// fall back to the SOCKS proxy.
TEST(ProxyServiceTest, DefaultProxyFallbackToSOCKS) {
  net::ProxyConfig config;
  config.proxy_rules.ParseFromString("http=foopy1:8080;socks=foopy2:1080");
  config.auto_detect = false;
  EXPECT_EQ(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
            config.proxy_rules.type);

  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("http://www.msn.com");
    net::ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());
  }
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("ftp://ftp.google.com");
    net::ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("socks4://foopy2:1080", info.proxy_server().ToURI());
  }
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("https://webbranch.techcu.com");
    net::ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("socks4://foopy2:1080", info.proxy_server().ToURI());
  }
  {
    net::ProxyService service(new MockProxyConfigService(config),
                              new MockAsyncProxyResolver);
    GURL test_url("www.microsoft.com");
    net::ProxyInfo info;
    TestCompletionCallback callback;
    int rv = service.ResolveProxy(test_url, &info, &callback, NULL);
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(info.is_direct());
    EXPECT_EQ("socks4://foopy2:1080", info.proxy_server().ToURI());
  }
}

// Test cancellation of an in-progress request.
TEST(ProxyServiceTest, CancelInProgressRequest) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolver* resolver = new MockAsyncProxyResolver;

  net::ProxyService service(config_service, resolver);

  // Start 3 requests.

  net::ProxyInfo info1;
  TestCompletionCallback callback1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), &info1, &callback1, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(GURL("http://request1"), resolver->pending_requests()[0]->url());

  net::ProxyInfo info2;
  TestCompletionCallback callback2;
  net::ProxyService::PacRequest* request2;
  rv = service.ResolveProxy(
      GURL("http://request2"), &info2, &callback2, &request2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  ASSERT_EQ(2u, resolver->pending_requests().size());
  EXPECT_EQ(GURL("http://request2"), resolver->pending_requests()[1]->url());

  net::ProxyInfo info3;
  TestCompletionCallback callback3;
  rv = service.ResolveProxy(
      GURL("http://request3"), &info3, &callback3, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  ASSERT_EQ(3u, resolver->pending_requests().size());
  EXPECT_EQ(GURL("http://request3"), resolver->pending_requests()[2]->url());

  // Cancel the second request
  service.CancelPacRequest(request2);

  ASSERT_EQ(2u, resolver->pending_requests().size());
  EXPECT_EQ(GURL("http://request1"), resolver->pending_requests()[0]->url());
  EXPECT_EQ(GURL("http://request3"), resolver->pending_requests()[1]->url());

  // Complete the two un-cancelled requests.
  // We complete the last one first, just to mix it up a bit.
  resolver->pending_requests()[1]->results()->UseNamedProxy("request3:80");
  resolver->pending_requests()[1]->CompleteNow(net::OK);

  resolver->pending_requests()[0]->results()->UseNamedProxy("request1:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // Complete and verify that requests ran as expected.
  EXPECT_EQ(net::OK, callback1.WaitForResult());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  EXPECT_FALSE(callback2.have_result());  // Cancelled.
  ASSERT_EQ(1u, resolver->cancelled_requests().size());
  EXPECT_EQ(GURL("http://request2"), resolver->cancelled_requests()[0]->url());

  EXPECT_EQ(net::OK, callback3.WaitForResult());
  EXPECT_EQ("request3:80", info3.proxy_server().ToURI());
}

// Test the initial PAC download for ProxyResolverWithoutFetch.
TEST(ProxyServiceTest, InitialPACScriptDownload) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolverExpectsBytes* resolver =
      new MockAsyncProxyResolverExpectsBytes;

  net::ProxyService service(config_service, resolver);

  MockProxyScriptFetcher* fetcher = new MockProxyScriptFetcher;
  service.SetProxyScriptFetcher(fetcher);

  // Start 3 requests.

  net::ProxyInfo info1;
  TestCompletionCallback callback1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), &info1, &callback1, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // The first request should have triggered download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  net::ProxyInfo info2;
  TestCompletionCallback callback2;
  rv = service.ResolveProxy(
      GURL("http://request2"), &info2, &callback2, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  net::ProxyInfo info3;
  TestCompletionCallback callback3;
  rv = service.ResolveProxy(
      GURL("http://request3"), &info3, &callback3, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Nothing has been sent to the resolver yet.
  EXPECT_TRUE(resolver->pending_requests().empty());

  // At this point the ProxyService should be waiting for the
  // ProxyScriptFetcher to invoke its completion callback, notifying it of
  // PAC script download completion.
  fetcher->NotifyFetchCompletion(net::OK, "pac-v1");

  // Now that the PAC script is downloaded, everything should have been sent
  // over to the proxy resolver.
  EXPECT_EQ("pac-v1", resolver->pac_bytes());
  ASSERT_EQ(3u, resolver->pending_requests().size());
  EXPECT_EQ(GURL("http://request1"), resolver->pending_requests()[0]->url());
  EXPECT_EQ(GURL("http://request2"), resolver->pending_requests()[1]->url());
  EXPECT_EQ(GURL("http://request3"), resolver->pending_requests()[2]->url());

  // Complete all the requests (in some order).
  // Note that as we complete requests, they shift up in |pending_requests()|.

  resolver->pending_requests()[2]->results()->UseNamedProxy("request3:80");
  resolver->pending_requests()[2]->CompleteNow(net::OK);

  resolver->pending_requests()[0]->results()->UseNamedProxy("request1:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  resolver->pending_requests()[0]->results()->UseNamedProxy("request2:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  // Complete and verify that requests ran as expected.
  EXPECT_EQ(net::OK, callback1.WaitForResult());
  EXPECT_EQ("request1:80", info1.proxy_server().ToURI());

  EXPECT_EQ(net::OK, callback2.WaitForResult());
  EXPECT_EQ("request2:80", info2.proxy_server().ToURI());

  EXPECT_EQ(net::OK, callback3.WaitForResult());
  EXPECT_EQ("request3:80", info3.proxy_server().ToURI());
}

// Test cancellation of a request, while the PAC script is being fetched.
TEST(ProxyServiceTest, CancelWhilePACFetching) {
  MockProxyConfigService* config_service =
      new MockProxyConfigService("http://foopy/proxy.pac");

  MockAsyncProxyResolverExpectsBytes* resolver =
      new MockAsyncProxyResolverExpectsBytes;

  net::ProxyService service(config_service, resolver);

  MockProxyScriptFetcher* fetcher = new MockProxyScriptFetcher;
  service.SetProxyScriptFetcher(fetcher);

  // Start 3 requests.
  net::ProxyInfo info1;
  TestCompletionCallback callback1;
  net::ProxyService::PacRequest* request1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), &info1, &callback1, &request1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // The first request should have triggered download of PAC script.
  EXPECT_TRUE(fetcher->has_pending_request());
  EXPECT_EQ(GURL("http://foopy/proxy.pac"), fetcher->pending_request_url());

  net::ProxyInfo info2;
  TestCompletionCallback callback2;
  net::ProxyService::PacRequest* request2;
  rv = service.ResolveProxy(
      GURL("http://request2"), &info2, &callback2, &request2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  net::ProxyInfo info3;
  TestCompletionCallback callback3;
  rv = service.ResolveProxy(
      GURL("http://request3"), &info3, &callback3, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Nothing has been sent to the resolver yet.
  EXPECT_TRUE(resolver->pending_requests().empty());

  // Cancel the first 2 requests.
  service.CancelPacRequest(request1);
  service.CancelPacRequest(request2);

  // At this point the ProxyService should be waiting for the
  // ProxyScriptFetcher to invoke its completion callback, notifying it of
  // PAC script download completion.
  fetcher->NotifyFetchCompletion(net::OK, "pac-v1");

  // Now that the PAC script is downloaded, everything should have been sent
  // over to the proxy resolver.
  EXPECT_EQ("pac-v1", resolver->pac_bytes());
  ASSERT_EQ(1u, resolver->pending_requests().size());
  EXPECT_EQ(GURL("http://request3"), resolver->pending_requests()[0]->url());

  // Complete all the requests.
  resolver->pending_requests()[0]->results()->UseNamedProxy("request3:80");
  resolver->pending_requests()[0]->CompleteNow(net::OK);

  EXPECT_EQ(net::OK, callback3.WaitForResult());
  EXPECT_EQ("request3:80", info3.proxy_server().ToURI());

  EXPECT_TRUE(resolver->cancelled_requests().empty());

  EXPECT_FALSE(callback1.have_result());  // Cancelled.
  EXPECT_FALSE(callback2.have_result());  // Cancelled.
}

TEST(ProxyServiceTest, ResetProxyConfigService) {
  net::ProxyConfig config1;
  config1.proxy_rules.ParseFromString("foopy1:8080");
  config1.auto_detect = false;
  net::ProxyService service(new MockProxyConfigService(config1),
                            new MockAsyncProxyResolverExpectsBytes);

  net::ProxyInfo info;
  TestCompletionCallback callback1;
  int rv = service.ResolveProxy(
      GURL("http://request1"), &info, &callback1, NULL);
  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ("foopy1:8080", info.proxy_server().ToURI());

  net::ProxyConfig config2;
  config2.proxy_rules.ParseFromString("foopy2:8080");
  config2.auto_detect = false;
  service.ResetConfigService(new MockProxyConfigService(config2));
  TestCompletionCallback callback2;
  rv = service.ResolveProxy(GURL("http://request2"), &info, &callback2, NULL);
  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ("foopy2:8080", info.proxy_server().ToURI());
}
