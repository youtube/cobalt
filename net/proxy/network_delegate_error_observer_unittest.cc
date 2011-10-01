// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/network_delegate_error_observer.h"

#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

DISABLE_RUNNABLE_METHOD_REFCOUNT(net::NetworkDelegateErrorObserver);

namespace net {

namespace {

class TestNetworkDelegate : public net::NetworkDelegate {
 public:
  TestNetworkDelegate() : got_pac_error_(false) {}
  virtual ~TestNetworkDelegate() {}

  bool got_pac_error() const { return got_pac_error_; }

 private:
  // net::NetworkDelegate:
  virtual int OnBeforeURLRequest(URLRequest* request,
                                 OldCompletionCallback* callback,
                                 GURL* new_url) OVERRIDE {
    return net::OK;
  }
  virtual int OnBeforeSendHeaders(URLRequest* request,
                                  OldCompletionCallback* callback,
                                  HttpRequestHeaders* headers) OVERRIDE {
    return net::OK;
  }
  virtual void OnSendHeaders(URLRequest* request,
                             const HttpRequestHeaders& headers) OVERRIDE {}
  virtual void OnBeforeRedirect(URLRequest* request,
                                const GURL& new_location) OVERRIDE {}
  virtual void OnResponseStarted(URLRequest* request) OVERRIDE {}
  virtual void OnRawBytesRead(const URLRequest& request,
                              int bytes_read) OVERRIDE {}
  virtual void OnCompleted(URLRequest* request) OVERRIDE {}
  virtual void OnURLRequestDestroyed(URLRequest* request) OVERRIDE {}

  virtual void OnPACScriptError(int line_number,
                                const string16& error) OVERRIDE {
    got_pac_error_ = true;
  }
  virtual void OnAuthRequired(URLRequest* request,
                              const AuthChallengeInfo& auth_info) OVERRIDE {}

  bool got_pac_error_;
};

}  // namespace

// Check that the OnPACScriptError method can be called from an arbitrary
// thread.
TEST(NetworkDelegateErrorObserverTest, CallOnThread) {
  base::Thread thread("test_thread");
  thread.Start();
  TestNetworkDelegate network_delegate;
  NetworkDelegateErrorObserver
      observer(&network_delegate,
               base::MessageLoopProxy::current());
  thread.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(&observer,
                        &NetworkDelegateErrorObserver::OnPACScriptError,
                        42, string16()));
  thread.Stop();
  MessageLoop::current()->RunAllPending();
  ASSERT_TRUE(network_delegate.got_pac_error());
}

// Check that passing a NULL network delegate works.
TEST(NetworkDelegateErrorObserverTest, NoDelegate) {
  base::Thread thread("test_thread");
  thread.Start();
  NetworkDelegateErrorObserver
      observer(NULL, base::MessageLoopProxy::current());
  thread.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(&observer,
                        &NetworkDelegateErrorObserver::OnPACScriptError,
                        42, string16()));
  thread.Stop();
  MessageLoop::current()->RunAllPending();
  // Shouldn't have crashed until here...
}

}  // namespace net
