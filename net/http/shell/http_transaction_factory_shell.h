// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_FACTORY_SHELL_H_
#define NET_HTTP_HTTP_TRANSACTION_FACTORY_SHELL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/system_monitor/system_monitor.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"
#include "net/http/http_network_session.h"  // net::HttpNetworkSession::Params
#include "net/http/http_transaction_factory.h"

namespace net {

class HttpNetworkSession;
class HttpStreamShell;

class NET_EXPORT HttpTransactionFactoryShell
    : public HttpTransactionFactory,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
  friend class HttpTransactionShellTest;  // For unittests.
 public:
  explicit HttpTransactionFactoryShell(
      const net::HttpNetworkSession::Params& params);
  virtual ~HttpTransactionFactoryShell();

  // Create a transaction factory.
  static HttpTransactionFactory* CreateFactory(
      const net::HttpNetworkSession::Params& params);

  // HttpTransactionFactory methods:
  virtual int CreateTransaction(scoped_ptr<HttpTransaction>* trans,
                                HttpTransactionDelegate* delegate) override;

  virtual HttpCache* GetCache() override;
  virtual HttpNetworkSession* GetSession() override;

 protected:
  // This function is used in unit test to use a MockStream object instead
  // of normal stream object.
  int CreateTransaction(scoped_ptr<HttpTransaction>* trans,
                        HttpTransactionDelegate* delegate,
                        HttpStreamShell* stream);

  const net::HttpNetworkSession::Params params_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_FACTORY_SHELL_H_
