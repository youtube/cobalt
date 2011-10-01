// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_TRANSACTION_H_
#define NET_DNS_DNS_TRANSACTION_H_
#pragma once

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/rand_callback.h"

namespace net {

class ClientSocketFactory;
class DatagramClientSocket;
class DnsQuery;
class DnsResponse;

// Performs (with fixed retries) a single asynchronous DNS transaction,
// which consists of sending out a DNS query, waiting for response, and
// parsing and returning the IP addresses that it matches.
class NET_EXPORT_PRIVATE DnsTransaction :
    NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  typedef std::pair<std::string, uint16> Key;

  // Interface that should implemented by DnsTransaction consumer and
  // passed to |Start| method to be notified when the transaction has
  // completed.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // A consumer of DnsTransaction should override |OnTransactionComplete|
    // and call |set_delegate(this)|.  The method will be called once the
    // resolution has completed, results passed in as arguments.
    virtual void OnTransactionComplete(
        int result,
        const DnsTransaction* transaction,
        const IPAddressList& ip_addresses);

   private:
    friend class DnsTransaction;

    void Attach(DnsTransaction* transaction);
    void Detach(DnsTransaction* transaction);

    std::set<DnsTransaction*> registered_transactions_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // |dns_server| is the address of the DNS server, |dns_name| is the
  // hostname (in DNS format) to be resolved, |query_type| is the type of
  // the query, either kDNS_A or kDNS_AAAA, |rand_int| is the PRNG used for
  // generating DNS query.
  DnsTransaction(const IPEndPoint& dns_server,
                 const std::string& dns_name,
                 uint16 query_type,
                 const RandIntCallback& rand_int,
                 ClientSocketFactory* socket_factory,
                 const BoundNetLog& source_net_log,
                 NetLog* net_log);
  ~DnsTransaction();
  void SetDelegate(Delegate* delegate);
  const Key& key() const { return key_; }

  // Starts the resolution process.  Will return ERR_IO_PENDING and will
  // notify the caller via |delegate|.  Should only be called once.
  int Start();

 private:
  FRIEND_TEST_ALL_PREFIXES(DnsTransactionTest, FirstTimeoutTest);
  FRIEND_TEST_ALL_PREFIXES(DnsTransactionTest, SecondTimeoutTest);
  FRIEND_TEST_ALL_PREFIXES(DnsTransactionTest, ThirdTimeoutTest);

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

  // This is to be used by unit tests only.
  void set_timeouts_ms(const std::vector<base::TimeDelta>& timeouts_ms);

  const IPEndPoint dns_server_;
  Key key_;
  IPAddressList ip_addresses_;
  Delegate* delegate_;

  scoped_ptr<DnsQuery> query_;
  scoped_ptr<DnsResponse> response_;
  scoped_ptr<DatagramClientSocket> socket_;

  // Number of retry attempts so far.
  size_t attempts_;

  // Timeouts in milliseconds.
  std::vector<base::TimeDelta> timeouts_ms_;

  State next_state_;
  ClientSocketFactory* socket_factory_;
  base::OneShotTimer<DnsTransaction> timer_;
  OldCompletionCallbackImpl<DnsTransaction> io_callback_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(DnsTransaction);
};

}  // namespace net

#endif  // NET_DNS_DNS_TRANSACTION_H_
