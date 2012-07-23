// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "net/base/net_log.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_transaction.h"
#include "net/socket/client_socket_factory.h"

namespace net {

namespace {

class DnsClientImpl : public DnsClient {
 public:
  explicit DnsClientImpl(NetLog* net_log) : net_log_(net_log) {}

  virtual void SetConfig(const DnsConfig& config) OVERRIDE {
    factory_.reset();
    session_.release();
    if (config.IsValid()) {
      session_ = new DnsSession(config,
                                ClientSocketFactory::GetDefaultFactory(),
                                base::Bind(&base::RandInt),
                                net_log_);
      factory_ = DnsTransactionFactory::CreateFactory(session_);
    }
  }

  virtual const DnsConfig* GetConfig() const OVERRIDE {
    return session_.get() ? &session_->config() : NULL;
  }

  virtual DnsTransactionFactory* GetTransactionFactory() OVERRIDE {
    return session_.get() ? factory_.get() : NULL;
  }

 private:
  scoped_refptr<DnsSession> session_;
  scoped_ptr<DnsTransactionFactory> factory_;

  NetLog* net_log_;
};

}  // namespace

// static
scoped_ptr<DnsClient> DnsClient::CreateClient(NetLog* net_log) {
  return scoped_ptr<DnsClient>(new DnsClientImpl(net_log));
}

}  // namespace net

