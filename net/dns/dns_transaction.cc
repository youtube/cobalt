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
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
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
#include "net/socket/client_socket_factory.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

namespace {

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

class StartParameters : public NetLog::EventParameters {
 public:
  StartParameters(const std::string& hostname,
                  uint16 qtype)
      : hostname_(hostname), qtype_(qtype) {}

  virtual Value* ToValue() const OVERRIDE {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("hostname", hostname_);
    dict->SetInteger("query_type", qtype_);
    return dict;
  }

 private:
  const std::string hostname_;
  const uint16 qtype_;
};

class ResponseParameters : public NetLog::EventParameters {
 public:
  ResponseParameters(int rcode, int answer_count, const NetLog::Source& source)
      : rcode_(rcode), answer_count_(answer_count), source_(source) {}

  virtual Value* ToValue() const OVERRIDE {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("rcode", rcode_);
    dict->SetInteger("answer_count", answer_count_);
    dict->Set("source_dependency", source_.ToValue());
    return dict;
  }

 private:
  const int rcode_;
  const int answer_count_;
  const NetLog::Source source_;
};

// ----------------------------------------------------------------------------

// A single asynchronous DNS exchange over UDP, which consists of sending out a
// DNS query, waiting for a response, and returning the response that it
// matches. Logging is done in the socket and in the outer DnsTransaction.
class DnsUDPAttempt {
 public:
  DnsUDPAttempt(scoped_ptr<DatagramClientSocket> socket,
                const IPEndPoint& server,
                scoped_ptr<DnsQuery> query,
                const CompletionCallback& callback)
      : next_state_(STATE_NONE),
        socket_(socket.Pass()),
        server_(server),
        query_(query.Pass()),
        callback_(callback) {
  }

  // Starts the attempt. Returns ERR_IO_PENDING if cannot complete synchronously
  // and calls |callback| upon completion.
  int Start() {
    DCHECK_EQ(STATE_NONE, next_state_);
    next_state_ = STATE_CONNECT;
    return DoLoop(OK);
  }

  const DnsQuery* query() const {
    return query_.get();
  }

  const DatagramClientSocket* socket() const {
    return socket_.get();
  }

  // Returns the response or NULL if has not received a matching response from
  // the server.
  const DnsResponse* response() const {
    const DnsResponse* resp = response_.get();
    return (resp != NULL && resp->IsValid()) ? resp : NULL;
  }

 private:
  enum State {
    STATE_CONNECT,
    STATE_SEND_QUERY,
    STATE_SEND_QUERY_COMPLETE,
    STATE_READ_RESPONSE,
    STATE_READ_RESPONSE_COMPLETE,
    STATE_NONE,
  };

  int DoLoop(int result) {
    DCHECK_NE(STATE_NONE, next_state_);
    int rv = result;
    do {
      State state = next_state_;
      next_state_ = STATE_NONE;
      switch (state) {
        case STATE_CONNECT:
          rv = DoConnect();
          break;
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

    return rv;
  }

  int DoConnect() {
    next_state_ = STATE_SEND_QUERY;
    return socket_->Connect(server_);
  }

  int DoSendQuery() {
    next_state_ = STATE_SEND_QUERY_COMPLETE;
    return socket_->Write(query_->io_buffer(),
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
    return socket_->Read(response_->io_buffer(),
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
      // TODO(szym): Consider making this reaction less aggressive.
      // Other implementations simply ignore mismatched responses. Since each
      // DnsUDPAttempt binds to a different port, we might find that responses
      // to previously timed out queries lead to failures in the future.
      // http://crbug.com/107413
      return ERR_DNS_MALFORMED_RESPONSE;
    }
    if (response_->flags() & dns_protocol::kFlagTC)
      return ERR_DNS_SERVER_REQUIRES_TCP;
    // TODO(szym): Extract TTL for NXDOMAIN results. http://crbug.com/115051
    if (response_->rcode() == dns_protocol::kRcodeNXDOMAIN)
      return ERR_NAME_NOT_RESOLVED;
    if (response_->rcode() != dns_protocol::kRcodeNOERROR)
      return ERR_DNS_SERVER_FAILED;

    return OK;
  }

  void OnIOComplete(int rv) {
    rv = DoLoop(rv);
    if (rv != ERR_IO_PENDING)
      callback_.Run(rv);
  }

  State next_state_;

  scoped_ptr<DatagramClientSocket> socket_;
  IPEndPoint server_;
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
class DnsTransactionImpl : public DnsTransaction, public base::NonThreadSafe {
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
    STLDeleteElements(&attempts_);
  }

  virtual const std::string& GetHostname() const OVERRIDE {
    DCHECK(CalledOnValidThread());
    return hostname_;
  }

  virtual uint16 GetType() const OVERRIDE {
    DCHECK(CalledOnValidThread());
    return qtype_;
  }

  virtual int Start() OVERRIDE {
    DCHECK(!callback_.is_null());
    DCHECK(attempts_.empty());
    net_log_.BeginEvent(NetLog::TYPE_DNS_TRANSACTION, make_scoped_refptr(
        new StartParameters(hostname_, qtype_)));
    int rv = PrepareSearch();
    if (rv == OK)
      rv = StartQuery();
    if (rv == OK) {
      // In the very unlikely case that we immediately received the response, we
      // cannot simply return OK nor run the callback, but instead complete
      // asynchronously.
      // TODO(szym): replace Unretained with WeakPtr factory.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&DnsTransactionImpl::DoCallback,
                     base::Unretained(this),
                     OK,
                     attempts_.back()));
      return ERR_IO_PENDING;
    }
    if (rv != ERR_IO_PENDING) {
      // Clear |callback_| to catch re-starts.
      callback_.Reset();
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_DNS_TRANSACTION, rv);
    }
    return rv;
  }

 private:
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

    return qnames_.empty() ? ERR_NAME_NOT_RESOLVED : OK;
  }

  void DoCallback(int rv, const DnsUDPAttempt* successful_attempt) {
    if (callback_.is_null())
      return;
    DCHECK_NE(ERR_IO_PENDING, rv);
    DCHECK(rv != OK || successful_attempt != NULL);

    DnsTransactionFactory::CallbackType callback = callback_;
    callback_.Reset();
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_DNS_TRANSACTION, rv);
    callback.Run(this,
                 rv,
                 successful_attempt ? successful_attempt->response() : NULL);
  }

  // Makes another attempt at the current name, |qnames_.front()|, using the
  // next nameserver.
  int MakeAttempt() {
    unsigned attempt_number = attempts_.size();

#if defined(OS_WIN)
    // Avoid the Windows firewall warning about explicit UDP binding.
    // TODO(szym): Reuse a pool of pre-bound sockets. http://crbug.com/107413
    DatagramSocket::BindType bind_type = DatagramSocket::DEFAULT_BIND;
#else
    DatagramSocket::BindType bind_type = DatagramSocket::RANDOM_BIND;
#endif

    scoped_ptr<DatagramClientSocket> socket(
        session_->socket_factory()->CreateDatagramClientSocket(
            bind_type,
            base::Bind(&base::RandInt),
            net_log_.net_log(),
            net_log_.source()));

    uint16 id = session_->NextQueryId();
    scoped_ptr<DnsQuery> query;
    if (attempts_.empty()) {
      query.reset(new DnsQuery(id, qnames_.front(), qtype_));
    } else {
      query.reset(attempts_[0]->query()->CloneWithNewId(id));
    }

    net_log_.AddEvent(NetLog::TYPE_DNS_TRANSACTION_ATTEMPT, make_scoped_refptr(
        new NetLogSourceParameter("source_dependency",
                                  socket->NetLog().source())));

    const DnsConfig& config = session_->config();

    unsigned server_index = first_server_index_ +
        (attempt_number % config.nameservers.size());

    DnsUDPAttempt* attempt = new DnsUDPAttempt(
        socket.Pass(),
        config.nameservers[server_index],
        query.Pass(),
        base::Bind(&DnsTransactionImpl::OnAttemptComplete,
                   base::Unretained(this),
                   attempt_number));

    base::TimeDelta timeout = session_->NextTimeout(attempt_number);
    timer_.Start(FROM_HERE, timeout, this, &DnsTransactionImpl::OnTimeout);
    attempts_.push_back(attempt);
    return attempt->Start();
  }

  // Begins query for the current name. Makes the first attempt.
  int StartQuery() {
    std::string dotted_qname = DNSDomainToString(qnames_.front());
    net_log_.BeginEvent(
        NetLog::TYPE_DNS_TRANSACTION_QUERY,
        make_scoped_refptr(new NetLogStringParameter("qname", dotted_qname)));

    first_server_index_ = session_->NextFirstServerIndex();

    STLDeleteElements(&attempts_);
    return MakeAttempt();
  }

  void OnAttemptComplete(unsigned attempt_number, int rv) {
    DCHECK_LT(attempt_number, attempts_.size());
    timer_.Stop();

    const DnsUDPAttempt* attempt = attempts_[attempt_number];

    if (attempt->response()) {
      net_log_.AddEvent(
          NetLog::TYPE_DNS_TRANSACTION_RESPONSE,
          make_scoped_refptr(
              new ResponseParameters(attempt->response()->rcode(),
                                     attempt->response()->answer_count(),
                                     attempt->socket()->NetLog().source())));
    }

    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_DNS_TRANSACTION_QUERY, rv);

    switch (rv) {
      case ERR_NAME_NOT_RESOLVED:
        // Try next suffix.
        qnames_.pop_front();
        if (qnames_.empty())
          rv = ERR_NAME_NOT_RESOLVED;
        else
          rv = StartQuery();
        break;
      case OK:
        DoCallback(rv, attempt);
        return;
      default:
        // Some nameservers could fail so try the next one.
        const DnsConfig& config = session_->config();
        if (attempts_.size() < config.attempts * config.nameservers.size()) {
          rv = MakeAttempt();
        } else {
          // TODO(szym): Should this be different than the timeout case?
          rv = ERR_DNS_SERVER_FAILED;
        }
        break;
    }
    if (rv != ERR_IO_PENDING)
      DoCallback(rv, NULL);
  }

  void OnTimeout() {
    const DnsConfig& config = session_->config();
    if (attempts_.size() == config.attempts * config.nameservers.size()) {
      DoCallback(ERR_DNS_TIMED_OUT, NULL);
      return;
    }
    int rv = MakeAttempt();
    if (rv != ERR_IO_PENDING)
      DoCallback(rv, NULL);
  }

  scoped_refptr<DnsSession> session_;
  std::string hostname_;
  uint16 qtype_;
  // Set to null once the transaction completes.
  DnsTransactionFactory::CallbackType callback_;

  BoundNetLog net_log_;

  // Search list of fully-qualified DNS names to query next (in DNS format).
  std::deque<std::string> qnames_;

  // List of attempts for the current name.
  std::vector<DnsUDPAttempt*> attempts_;

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
      const BoundNetLog& net_log) OVERRIDE {
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

