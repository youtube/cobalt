// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_TRANSACTION_H_
#define NET_DNS_DNS_TRANSACTION_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/rand_callback.h"

namespace net {

class DatagramClientSocket;
class DnsQuery;
class DnsResponse;
class DnsSession;

// Performs a single asynchronous DNS transaction over UDP,
// which consists of sending out a DNS query, waiting for a response, and
// returning the response that it matches.
class NET_EXPORT_PRIVATE DnsTransaction :
    NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  typedef base::Callback<void(DnsTransaction*, int)> ResultCallback;

  // Create new transaction using the parameters and state in |session|.
  // Issues query for name |qname| (in DNS format) type |qtype| and class IN.
  // Calls |callback| on completion or timeout.
  // TODO(szym): change dependency to (IPEndPoint, Socket, DnsQuery, callback)
  DnsTransaction(DnsSession* session,
                 const base::StringPiece& qname,
                 uint16 qtype,
                 const ResultCallback& callback,
                 const BoundNetLog& source_net_log);
  ~DnsTransaction();

  const DnsQuery* query() const { return query_.get(); }

  const DnsResponse* response() const { return response_.get(); }

  // Starts the resolution process.  Will return ERR_IO_PENDING and will
  // notify the caller via |delegate|.  Should only be called once.
  int Start();

 private:
  enum State {
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
    STATE_SEND_QUERY,
    STATE_SEND_QUERY_COMPLETE,
    STATE_READ_RESPONSE,
    STATE_READ_RESPONSE_COMPLETE,
    STATE_NONE,
  };

  int DoLoop(int result);
  void DoCallback(int result);
  void OnIOComplete(int result);

  int DoConnect();
  int DoConnectComplete(int result);
  int DoSendQuery();
  int DoSendQueryComplete(int result);
  int DoReadResponse();
  int DoReadResponseComplete(int result);

  // Fixed number of attempts are made to send a query and read a response,
  // and at the start of each, a timer is started with increasing delays.
  void StartTimer(base::TimeDelta delay);
  void RevokeTimer();
  void OnTimeout();

  scoped_refptr<DnsSession> session_;
  IPEndPoint dns_server_;
  scoped_ptr<DnsQuery> query_;
  ResultCallback callback_;
  scoped_ptr<DnsResponse> response_;
  scoped_ptr<DatagramClientSocket> socket_;

  // Number of retry attempts so far.
  int attempts_;

  State next_state_;
  base::OneShotTimer<DnsTransaction> timer_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(DnsTransaction);
};

}  // namespace net

#endif  // NET_DNS_DNS_TRANSACTION_H_
