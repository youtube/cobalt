// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/shell/http_transaction_factory_shell.h"

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/http/shell/http_stream_shell.h"
#include "net/http/shell/http_stream_shell_loader.h"
#include "net/http/shell/http_transaction_shell.h"

namespace net {

//-----------------------------------------------------------------------------
HttpTransactionFactoryShell::HttpTransactionFactoryShell(
    const net::HttpNetworkSession::Params& params)
        : params_(params) {
}

HttpTransactionFactoryShell::~HttpTransactionFactoryShell() {
}

//-----------------------------------------------------------------------------

// static
HttpTransactionFactory* HttpTransactionFactoryShell::CreateFactory(
    const net::HttpNetworkSession::Params& params) {
  return new HttpTransactionFactoryShell(params);
}

//-----------------------------------------------------------------------------

int HttpTransactionFactoryShell::CreateTransaction(
    scoped_ptr<HttpTransaction>* trans, HttpTransactionDelegate* delegate) {
  // Create an Http stream loader object
  HttpStreamShellLoader* loader = CreateHttpStreamShellLoader();
  DCHECK(loader);
  // Create an Http stream object
  HttpStreamShell* stream = new HttpStreamShell(loader);
  return CreateTransaction(trans, delegate, stream);
}

int HttpTransactionFactoryShell::CreateTransaction(
    scoped_ptr<HttpTransaction>* trans,
    HttpTransactionDelegate* delegate,
    HttpStreamShell* stream) {
  // Create a Http transaction object
  trans->reset(new HttpTransactionShell(&params_, stream));
  return OK;
}

HttpCache* HttpTransactionFactoryShell::GetCache() {
  return NULL;
}

HttpNetworkSession* HttpTransactionFactoryShell::GetSession() {
  return NULL;
}

}  // namespace net
