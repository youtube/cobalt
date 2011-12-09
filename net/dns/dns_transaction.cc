// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/socket/client_socket_factory.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

namespace {

class DnsTransactionStartParameters : public NetLog::EventParameters {
 public:
  DnsTransactionStartParameters(const IPEndPoint& dns_server,
                                const base::StringPiece& qname,
                                uint16 qtype,
                                const NetLog::Source& source)
      : dns_server_(dns_server),
        qname_(qname.data(), qname.length()),
        qtype_(qtype),
        source_(source) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("dns_server", dns_server_.ToString());
    dict->SetString("hostname", qname_);
    dict->SetInteger("query_type", qtype_);
    if (source_.is_valid())
      dict->Set("source_dependency", source_.ToValue());
    return dict;
  }

 private:
  IPEndPoint dns_server_;
  std::string qname_;
  uint16 qtype_;
  const NetLog::Source source_;
};

class DnsTransactionFinishParameters : public NetLog::EventParameters {
 public:
  // TODO(szym): add rcode ?
  DnsTransactionFinishParameters(int net_error, int answer_count)
      : net_error_(net_error), answer_count_(answer_count) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    if (net_error_)
      dict->SetInteger("net_error", net_error_);
    dict->SetInteger("answer_count", answer_count_);
    return dict;
  }

 private:
  const int net_error_;
  const int answer_count_;
};

class DnsTransactionRetryParameters : public NetLog::EventParameters {
 public:
  DnsTransactionRetryParameters(int attempt_number,
                                const NetLog::Source& source)
      : attempt_number_(attempt_number), source_(source) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("attempt_number", attempt_number_);
    dict->Set("source_dependency", source_.ToValue());
    return dict;
  }

 private:
  const int attempt_number_;
  const NetLog::Source source_;
};

}  // namespace


DnsTransaction::DnsTransaction(DnsSession* session,
                               const base::StringPiece& qname,
                               uint16 qtype,
                               const ResultCallback& callback,
                               const BoundNetLog& source_net_log)
    : session_(session),
      dns_server_(session->NextServer()),
      query_(new DnsQuery(session->NextId(), qname, qtype)),
      callback_(callback),
      attempts_(0),
      next_state_(STATE_NONE),
      net_log_(BoundNetLog::Make(session->net_log(),
          NetLog::SOURCE_DNS_TRANSACTION)) {
  net_log_.BeginEvent(
      NetLog::TYPE_DNS_TRANSACTION,
      make_scoped_refptr(
          new DnsTransactionStartParameters(dns_server_,
                                            qname,
                                            qtype,
                                            source_net_log.source())));
}

DnsTransaction::~DnsTransaction() {}

int DnsTransaction::Start() {
  DCHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_CONNECT;
  return DoLoop(OK);
}

int DnsTransaction::DoLoop(int result) {
  DCHECK_NE(STATE_NONE, next_state_);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_CONNECT:
        rv = DoConnect();
        break;
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
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

void DnsTransaction::DoCallback(int result) {
  DCHECK_NE(result, ERR_IO_PENDING);
  int answer_count = (result == OK) ? response()->answer_count() : 0;
  net_log_.EndEvent(
      NetLog::TYPE_DNS_TRANSACTION,
      make_scoped_refptr(
          new DnsTransactionFinishParameters(result, answer_count)));
  callback_.Run(this, result);
}

void DnsTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int DnsTransaction::DoConnect() {
  next_state_ = STATE_CONNECT_COMPLETE;

  StartTimer(session_->NextTimeout(attempts_));
  ++attempts_;

  // TODO(szym): keep all sockets around in case the server responds
  // after its timeout; state machine will need to change to handle that.
  // The current plan is to move socket management out to DnsSession.
  // Hence also move retransmissions to DnsClient::Request.
  socket_.reset(session_->socket_factory()->CreateDatagramClientSocket(
      DatagramSocket::RANDOM_BIND,
      base::Bind(&base::RandInt),
      net_log_.net_log(),
      net_log_.source()));

  net_log_.AddEvent(
      NetLog::TYPE_DNS_TRANSACTION_ATTEMPT_STARTED,
      make_scoped_refptr(
          new DnsTransactionRetryParameters(attempts_,
                                            socket_->NetLog().source())));

  return socket_->Connect(dns_server_);
}

int DnsTransaction::DoConnectComplete(int rv) {
  if (rv < 0)
    return rv;
  next_state_ = STATE_SEND_QUERY;
  return OK;
}

int DnsTransaction::DoSendQuery() {
  next_state_ = STATE_SEND_QUERY_COMPLETE;
  return socket_->Write(query_->io_buffer(),
                        query_->io_buffer()->size(),
                        base::Bind(&DnsTransaction::OnIOComplete,
                                   base::Unretained(this)));
}

int DnsTransaction::DoSendQueryComplete(int rv) {
  if (rv < 0)
    return rv;

  // Writing to UDP should not result in a partial datagram.
  if (rv != query_->io_buffer()->size())
    return ERR_MSG_TOO_BIG;

  next_state_ = STATE_READ_RESPONSE;
  return OK;
}

int DnsTransaction::DoReadResponse() {
  next_state_ = STATE_READ_RESPONSE_COMPLETE;
  response_.reset(new DnsResponse());
  return socket_->Read(response_->io_buffer(),
                       response_->io_buffer()->size(),
                       base::Bind(&DnsTransaction::OnIOComplete,
                                  base::Unretained(this)));
}

int DnsTransaction::DoReadResponseComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  RevokeTimer();
  if (rv < 0)
    return rv;

  DCHECK(rv);
  if (!response_->InitParse(rv, *query_))
    return ERR_DNS_MALFORMED_RESPONSE;
  // TODO(szym): define this flag value in dns_protocol
  if (response_->flags1() & 2)
    return ERR_DNS_SERVER_REQUIRES_TCP;
  // TODO(szym): move this handling out of DnsTransaction?
  if (response_->rcode() != dns_protocol::kRcodeNOERROR &&
      response_->rcode() != dns_protocol::kRcodeNXDOMAIN) {
    return ERR_DNS_SERVER_FAILED;
  }
  // TODO(szym): add ERR_DNS_RR_NOT_FOUND?
  if (response_->answer_count() == 0)
    return ERR_NAME_NOT_RESOLVED;

  return OK;
}

void DnsTransaction::StartTimer(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay, this, &DnsTransaction::OnTimeout);
}

void DnsTransaction::RevokeTimer() {
  timer_.Stop();
}

void DnsTransaction::OnTimeout() {
  DCHECK(next_state_ == STATE_SEND_QUERY_COMPLETE ||
         next_state_ == STATE_READ_RESPONSE_COMPLETE);
  if (attempts_ == session_->config().attempts) {
    DoCallback(ERR_DNS_TIMED_OUT);
    return;
  }
  next_state_ = STATE_CONNECT;
  query_.reset(query_->CloneWithNewId(session_->NextId()));
  int rv = DoLoop(OK);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

}  // namespace net
