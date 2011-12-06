// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"

#include "base/bind.h"
#include "base/string_piece.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_transaction.h"
#include "net/socket/client_socket_factory.h"

namespace net {

DnsClient::Request::Request(const base::StringPiece& qname,
                            uint16 qtype,
                            const RequestCallback& callback)
    : qname_(qname.data(), qname.size()),
      qtype_(qtype),
      callback_(callback) {
}

DnsClient::Request::~Request() {}

// Implementation of DnsClient that uses DnsTransaction to serve requests.
class DnsClientImpl : public DnsClient {
 public:
  class RequestImpl : public Request {
   public:
    RequestImpl(const base::StringPiece& qname,
                uint16 qtype,
                const RequestCallback& callback,
                DnsSession* session,
                const BoundNetLog& net_log)
        : Request(qname, qtype, callback),
          session_(session),
          net_log_(net_log) {
    }

    virtual int Start() OVERRIDE {
      transaction_.reset(new DnsTransaction(
          session_,
          qname(),
          qtype(),
          base::Bind(&RequestImpl::OnComplete, base::Unretained(this)),
          net_log_));
      return transaction_->Start();
    }

    void OnComplete(DnsTransaction* transaction, int rv) {
      DCHECK_EQ(transaction_.get(), transaction);
      // TODO(szym):
      // - handle retransmissions here instead of DnsTransaction
      // - handle rcode and flags here instead of DnsTransaction
      // - update RTT in DnsSession
      // - perform suffix search
      // - handle DNS over TCP
      DoCallback(rv, (rv == OK) ? transaction->response() : NULL);
    }

   private:
    scoped_refptr<DnsSession> session_;
    BoundNetLog net_log_;
    scoped_ptr<DnsTransaction> transaction_;
  };

  explicit DnsClientImpl(DnsSession* session) {
    session_ = session;
  }

  virtual Request* CreateRequest(
      const base::StringPiece& qname,
      uint16 qtype,
      const RequestCallback& callback,
      const BoundNetLog& source_net_log) OVERRIDE {
    return new RequestImpl(qname, qtype, callback, session_, source_net_log);
  }

 private:
  scoped_refptr<DnsSession> session_;
};

// static
DnsClient* DnsClient::CreateClient(DnsSession* session) {
  return new DnsClientImpl(session);
}

}  // namespace net

