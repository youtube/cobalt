// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include <deque>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "base/values.h"
#include "net/base/completion_callback.h"
#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

namespace {

// Provide a common macro to simplify code and readability. We must use a
// macro as the underlying HISTOGRAM macro creates static variables.
#define DNS_HISTOGRAM(name, time) UMA_HISTOGRAM_CUSTOM_TIMES(name, time, \
    base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1), 100)

// Count labels in the fully-qualified name in DNS format.
int CountLabels(const std::string& name) {
  size_t count = 0;
  for (size_t i = 0; i < name.size() && name[i]; i += name[i] + 1)
    ++count;
  return count;
}

bool IsIPLiteral(const std::string& hostname) {
  IPAddressNumber ip;
  return ParseIPLiteralToNumber(hostname, &ip);
}

Value* NetLogStartCallback(const std::string* hostname,
                           uint16 qtype,
                           NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("hostname", *hostname);
  dict->SetInteger("query_type", qtype);
  return dict;
};

// ----------------------------------------------------------------------------

// A single asynchronous DNS exchange over UDP, which consists of sending out a
// DNS query, waiting for a response, and returning the response that it
// matches. Logging is done in the socket and in the outer DnsTransaction.
class DnsUDPAttempt {
 public:
  DnsUDPAttempt(scoped_ptr<DnsSession::SocketLease> socket_lease,
                scoped_ptr<DnsQuery> query,
                const CompletionCallback& callback)
      : next_state_(STATE_NONE),
        received_malformed_response_(false),
        socket_lease_(socket_lease.Pass()),
        query_(query.Pass()),
        callback_(callback) {
  }

  // Starts the attempt. Returns ERR_IO_PENDING if cannot complete synchronously
  // and calls |callback| upon completion.
  int Start() {
    DCHECK_EQ(STATE_NONE, next_state_);
    start_time_ = base::TimeTicks::Now();
    next_state_ = STATE_SEND_QUERY;
    return DoLoop(OK);
  }

  const DnsQuery* query() const {
    return query_.get();
  }

  const BoundNetLog& socket_net_log() const {
    return socket_lease_->socket()->NetLog();
  }

  // Returns the response or NULL if has not received a matching response from
  // the server.
  const DnsResponse* response() const {
    const DnsResponse* resp = response_.get();
    return (resp != NULL && resp->IsValid()) ? resp : NULL;
  }

  // Returns a Value representing the received response, along with a reference
  // to the NetLog source source of the UDP socket used.  The request must have
  // completed before this is called.
  Value* NetLogResponseCallback(NetLog::LogLevel /* log_level */) const {
    DCHECK(response_->IsValid());

    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("rcode", response_->rcode());
    dict->SetInteger("answer_count", response_->answer_count());
    socket_net_log().source().AddToEventParameters(dict);
    return dict;
  }

 private:
  enum State {
    STATE_SEND_QUERY,
    STATE_SEND_QUERY_COMPLETE,
    STATE_READ_RESPONSE,
    STATE_READ_RESPONSE_COMPLETE,
    STATE_NONE,
  };

  DatagramClientSocket* socket() {
    return socket_lease_->socket();
  }

  int DoLoop(int result) {
    CHECK_NE(STATE_NONE, next_state_);
    int rv = result;
    do {
      State state = next_state_;
      next_state_ = STATE_NONE;
      switch (state) {
        case STATE_SEND_QUERY:
          rv = DoSendQuery();
          break;
        case STATE_SEND_QUERY_COMPLETE:
          rv = DoSendQueryComplete(rv);
          break;
        case STATE_READ_RESPONSE:
          rv = DoReadResponse();
          break;
        case STATE_READ_RESPONSE_COMPLETE:
          rv = DoReadResponseComplete(rv);
          break;
        default:
          NOTREACHED();
          break;
      }
    } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
    // If we received a malformed response, and are now waiting for another one,
    // indicate to the transaction that the server might be misbehaving.
    if (rv == ERR_IO_PENDING && received_malformed_response_)
      return ERR_DNS_MALFORMED_RESPONSE;
    if (rv == OK) {
      DNS_HISTOGRAM("AsyncDNS.UDPAttemptSuccess",
                    base::TimeTicks::Now() - start_time_);
    } else if (rv != ERR_IO_PENDING) {
      DNS_HISTOGRAM("AsyncDNS.UDPAttemptFail",
                    base::TimeTicks::Now() - start_time_);
    }
    return rv;
  }

  int DoSendQuery() {
    next_state_ = STATE_SEND_QUERY_COMPLETE;
    return socket()->Write(query_->io_buffer(),
                              query_->io_buffer()->size(),
                              base::Bind(&DnsUDPAttempt::OnIOComplete,
                                         base::Unretained(this)));
  }

  int DoSendQueryComplete(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;

    // Writing to UDP should not result in a partial datagram.
    if (rv != query_->io_buffer()->size())
      return ERR_MSG_TOO_BIG;

    next_state_ = STATE_READ_RESPONSE;
    return OK;
  }

  int DoReadResponse() {
    next_state_ = STATE_READ_RESPONSE_COMPLETE;
    response_.reset(new DnsResponse());
    return socket()->Read(response_->io_buffer(),
                             response_->io_buffer()->size(),
                             base::Bind(&DnsUDPAttempt::OnIOComplete,
                                        base::Unretained(this)));
  }

  int DoReadResponseComplete(int rv) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;

    DCHECK(rv);
    if (!response_->InitParse(rv, *query_)) {
      // Other implementations simply ignore mismatched responses. Since each
      // DnsUDPAttempt binds to a different port, we might find that responses
      // to previously timed out queries lead to failures in the future.
      // Our solution is to make another attempt, in case the query truly
      // failed, but keep this attempt alive, in case it was a false alarm.
      received_malformed_response_ = true;
      next_state_ = STATE_READ_RESPONSE;
      return OK;
    }
    if (response_->flags() & dns_protocol::kFlagTC)
      return ERR_DNS_SERVER_REQUIRES_TCP;
    // TODO(szym): Extract TTL for NXDOMAIN results. http://crbug.com/115051
    if (response_->rcode() == dns_protocol::kRcodeNXDOMAIN)
      return ERR_NAME_NOT_RESOLVED;
    if (response_->rcode() != dns_protocol::kRcodeNOERROR)
      return ERR_DNS_SERVER_FAILED;

    CHECK(response());
    return OK;
  }

  void OnIOComplete(int rv) {
    rv = DoLoop(rv);
    if (rv != ERR_IO_PENDING)
      callback_.Run(rv);
  }

  State next_state_;
  bool received_malformed_response_;
  base::TimeTicks start_time_;

  scoped_ptr<DnsSession::SocketLease> socket_lease_;
  scoped_ptr<DnsQuery> query_;

  scoped_ptr<DnsResponse> response_;

  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DnsUDPAttempt);
};

// ----------------------------------------------------------------------------

// Implements DnsTransaction. Configuration is supplied by DnsSession.
// The suffix list is built according to the DnsConfig from the session.
// The timeout for each DnsUDPAttempt is given by DnsSession::NextTimeout.
// The first server to attempt on each query is given by
// DnsSession::NextFirstServerIndex, and the order is round-robin afterwards.
// Each server is attempted DnsConfig::attempts times.
class DnsTransactionImpl : public DnsTransaction,
                           public base::NonThreadSafe,
                           public base::SupportsWeakPtr<DnsTransactionImpl> {
 public:
  DnsTransactionImpl(DnsSession* session,
                     const std::string& hostname,
                     uint16 qtype,
                     const DnsTransactionFactory::CallbackType& callback,
                     const BoundNetLog& net_log)
    : session_(session),
      hostname_(hostname),
      qtype_(qtype),
      callback_(callback),
      net_log_(net_log),
      first_server_index_(0) {
    DCHECK(session_);
    DCHECK(!hostname_.empty());
    DCHECK(!callback_.is_null());
    DCHECK(!IsIPLiteral(hostname_));
  }

  virtual ~DnsTransactionImpl() {
    if (!callback_.is_null()) {
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_DNS_TRANSACTION,
                                        ERR_ABORTED);
    }  // otherwise logged in DoCallback or Start
  }

  virtual const std::string& GetHostname() const override {
    DCHECK(CalledOnValidThread());
    return hostname_;
  }

  virtual uint16 GetType() const override {
    DCHECK(CalledOnValidThread());
    return qtype_;
  }

  virtual int Start() override {
    DCHECK(!callback_.is_null());
    DCHECK(attempts_.empty());
    net_log_.BeginEvent(NetLog::TYPE_DNS_TRANSACTION,
                        base::Bind(&NetLogStartCallback, &hostname_, qtype_));
    int rv = PrepareSearch();
    if (rv == OK) {
      AttemptResult result = ProcessAttemptResult(StartQuery());
      if (result.rv == OK) {
        // DnsTransaction must never succeed synchronously.
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&DnsTransactionImpl::DoCallback, AsWeakPtr(), result));
        return ERR_IO_PENDING;
      }
      rv = result.rv;
    }
    if (rv != ERR_IO_PENDING) {
      callback_.Reset();
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_DNS_TRANSACTION, rv);
    }
    DCHECK_NE(OK, rv);
    return rv;
  }

 private:
  // Wrapper for the result of a DnsUDPAttempt.
  struct AttemptResult {
    AttemptResult(int rv, const DnsUDPAttempt* attempt)
        : rv(rv), attempt(attempt) {}

    int rv;
    const DnsUDPAttempt* attempt;
  };

  // Prepares |qnames_| according to the DnsConfig.
  int PrepareSearch() {
    const DnsConfig& config = session_->config();

    std::string labeled_hostname;
    if (!DNSDomainFromDot(hostname_, &labeled_hostname))
      return ERR_INVALID_ARGUMENT;

    if (hostname_[hostname_.size() - 1] == '.') {
      // It's a fully-qualified name, no suffix search.
      qnames_.push_back(labeled_hostname);
      return OK;
    }

    int ndots = CountLabels(labeled_hostname) - 1;

    if (ndots > 0 && !config.append_to_multi_label_name) {
      qnames_.push_back(labeled_hostname);
      return OK;
    }

    // Set true when |labeled_hostname| is put on the list.
    bool had_hostname = false;

    if (ndots >= config.ndots) {
      qnames_.push_back(labeled_hostname);
      had_hostname = true;
    }

    std::string qname;
    for (size_t i = 0; i < config.search.size(); ++i) {
      // Ignore invalid (too long) combinations.
      if (!DNSDomainFromDot(hostname_ + "." + config.search[i], &qname))
        continue;
      if (qname.size() == labeled_hostname.size()) {
        if (had_hostname)
          continue;
        had_hostname = true;
      }
      qnames_.push_back(qname);
    }

    if (ndots > 0 && !had_hostname)
      qnames_.push_back(labeled_hostname);

    return qnames_.empty() ? ERR_DNS_SEARCH_EMPTY : OK;
  }

  void DoCallback(AttemptResult result) {
    DCHECK(!callback_.is_null());
    DCHECK_NE(ERR_IO_PENDING, result.rv);
    const DnsResponse* response = result.attempt ?
        result.attempt->response() : NULL;
    CHECK(result.rv != OK || response != NULL);

    timer_.Stop();

    DnsTransactionFactory::CallbackType callback = callback_;
    callback_.Reset();

    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_DNS_TRANSACTION, result.rv);
    callback.Run(this, result.rv, response);
  }

  // Makes another attempt at the current name, |qnames_.front()|, using the
  // next nameserver.
  AttemptResult MakeAttempt() {
    unsigned attempt_number = attempts_.size();

    uint16 id = session_->NextQueryId();
    scoped_ptr<DnsQuery> query;
    if (attempts_.empty()) {
      query.reset(new DnsQuery(id, qnames_.front(), qtype_));
    } else {
      query.reset(attempts_[0]->query()->CloneWithNewId(id));
    }

    const DnsConfig& config = session_->config();

    unsigned server_index = first_server_index_ +
        (attempt_number % config.nameservers.size());

    scoped_ptr<DnsSession::SocketLease> lease =
        session_->AllocateSocket(server_index, net_log_.source());

    bool got_socket = !!lease.get();

    DnsUDPAttempt* attempt = new DnsUDPAttempt(
        lease.Pass(),
        query.Pass(),
        base::Bind(&DnsTransactionImpl::OnAttemptComplete,
                   base::Unretained(this),
                   attempt_number));

    attempts_.push_back(attempt);

    if (!got_socket)
      return AttemptResult(ERR_CONNECTION_REFUSED, NULL);

    net_log_.AddEvent(
        NetLog::TYPE_DNS_TRANSACTION_ATTEMPT,
        attempt->socket_net_log().source().ToEventParametersCallback());

    int rv = attempt->Start();
    if (rv == ERR_IO_PENDING) {
      timer_.Stop();
      base::TimeDelta timeout = session_->NextTimeout(attempt_number);
      timer_.Start(FROM_HERE, timeout, this, &DnsTransactionImpl::OnTimeout);
    }
    return AttemptResult(rv, attempt);
  }

  // Begins query for the current name. Makes the first attempt.
  AttemptResult StartQuery() {
    std::string dotted_qname = DNSDomainToString(qnames_.front());
    net_log_.BeginEvent(NetLog::TYPE_DNS_TRANSACTION_QUERY,
                        NetLog::StringCallback("qname", &dotted_qname));

    first_server_index_ = session_->NextFirstServerIndex();

    attempts_.clear();
    return MakeAttempt();
  }

  void OnAttemptComplete(unsigned attempt_number, int rv) {
    if (callback_.is_null())
      return;
    DCHECK_LT(attempt_number, attempts_.size());
    const DnsUDPAttempt* attempt = attempts_[attempt_number];
    AttemptResult result = ProcessAttemptResult(AttemptResult(rv, attempt));
    if (result.rv != ERR_IO_PENDING)
      DoCallback(result);
  }

  void LogResponse(const DnsUDPAttempt* attempt) {
    if (attempt && attempt->response()) {
      net_log_.AddEvent(
          NetLog::TYPE_DNS_TRANSACTION_RESPONSE,
          base::Bind(&DnsUDPAttempt::NetLogResponseCallback,
                     base::Unretained(attempt)));
    }
  }

  bool MoreAttemptsAllowed() const {
    const DnsConfig& config = session_->config();
    return attempts_.size() < config.attempts * config.nameservers.size();
  }

  // Resolves the result of a DnsUDPAttempt until a terminal result is reached
  // or it will complete asynchronously (ERR_IO_PENDING).
  AttemptResult ProcessAttemptResult(AttemptResult result) {
    while (result.rv != ERR_IO_PENDING) {
      LogResponse(result.attempt);

      switch (result.rv) {
        case OK:
          net_log_.EndEventWithNetErrorCode(
              NetLog::TYPE_DNS_TRANSACTION_QUERY, result.rv);
          DCHECK(result.attempt);
          DCHECK(result.attempt->response());
          return result;
        case ERR_NAME_NOT_RESOLVED:
          net_log_.EndEventWithNetErrorCode(
              NetLog::TYPE_DNS_TRANSACTION_QUERY, result.rv);
          // Try next suffix.
          qnames_.pop_front();
          if (qnames_.empty()) {
            return AttemptResult(ERR_NAME_NOT_RESOLVED, NULL);
          } else {
            result = StartQuery();
          }
          break;
        case ERR_CONNECTION_REFUSED:
        case ERR_DNS_TIMED_OUT:
          if (MoreAttemptsAllowed()) {
            result = MakeAttempt();
          } else {
            return result;
          }
          break;
        default:
          // Server failure.
          DCHECK(result.attempt);
          if (result.attempt != attempts_.back()) {
            // This attempt already timed out. Ignore it.
            return AttemptResult(ERR_IO_PENDING, NULL);
          }
          if (MoreAttemptsAllowed()) {
            result = MakeAttempt();
          } else if (result.rv == ERR_DNS_MALFORMED_RESPONSE) {
            // Wait until the last attempt times out.
            return AttemptResult(ERR_IO_PENDING, NULL);
          } else {
            return AttemptResult(result.rv, NULL);
          }
          break;
      }
    }
    return result;
  }

  void OnTimeout() {
    if (callback_.is_null())
      return;
    AttemptResult result = ProcessAttemptResult(
        AttemptResult(ERR_DNS_TIMED_OUT, NULL));
    if (result.rv != ERR_IO_PENDING)
      DoCallback(result);
  }

  scoped_refptr<DnsSession> session_;
  std::string hostname_;
  uint16 qtype_;
  // Cleared in DoCallback.
  DnsTransactionFactory::CallbackType callback_;

  BoundNetLog net_log_;

  // Search list of fully-qualified DNS names to query next (in DNS format).
  std::deque<std::string> qnames_;

  // List of attempts for the current name.
  ScopedVector<DnsUDPAttempt> attempts_;

  // Index of the first server to try on each search query.
  int first_server_index_;

  base::OneShotTimer<DnsTransactionImpl> timer_;

  DISALLOW_COPY_AND_ASSIGN(DnsTransactionImpl);
};

// ----------------------------------------------------------------------------

// Implementation of DnsTransactionFactory that returns instances of
// DnsTransactionImpl.
class DnsTransactionFactoryImpl : public DnsTransactionFactory {
 public:
  explicit DnsTransactionFactoryImpl(DnsSession* session) {
    session_ = session;
  }

  virtual scoped_ptr<DnsTransaction> CreateTransaction(
      const std::string& hostname,
      uint16 qtype,
      const CallbackType& callback,
      const BoundNetLog& net_log) override {
    return scoped_ptr<DnsTransaction>(new DnsTransactionImpl(session_,
                                                             hostname,
                                                             qtype,
                                                             callback,
                                                             net_log));
  }

 private:
  scoped_refptr<DnsSession> session_;
};

}  // namespace

// static
scoped_ptr<DnsTransactionFactory> DnsTransactionFactory::CreateFactory(
    DnsSession* session) {
  return scoped_ptr<DnsTransactionFactory>(
      new DnsTransactionFactoryImpl(session));
}

}  // namespace net
