// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MOCK_PROXY_RESOLVER_H_
#define NET_PROXY_MOCK_PROXY_RESOLVER_H_

#include <vector>

#include "base/logging.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver.h"

namespace net {

// Asynchronous mock proxy resolver. All requests complete asynchronously,
// user must call Request::CompleteNow() on a pending request to signal it.
class MockAsyncProxyResolverBase : public ProxyResolver {
 public:
  class Request : public base::RefCounted<Request> {
   public:
    Request(MockAsyncProxyResolverBase* resolver,
            const GURL& url,
            ProxyInfo* results,
            CompletionCallback* callback)
        : resolver_(resolver),
          url_(url),
          results_(results),
          callback_(callback),
          origin_loop_(MessageLoop::current()) {
    }

    const GURL& url() const { return url_; }
    ProxyInfo* results() const { return results_; }
    CompletionCallback* callback() const { return callback_; }

    void CompleteNow(int rv) {
      CompletionCallback* callback = callback_;

      // May delete |this|.
      resolver_->RemovePendingRequest(this);

      callback->Run(rv);
    }

   private:
    friend class base::RefCounted<Request>;

    ~Request() {}

    MockAsyncProxyResolverBase* resolver_;
    const GURL url_;
    ProxyInfo* results_;
    CompletionCallback* callback_;
    MessageLoop* origin_loop_;
  };

  class SetPacScriptRequest {
   public:
    SetPacScriptRequest(
        MockAsyncProxyResolverBase* resolver,
        const scoped_refptr<ProxyResolverScriptData>& script_data,
        CompletionCallback* callback)
        : resolver_(resolver),
          script_data_(script_data),
          callback_(callback),
          origin_loop_(MessageLoop::current()) {
    }

    const ProxyResolverScriptData* script_data() const { return script_data_; }

    void CompleteNow(int rv) {
       CompletionCallback* callback = callback_;

      // Will delete |this|.
      resolver_->RemovePendingSetPacScriptRequest(this);

      callback->Run(rv);
    }

   private:
    MockAsyncProxyResolverBase* resolver_;
    const scoped_refptr<ProxyResolverScriptData> script_data_;
    CompletionCallback* callback_;
    MessageLoop* origin_loop_;
  };

  typedef std::vector<scoped_refptr<Request> > RequestsList;

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request_handle,
                             const BoundNetLog& /*net_log*/) {
    scoped_refptr<Request> request = new Request(this, url, results, callback);
    pending_requests_.push_back(request);

    if (request_handle)
      *request_handle = reinterpret_cast<RequestHandle>(request.get());

    // Test code completes the request by calling request->CompleteNow().
    return ERR_IO_PENDING;
  }

  virtual void CancelRequest(RequestHandle request_handle) {
    scoped_refptr<Request> request = reinterpret_cast<Request*>(request_handle);
    cancelled_requests_.push_back(request);
    RemovePendingRequest(request);
  }

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      CompletionCallback* callback) {
    DCHECK(!pending_set_pac_script_request_.get());
    pending_set_pac_script_request_.reset(
        new SetPacScriptRequest(this, script_data, callback));
    // Finished when user calls SetPacScriptRequest::CompleteNow().
    return ERR_IO_PENDING;
  }

  virtual void CancelSetPacScript() {
    // Do nothing (caller was responsible for completing the request).
  }

  const RequestsList& pending_requests() const {
    return pending_requests_;
  }

  const RequestsList& cancelled_requests() const {
    return cancelled_requests_;
  }

  SetPacScriptRequest* pending_set_pac_script_request() const {
    return pending_set_pac_script_request_.get();
  }

  void RemovePendingRequest(Request* request) {
    RequestsList::iterator it = std::find(
        pending_requests_.begin(), pending_requests_.end(), request);
    DCHECK(it != pending_requests_.end());
    pending_requests_.erase(it);
  }

  void RemovePendingSetPacScriptRequest(SetPacScriptRequest* request) {
    DCHECK_EQ(request, pending_set_pac_script_request());
    pending_set_pac_script_request_.reset();
  }

 protected:
  explicit MockAsyncProxyResolverBase(bool expects_pac_bytes)
      : ProxyResolver(expects_pac_bytes) {}

 private:
  RequestsList pending_requests_;
  RequestsList cancelled_requests_;
  scoped_ptr<SetPacScriptRequest> pending_set_pac_script_request_;
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

}  // namespace net

#endif  // NET_PROXY_MOCK_PROXY_RESOLVER_H_
