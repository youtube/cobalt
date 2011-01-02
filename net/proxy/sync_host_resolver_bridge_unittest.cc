// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/sync_host_resolver_bridge.h"

#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/proxy/multi_threaded_proxy_resolver.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy/proxy_info.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(eroman): This test should be moved into
//               multi_threaded_proxy_resolver_unittest.cc.

namespace net {

namespace {

// This implementation of HostResolver allows blocking until a resolve request
// has been received. The resolve requests it receives will never be completed.
class BlockableHostResolver : public HostResolver {
 public:
  BlockableHostResolver()
      : event_(true, false),
        was_request_cancelled_(false) {
  }

  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) {
    EXPECT_TRUE(callback);
    EXPECT_TRUE(out_req);
    *out_req = reinterpret_cast<RequestHandle*>(1);  // Magic value.

    // Indicate to the caller that a request was received.
    event_.Signal();

    // We return ERR_IO_PENDING, as this request will NEVER be completed.
    // Expectation is for the caller to later cancel the request.
    return ERR_IO_PENDING;
  }

  virtual void CancelRequest(RequestHandle req) {
    EXPECT_EQ(reinterpret_cast<RequestHandle*>(1), req);
    was_request_cancelled_ = true;
  }

  virtual void AddObserver(Observer* observer) {
    NOTREACHED();
  }

  virtual void RemoveObserver(Observer* observer) {
    NOTREACHED();
  }

  // Waits until Resolve() has been called.
  void WaitUntilRequestIsReceived() {
    event_.Wait();
  }

  bool was_request_cancelled() const {
    return was_request_cancelled_;
  }

 private:
  // Event to notify when a resolve request was received.
  base::WaitableEvent event_;
  bool was_request_cancelled_;
};

// This implementation of ProxyResolver simply does a synchronous resolve
// on |host_resolver| in response to GetProxyForURL().
class SyncProxyResolver : public ProxyResolver {
 public:
  explicit SyncProxyResolver(SyncHostResolverBridge* host_resolver)
      : ProxyResolver(false), host_resolver_(host_resolver) {}

  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) {
    EXPECT_FALSE(callback);
    EXPECT_FALSE(request);

    // Do a synchronous host resolve.
    HostResolver::RequestInfo info(HostPortPair::FromURL(url));
    AddressList addresses;
    int rv =
        host_resolver_->Resolve(info, &addresses, NULL, NULL, BoundNetLog());

    EXPECT_EQ(ERR_ABORTED, rv);

    return rv;
  }

  virtual void CancelRequest(RequestHandle request) {
    NOTREACHED();
  }

  virtual void Shutdown() {
    host_resolver_->Shutdown();
  }

  virtual void CancelSetPacScript() {
    NOTREACHED();
  }

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      CompletionCallback* callback) {
    return OK;
  }

 private:
  SyncHostResolverBridge* const host_resolver_;
};

class SyncProxyResolverFactory : public ProxyResolverFactory {
 public:
  // Takes ownership of |sync_host_resolver|.
  explicit SyncProxyResolverFactory(SyncHostResolverBridge* sync_host_resolver)
      : ProxyResolverFactory(false),
        sync_host_resolver_(sync_host_resolver) {
  }

  virtual ProxyResolver* CreateProxyResolver() {
    return new SyncProxyResolver(sync_host_resolver_.get());
  }

 private:
  const scoped_ptr<SyncHostResolverBridge> sync_host_resolver_;
};

// This helper thread is used to create the circumstances for the deadlock.
// It is analagous to the "IO thread" which would be main thread running the
// network stack.
class IOThread : public base::Thread {
 public:
  IOThread() : base::Thread("IO-thread") {}

  virtual ~IOThread() {
    Stop();
  }

  BlockableHostResolver* async_resolver() {
    return async_resolver_.get();
  }

 protected:
  virtual void Init() {
    async_resolver_.reset(new BlockableHostResolver());

    // Create a synchronous host resolver that operates the async host
    // resolver on THIS thread.
    SyncHostResolverBridge* sync_resolver =
        new SyncHostResolverBridge(async_resolver_.get(), message_loop());

    proxy_resolver_.reset(
        new MultiThreadedProxyResolver(
            new SyncProxyResolverFactory(sync_resolver),
            1u));

    // Initialize the resolver.
    TestCompletionCallback callback;
    proxy_resolver_->SetPacScript(ProxyResolverScriptData::FromURL(GURL()),
                                  &callback);
    EXPECT_EQ(OK, callback.WaitForResult());

    // Start an asynchronous request to the proxy resolver
    // (note that it will never complete).
    proxy_resolver_->GetProxyForURL(GURL("http://test/"), &results_,
                                    &callback_, &request_, BoundNetLog());
  }

  virtual void CleanUp() {
    // Cancel the outstanding request (note however that this will not
    // unblock the PAC thread though).
    proxy_resolver_->CancelRequest(request_);

    // Delete the single threaded proxy resolver.
    proxy_resolver_.reset();

    // (There may have been a completion posted back to origin thread, avoid
    // leaking it by running).
    MessageLoop::current()->RunAllPending();

    // During the teardown sequence of the single threaded proxy resolver,
    // the outstanding host resolve should have been cancelled.
    EXPECT_TRUE(async_resolver_->was_request_cancelled());
  }

 private:
  // This (async) host resolver will outlive the thread that is operating it
  // synchronously.
  scoped_ptr<BlockableHostResolver> async_resolver_;

  scoped_ptr<ProxyResolver> proxy_resolver_;

  // Data for the outstanding request to the single threaded proxy resolver.
  TestCompletionCallback callback_;
  ProxyInfo results_;
  ProxyResolver::RequestHandle request_;
};

// Test that a deadlock does not happen during shutdown when a host resolve
// is outstanding on the SyncHostResolverBridge.
// This is a regression test for http://crbug.com/41244.
TEST(MultiThreadedProxyResolverTest, ShutdownIsCalledBeforeThreadJoin) {
  IOThread io_thread;
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  ASSERT_TRUE(io_thread.StartWithOptions(options));

  io_thread.async_resolver()->WaitUntilRequestIsReceived();

  // Now upon exitting this scope, the IOThread is destroyed -- this will
  // stop the IOThread, which will in turn delete the
  // SingleThreadedProxyResolver, which in turn will stop its internal
  // PAC thread (which is currently blocked waiting on the host resolve which
  // is running on IOThread).  The IOThread::Cleanup() will verify that after
  // the PAC thread is stopped, it cancels the request on the HostResolver.
}

}  // namespace

}  // namespace net
