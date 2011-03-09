// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_test_util.h"

#include <algorithm>
#include <vector>


#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "net/base/address_family.h"
#include "net/base/auth.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket.h"
#include "net/socket/ssl_host_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#define NET_TRACE(level, s)   DLOG(level) << s << __FUNCTION__ << "() "

namespace net {

namespace {

inline char AsciifyHigh(char x) {
  char nybble = static_cast<char>((x >> 4) & 0x0F);
  return nybble + ((nybble < 0x0A) ? '0' : 'A' - 10);
}

inline char AsciifyLow(char x) {
  char nybble = static_cast<char>((x >> 0) & 0x0F);
  return nybble + ((nybble < 0x0A) ? '0' : 'A' - 10);
}

inline char Asciify(char x) {
  if ((x < 0) || !isprint(x))
    return '.';
  return x;
}

void DumpData(const char* data, int data_len) {
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;
  DVLOG(1) << "Length:  " << data_len;
  const char* pfx = "Data:    ";
  if (!data || (data_len <= 0)) {
    DVLOG(1) << pfx << "<None>";
  } else {
    int i;
    for (i = 0; i <= (data_len - 4); i += 4) {
      DVLOG(1) << pfx
               << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
               << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
               << AsciifyHigh(data[i + 2]) << AsciifyLow(data[i + 2])
               << AsciifyHigh(data[i + 3]) << AsciifyLow(data[i + 3])
               << "  '"
               << Asciify(data[i + 0])
               << Asciify(data[i + 1])
               << Asciify(data[i + 2])
               << Asciify(data[i + 3])
               << "'";
      pfx = "         ";
    }
    // Take care of any 'trailing' bytes, if data_len was not a multiple of 4.
    switch (data_len - i) {
      case 3:
        DVLOG(1) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
                 << AsciifyHigh(data[i + 2]) << AsciifyLow(data[i + 2])
                 << "    '"
                 << Asciify(data[i + 0])
                 << Asciify(data[i + 1])
                 << Asciify(data[i + 2])
                 << " '";
        break;
      case 2:
        DVLOG(1) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
                 << "      '"
                 << Asciify(data[i + 0])
                 << Asciify(data[i + 1])
                 << "  '";
        break;
      case 1:
        DVLOG(1) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << "        '"
                 << Asciify(data[i + 0])
                 << "   '";
        break;
    }
  }
}

void DumpMockRead(const MockRead& r) {
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;
  DVLOG(1) << "Async:   " << r.async
           << "\nResult:  " << r.result;
  DumpData(r.data, r.data_len);
  const char* stop = (r.sequence_number & MockRead::STOPLOOP) ? " (STOP)" : "";
  DVLOG(1) << "Stage:   " << (r.sequence_number & ~MockRead::STOPLOOP) << stop
           << "\nTime:    " << r.time_stamp.ToInternalValue();
}

}  // namespace

StaticSocketDataProvider::StaticSocketDataProvider()
    : reads_(NULL),
      read_index_(0),
      read_count_(0),
      writes_(NULL),
      write_index_(0),
      write_count_(0) {
}

StaticSocketDataProvider::StaticSocketDataProvider(MockRead* reads,
                                                   size_t reads_count,
                                                   MockWrite* writes,
                                                   size_t writes_count)
    : reads_(reads),
      read_index_(0),
      read_count_(reads_count),
      writes_(writes),
      write_index_(0),
      write_count_(writes_count) {
}

StaticSocketDataProvider::~StaticSocketDataProvider() {}

const MockRead& StaticSocketDataProvider::PeekRead() const {
  DCHECK(!at_read_eof());
  return reads_[read_index_];
}

const MockWrite& StaticSocketDataProvider::PeekWrite() const {
  DCHECK(!at_write_eof());
  return writes_[write_index_];
}

const MockRead& StaticSocketDataProvider::PeekRead(size_t index) const {
  DCHECK_LT(index, read_count_);
  return reads_[index];
}

const MockWrite& StaticSocketDataProvider::PeekWrite(size_t index) const {
  DCHECK_LT(index, write_count_);
  return writes_[index];
}

MockRead StaticSocketDataProvider::GetNextRead() {
  DCHECK(!at_read_eof());
  reads_[read_index_].time_stamp = base::Time::Now();
  return reads_[read_index_++];
}

MockWriteResult StaticSocketDataProvider::OnWrite(const std::string& data) {
  if (!writes_) {
    // Not using mock writes; succeed synchronously.
    return MockWriteResult(false, data.length());
  }

  DCHECK(!at_write_eof());

  // Check that what we are writing matches the expectation.
  // Then give the mocked return value.
  net::MockWrite* w = &writes_[write_index_++];
  w->time_stamp = base::Time::Now();
  int result = w->result;
  if (w->data) {
    // Note - we can simulate a partial write here.  If the expected data
    // is a match, but shorter than the write actually written, that is legal.
    // Example:
    //   Application writes "foobarbaz" (9 bytes)
    //   Expected write was "foo" (3 bytes)
    //   This is a success, and we return 3 to the application.
    std::string expected_data(w->data, w->data_len);
    EXPECT_GE(data.length(), expected_data.length());
    std::string actual_data(data.substr(0, w->data_len));
    EXPECT_EQ(expected_data, actual_data);
    if (expected_data != actual_data)
      return MockWriteResult(false, net::ERR_UNEXPECTED);
    if (result == net::OK)
      result = w->data_len;
  }
  return MockWriteResult(w->async, result);
}

void StaticSocketDataProvider::Reset() {
  read_index_ = 0;
  write_index_ = 0;
}

DynamicSocketDataProvider::DynamicSocketDataProvider()
    : short_read_limit_(0),
      allow_unconsumed_reads_(false) {
}

DynamicSocketDataProvider::~DynamicSocketDataProvider() {}

MockRead DynamicSocketDataProvider::GetNextRead() {
  if (reads_.empty())
    return MockRead(false, ERR_UNEXPECTED);
  MockRead result = reads_.front();
  if (short_read_limit_ == 0 || result.data_len <= short_read_limit_) {
    reads_.pop_front();
  } else {
    result.data_len = short_read_limit_;
    reads_.front().data += result.data_len;
    reads_.front().data_len -= result.data_len;
  }
  return result;
}

void DynamicSocketDataProvider::Reset() {
  reads_.clear();
}

void DynamicSocketDataProvider::SimulateRead(const char* data,
                                             const size_t length) {
  if (!allow_unconsumed_reads_) {
    EXPECT_TRUE(reads_.empty()) << "Unconsumed read: " << reads_.front().data;
  }
  reads_.push_back(MockRead(true, data, length));
}

DelayedSocketData::DelayedSocketData(
    int write_delay, MockRead* reads, size_t reads_count,
    MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      write_delay_(write_delay),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  DCHECK_GE(write_delay_, 0);
}

DelayedSocketData::DelayedSocketData(
    const MockConnect& connect, int write_delay, MockRead* reads,
    size_t reads_count, MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      write_delay_(write_delay),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  DCHECK_GE(write_delay_, 0);
  set_connect_data(connect);
}

DelayedSocketData::~DelayedSocketData() {
}

void DelayedSocketData::ForceNextRead() {
  write_delay_ = 0;
  CompleteRead();
}

MockRead DelayedSocketData::GetNextRead() {
  if (write_delay_ > 0)
    return MockRead(true, ERR_IO_PENDING);
  return StaticSocketDataProvider::GetNextRead();
}

MockWriteResult DelayedSocketData::OnWrite(const std::string& data) {
  MockWriteResult rv = StaticSocketDataProvider::OnWrite(data);
  // Now that our write has completed, we can allow reads to continue.
  if (!--write_delay_)
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
      factory_.NewRunnableMethod(&DelayedSocketData::CompleteRead), 100);
  return rv;
}

void DelayedSocketData::Reset() {
  set_socket(NULL);
  factory_.RevokeAll();
  StaticSocketDataProvider::Reset();
}

void DelayedSocketData::CompleteRead() {
  if (socket())
    socket()->OnReadComplete(GetNextRead());
}

OrderedSocketData::OrderedSocketData(
    MockRead* reads, size_t reads_count, MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      sequence_number_(0), loop_stop_stage_(0), callback_(NULL),
      blocked_(false), ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
}

OrderedSocketData::OrderedSocketData(
    const MockConnect& connect,
    MockRead* reads, size_t reads_count,
    MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      sequence_number_(0), loop_stop_stage_(0), callback_(NULL),
      blocked_(false), ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  set_connect_data(connect);
}

void OrderedSocketData::EndLoop() {
  // If we've already stopped the loop, don't do it again until we've advanced
  // to the next sequence_number.
  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_ << ": EndLoop()";
  if (loop_stop_stage_ > 0) {
    const MockRead& next_read = StaticSocketDataProvider::PeekRead();
    if ((next_read.sequence_number & ~MockRead::STOPLOOP) >
        loop_stop_stage_) {
      NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                                << ": Clearing stop index";
      loop_stop_stage_ = 0;
    } else {
      return;
    }
  }
  // Record the sequence_number at which we stopped the loop.
  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                            << ": Posting Quit at read " << read_index();
  loop_stop_stage_ = sequence_number_;
  if (callback_)
    callback_->RunWithParams(Tuple1<int>(ERR_IO_PENDING));
}

MockRead OrderedSocketData::GetNextRead() {
  factory_.RevokeAll();
  blocked_ = false;
  const MockRead& next_read = StaticSocketDataProvider::PeekRead();
  if (next_read.sequence_number & MockRead::STOPLOOP)
    EndLoop();
  if ((next_read.sequence_number & ~MockRead::STOPLOOP) <=
      sequence_number_++) {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_ - 1
                              << ": Read " << read_index();
    DumpMockRead(next_read);
    return StaticSocketDataProvider::GetNextRead();
  }
  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_ - 1
                            << ": I/O Pending";
  MockRead result = MockRead(true, ERR_IO_PENDING);
  DumpMockRead(result);
  blocked_ = true;
  return result;
}

MockWriteResult OrderedSocketData::OnWrite(const std::string& data) {
  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                            << ": Write " << write_index();
  DumpMockRead(PeekWrite());
  ++sequence_number_;
  if (blocked_) {
    // TODO(willchan): This 100ms delay seems to work around some weirdness.  We
    // should probably fix the weirdness.  One example is in SpdyStream,
    // DoSendRequest() will return ERR_IO_PENDING, and there's a race.  If the
    // SYN_REPLY causes OnResponseReceived() to get called before
    // SpdyStream::ReadResponseHeaders() is called, we hit a NOTREACHED().
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        factory_.NewRunnableMethod(&OrderedSocketData::CompleteRead), 100);
  }
  return StaticSocketDataProvider::OnWrite(data);
}

void OrderedSocketData::Reset() {
  NET_TRACE(INFO, "  *** ") << "Stage "
                            << sequence_number_ << ": Reset()";
  sequence_number_ = 0;
  loop_stop_stage_ = 0;
  set_socket(NULL);
  factory_.RevokeAll();
  StaticSocketDataProvider::Reset();
}

void OrderedSocketData::CompleteRead() {
  if (socket()) {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_;
    socket()->OnReadComplete(GetNextRead());
  }
}

OrderedSocketData::~OrderedSocketData() {}

DeterministicSocketData::DeterministicSocketData(MockRead* reads,
    size_t reads_count, MockWrite* writes, size_t writes_count)
    : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
      sequence_number_(0),
      current_read_(),
      current_write_(),
      stopping_sequence_number_(0),
      stopped_(false),
      print_debug_(false) {}

DeterministicSocketData::~DeterministicSocketData() {}

void DeterministicSocketData::Run() {
  SetStopped(false);
  int counter = 0;
  // Continue to consume data until all data has run out, or the stopped_ flag
  // has been set. Consuming data requires two separate operations -- running
  // the tasks in the message loop, and explicitly invoking the read/write
  // callbacks (simulating network I/O). We check our conditions between each,
  // since they can change in either.
  while ((!at_write_eof() || !at_read_eof()) && !stopped()) {
    if (counter % 2 == 0)
      MessageLoop::current()->RunAllPending();
    if (counter % 2 == 1) {
      InvokeCallbacks();
    }
    counter++;
  }
  // We're done consuming new data, but it is possible there are still some
  // pending callbacks which we expect to complete before returning.
  while (socket_ && (socket_->write_pending() || socket_->read_pending()) &&
         !stopped()) {
    InvokeCallbacks();
    MessageLoop::current()->RunAllPending();
  }
  SetStopped(false);
}

void DeterministicSocketData::RunFor(int steps) {
  StopAfter(steps);
  Run();
}

void DeterministicSocketData::SetStop(int seq) {
  DCHECK_LT(sequence_number_, seq);
  stopping_sequence_number_ = seq;
  stopped_ = false;
}

void DeterministicSocketData::StopAfter(int seq) {
  SetStop(sequence_number_ + seq);
}

MockRead DeterministicSocketData::GetNextRead() {
  current_read_ = StaticSocketDataProvider::PeekRead();
  EXPECT_LE(sequence_number_, current_read_.sequence_number);

  // Synchronous read while stopped is an error
  if (stopped() && !current_read_.async) {
    LOG(ERROR) << "Unable to perform synchronous IO while stopped";
    return MockRead(false, ERR_UNEXPECTED);
  }

  // Async read which will be called back in a future step.
  if (sequence_number_ < current_read_.sequence_number) {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                              << ": I/O Pending";
    MockRead result = MockRead(false, ERR_IO_PENDING);
    if (!current_read_.async) {
      LOG(ERROR) << "Unable to perform synchronous read: "
          << current_read_.sequence_number
          << " at stage: " << sequence_number_;
      result = MockRead(false, ERR_UNEXPECTED);
    }
    if (print_debug_)
      DumpMockRead(result);
    return result;
  }

  NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                            << ": Read " << read_index();
  if (print_debug_)
    DumpMockRead(current_read_);

  // Increment the sequence number if IO is complete
  if (!current_read_.async)
    NextStep();

  DCHECK_NE(ERR_IO_PENDING, current_read_.result);
  StaticSocketDataProvider::GetNextRead();

  return current_read_;
}

MockWriteResult DeterministicSocketData::OnWrite(const std::string& data) {
  const MockWrite& next_write = StaticSocketDataProvider::PeekWrite();
  current_write_ = next_write;

  // Synchronous write while stopped is an error
  if (stopped() && !next_write.async) {
    LOG(ERROR) << "Unable to perform synchronous IO while stopped";
    return MockWriteResult(false, ERR_UNEXPECTED);
  }

  // Async write which will be called back in a future step.
  if (sequence_number_ < next_write.sequence_number) {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                              << ": I/O Pending";
    if (!next_write.async) {
      LOG(ERROR) << "Unable to perform synchronous write: "
          << next_write.sequence_number << " at stage: " << sequence_number_;
      return MockWriteResult(false, ERR_UNEXPECTED);
    }
  } else {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
                              << ": Write " << write_index();
  }

  if (print_debug_)
    DumpMockRead(next_write);

  // Move to the next step if I/O is synchronous, since the operation will
  // complete when this method returns.
  if (!next_write.async)
    NextStep();

  // This is either a sync write for this step, or an async write.
  return StaticSocketDataProvider::OnWrite(data);
}

void DeterministicSocketData::Reset() {
  NET_TRACE(INFO, "  *** ") << "Stage "
                            << sequence_number_ << ": Reset()";
  sequence_number_ = 0;
  StaticSocketDataProvider::Reset();
  NOTREACHED();
}

void DeterministicSocketData::InvokeCallbacks() {
  if (socket_ && socket_->write_pending() &&
      (current_write().sequence_number == sequence_number())) {
    socket_->CompleteWrite();
    NextStep();
    return;
  }
  if (socket_ && socket_->read_pending() &&
      (current_read().sequence_number == sequence_number())) {
    socket_->CompleteRead();
    NextStep();
    return;
  }
}

void DeterministicSocketData::NextStep() {
  // Invariant: Can never move *past* the stopping step.
  DCHECK_LT(sequence_number_, stopping_sequence_number_);
  sequence_number_++;
  if (sequence_number_ == stopping_sequence_number_)
    SetStopped(true);
}

MockClientSocketFactory::MockClientSocketFactory() {}

MockClientSocketFactory::~MockClientSocketFactory() {}

void MockClientSocketFactory::AddSocketDataProvider(
    SocketDataProvider* data) {
  mock_data_.Add(data);
}

void MockClientSocketFactory::AddSSLSocketDataProvider(
    SSLSocketDataProvider* data) {
  mock_ssl_data_.Add(data);
}

void MockClientSocketFactory::ResetNextMockIndexes() {
  mock_data_.ResetNextIndex();
  mock_ssl_data_.ResetNextIndex();
}

MockTCPClientSocket* MockClientSocketFactory::GetMockTCPClientSocket(
    size_t index) const {
  DCHECK_LT(index, tcp_client_sockets_.size());
  return tcp_client_sockets_[index];
}

MockSSLClientSocket* MockClientSocketFactory::GetMockSSLClientSocket(
    size_t index) const {
  DCHECK_LT(index, ssl_client_sockets_.size());
  return ssl_client_sockets_[index];
}

ClientSocket* MockClientSocketFactory::CreateTCPClientSocket(
    const AddressList& addresses,
    net::NetLog* net_log,
    const NetLog::Source& source) {
  SocketDataProvider* data_provider = mock_data_.GetNext();
  MockTCPClientSocket* socket =
      new MockTCPClientSocket(addresses, net_log, data_provider);
  data_provider->set_socket(socket);
  tcp_client_sockets_.push_back(socket);
  return socket;
}

SSLClientSocket* MockClientSocketFactory::CreateSSLClientSocket(
    ClientSocketHandle* transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    SSLHostInfo* ssl_host_info,
    CertVerifier* cert_verifier,
    DnsCertProvenanceChecker* dns_cert_checker) {
  MockSSLClientSocket* socket =
      new MockSSLClientSocket(transport_socket, host_and_port, ssl_config,
                              ssl_host_info, mock_ssl_data_.GetNext());
  ssl_client_sockets_.push_back(socket);
  return socket;
}

void MockClientSocketFactory::ClearSSLSessionCache() {
}

MockClientSocket::MockClientSocket(net::NetLog* net_log)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      connected_(false),
      net_log_(NetLog::Source(), net_log) {
}

bool MockClientSocket::SetReceiveBufferSize(int32 size) {
  return true;
}

bool MockClientSocket::SetSendBufferSize(int32 size) {
  return true;
}

void MockClientSocket::Disconnect() {
  connected_ = false;
}

bool MockClientSocket::IsConnected() const {
  return connected_;
}

bool MockClientSocket::IsConnectedAndIdle() const {
  return connected_;
}

int MockClientSocket::GetPeerAddress(AddressList* address) const {
  return net::SystemHostResolverProc("192.0.2.33", ADDRESS_FAMILY_UNSPECIFIED,
                                     0, address, NULL);
}

const BoundNetLog& MockClientSocket::NetLog() const {
  return net_log_;
}

void MockClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  NOTREACHED();
}

void MockClientSocket::GetSSLCertRequestInfo(
    net::SSLCertRequestInfo* cert_request_info) {
}

SSLClientSocket::NextProtoStatus
MockClientSocket::GetNextProto(std::string* proto) {
  proto->clear();
  return SSLClientSocket::kNextProtoUnsupported;
}

MockClientSocket::~MockClientSocket() {}

void MockClientSocket::RunCallbackAsync(net::CompletionCallback* callback,
                                        int result) {
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &MockClientSocket::RunCallback, callback, result));
}

void MockClientSocket::RunCallback(net::CompletionCallback* callback,
                                   int result) {
  if (callback)
    callback->Run(result);
}

MockTCPClientSocket::MockTCPClientSocket(const net::AddressList& addresses,
                                         net::NetLog* net_log,
                                         net::SocketDataProvider* data)
    : MockClientSocket(net_log),
      addresses_(addresses),
      data_(data),
      read_offset_(0),
      read_data_(false, net::ERR_UNEXPECTED),
      need_read_data_(true),
      peer_closed_connection_(false),
      pending_buf_(NULL),
      pending_buf_len_(0),
      pending_callback_(NULL),
      was_used_to_convey_data_(false) {
  DCHECK(data_);
  data_->Reset();
}

int MockTCPClientSocket::Read(net::IOBuffer* buf, int buf_len,
                              net::CompletionCallback* callback) {
  if (!connected_)
    return net::ERR_UNEXPECTED;

  // If the buffer is already in use, a read is already in progress!
  DCHECK(pending_buf_ == NULL);

  // Store our async IO data.
  pending_buf_ = buf;
  pending_buf_len_ = buf_len;
  pending_callback_ = callback;

  if (need_read_data_) {
    read_data_ = data_->GetNextRead();
    if (read_data_.result == ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ) {
      // This MockRead is just a marker to instruct us to set
      // peer_closed_connection_.  Skip it and get the next one.
      read_data_ = data_->GetNextRead();
      peer_closed_connection_ = true;
    }
    // ERR_IO_PENDING means that the SocketDataProvider is taking responsibility
    // to complete the async IO manually later (via OnReadComplete).
    if (read_data_.result == ERR_IO_PENDING) {
      DCHECK(callback);  // We need to be using async IO in this case.
      return ERR_IO_PENDING;
    }
    need_read_data_ = false;
  }

  return CompleteRead();
}

int MockTCPClientSocket::Write(net::IOBuffer* buf, int buf_len,
                               net::CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  if (!connected_)
    return net::ERR_UNEXPECTED;

  std::string data(buf->data(), buf_len);
  net::MockWriteResult write_result = data_->OnWrite(data);

  was_used_to_convey_data_ = true;

  if (write_result.async) {
    RunCallbackAsync(callback, write_result.result);
    return net::ERR_IO_PENDING;
  }

  return write_result.result;
}

int MockTCPClientSocket::Connect(net::CompletionCallback* callback) {
  if (connected_)
    return net::OK;
  connected_ = true;
  peer_closed_connection_ = false;
  if (data_->connect_data().async) {
    RunCallbackAsync(callback, data_->connect_data().result);
    return net::ERR_IO_PENDING;
  }
  return data_->connect_data().result;
}

void MockTCPClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
  pending_callback_ = NULL;
}

bool MockTCPClientSocket::IsConnected() const {
  return connected_ && !peer_closed_connection_;
}

bool MockTCPClientSocket::IsConnectedAndIdle() const {
  return IsConnected();
}

bool MockTCPClientSocket::WasEverUsed() const {
  return was_used_to_convey_data_;
}

bool MockTCPClientSocket::UsingTCPFastOpen() const {
  return false;
}

void MockTCPClientSocket::OnReadComplete(const MockRead& data) {
  // There must be a read pending.
  DCHECK(pending_buf_);
  // You can't complete a read with another ERR_IO_PENDING status code.
  DCHECK_NE(ERR_IO_PENDING, data.result);
  // Since we've been waiting for data, need_read_data_ should be true.
  DCHECK(need_read_data_);

  read_data_ = data;
  need_read_data_ = false;

  // The caller is simulating that this IO completes right now.  Don't
  // let CompleteRead() schedule a callback.
  read_data_.async = false;

  net::CompletionCallback* callback = pending_callback_;
  int rv = CompleteRead();
  RunCallback(callback, rv);
}

int MockTCPClientSocket::CompleteRead() {
  DCHECK(pending_buf_);
  DCHECK(pending_buf_len_ > 0);

  was_used_to_convey_data_ = true;

  // Save the pending async IO data and reset our |pending_| state.
  net::IOBuffer* buf = pending_buf_;
  int buf_len = pending_buf_len_;
  net::CompletionCallback* callback = pending_callback_;
  pending_buf_ = NULL;
  pending_buf_len_ = 0;
  pending_callback_ = NULL;

  int result = read_data_.result;
  DCHECK(result != ERR_IO_PENDING);

  if (read_data_.data) {
    if (read_data_.data_len - read_offset_ > 0) {
      result = std::min(buf_len, read_data_.data_len - read_offset_);
      memcpy(buf->data(), read_data_.data + read_offset_, result);
      read_offset_ += result;
      if (read_offset_ == read_data_.data_len) {
        need_read_data_ = true;
        read_offset_ = 0;
      }
    } else {
      result = 0;  // EOF
    }
  }

  if (read_data_.async) {
    DCHECK(callback);
    RunCallbackAsync(callback, result);
    return net::ERR_IO_PENDING;
  }
  return result;
}

DeterministicMockTCPClientSocket::DeterministicMockTCPClientSocket(
    net::NetLog* net_log, net::DeterministicSocketData* data)
    : MockClientSocket(net_log),
      write_pending_(false),
      write_callback_(NULL),
      write_result_(0),
      read_data_(),
      read_buf_(NULL),
      read_buf_len_(0),
      read_pending_(false),
      read_callback_(NULL),
      data_(data),
      was_used_to_convey_data_(false) {}

DeterministicMockTCPClientSocket::~DeterministicMockTCPClientSocket() {}

void DeterministicMockTCPClientSocket::CompleteWrite() {
  was_used_to_convey_data_ = true;
  write_pending_ = false;
  write_callback_->Run(write_result_);
}

int DeterministicMockTCPClientSocket::CompleteRead() {
  DCHECK_GT(read_buf_len_, 0);
  DCHECK_LE(read_data_.data_len, read_buf_len_);
  DCHECK(read_buf_);

  was_used_to_convey_data_ = true;

  if (read_data_.result == ERR_IO_PENDING)
    read_data_ = data_->GetNextRead();
  DCHECK_NE(ERR_IO_PENDING, read_data_.result);
  // If read_data_.async is true, we do not need to wait, since this is already
  // the callback. Therefore we don't even bother to check it.
  int result = read_data_.result;

  if (read_data_.data_len > 0) {
    DCHECK(read_data_.data);
    result = std::min(read_buf_len_, read_data_.data_len);
    memcpy(read_buf_->data(), read_data_.data, result);
  }

  if (read_pending_) {
    read_pending_ = false;
    read_callback_->Run(result);
  }

  return result;
}

int DeterministicMockTCPClientSocket::Write(
    net::IOBuffer* buf, int buf_len, net::CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  if (!connected_)
    return net::ERR_UNEXPECTED;

  std::string data(buf->data(), buf_len);
  net::MockWriteResult write_result = data_->OnWrite(data);

  if (write_result.async) {
    write_callback_ = callback;
    write_result_ = write_result.result;
    DCHECK(write_callback_ != NULL);
    write_pending_ = true;
    return net::ERR_IO_PENDING;
  }

  was_used_to_convey_data_ = true;
  write_pending_ = false;
  return write_result.result;
}

int DeterministicMockTCPClientSocket::Read(
    net::IOBuffer* buf, int buf_len, net::CompletionCallback* callback) {
  if (!connected_)
    return net::ERR_UNEXPECTED;

  read_data_ = data_->GetNextRead();
  // The buffer should always be big enough to contain all the MockRead data. To
  // use small buffers, split the data into multiple MockReads.
  DCHECK_LE(read_data_.data_len, buf_len);

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = callback;

  if (read_data_.async || (read_data_.result == ERR_IO_PENDING)) {
    read_pending_ = true;
    DCHECK(read_callback_);
    return ERR_IO_PENDING;
  }

  was_used_to_convey_data_ = true;
  return CompleteRead();
}

// TODO(erikchen): Support connect sequencing.
int DeterministicMockTCPClientSocket::Connect(
    net::CompletionCallback* callback) {
  if (connected_)
    return net::OK;
  connected_ = true;
  if (data_->connect_data().async) {
    RunCallbackAsync(callback, data_->connect_data().result);
    return net::ERR_IO_PENDING;
  }
  return data_->connect_data().result;
}

void DeterministicMockTCPClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
}

bool DeterministicMockTCPClientSocket::IsConnected() const {
  return connected_;
}

bool DeterministicMockTCPClientSocket::IsConnectedAndIdle() const {
  return IsConnected();
}

bool DeterministicMockTCPClientSocket::WasEverUsed() const {
  return was_used_to_convey_data_;
}

bool DeterministicMockTCPClientSocket::UsingTCPFastOpen() const {
  return false;
}

void DeterministicMockTCPClientSocket::OnReadComplete(const MockRead& data) {}

class MockSSLClientSocket::ConnectCallback
    : public net::CompletionCallbackImpl<MockSSLClientSocket::ConnectCallback> {
 public:
  ConnectCallback(MockSSLClientSocket *ssl_client_socket,
                  net::CompletionCallback* user_callback,
                  int rv)
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          net::CompletionCallbackImpl<MockSSLClientSocket::ConnectCallback>(
                this, &ConnectCallback::Wrapper)),
        ssl_client_socket_(ssl_client_socket),
        user_callback_(user_callback),
        rv_(rv) {
  }

 private:
  void Wrapper(int rv) {
    if (rv_ == net::OK)
      ssl_client_socket_->connected_ = true;
    user_callback_->Run(rv_);
    delete this;
  }

  MockSSLClientSocket* ssl_client_socket_;
  net::CompletionCallback* user_callback_;
  int rv_;
};

MockSSLClientSocket::MockSSLClientSocket(
    net::ClientSocketHandle* transport_socket,
    const HostPortPair& host_port_pair,
    const net::SSLConfig& ssl_config,
    SSLHostInfo* ssl_host_info,
    net::SSLSocketDataProvider* data)
    : MockClientSocket(transport_socket->socket()->NetLog().net_log()),
      transport_(transport_socket),
      data_(data),
      is_npn_state_set_(false),
      new_npn_value_(false) {
  DCHECK(data_);
  delete ssl_host_info;  // we take ownership but don't use it.
}

MockSSLClientSocket::~MockSSLClientSocket() {
  Disconnect();
}

int MockSSLClientSocket::Read(net::IOBuffer* buf, int buf_len,
                              net::CompletionCallback* callback) {
  return transport_->socket()->Read(buf, buf_len, callback);
}

int MockSSLClientSocket::Write(net::IOBuffer* buf, int buf_len,
                               net::CompletionCallback* callback) {
  return transport_->socket()->Write(buf, buf_len, callback);
}

int MockSSLClientSocket::Connect(net::CompletionCallback* callback) {
  ConnectCallback* connect_callback = new ConnectCallback(
      this, callback, data_->connect.result);
  int rv = transport_->socket()->Connect(connect_callback);
  if (rv == net::OK) {
    delete connect_callback;
    if (data_->connect.result == net::OK)
      connected_ = true;
    if (data_->connect.async) {
      RunCallbackAsync(callback, data_->connect.result);
      return net::ERR_IO_PENDING;
    }
    return data_->connect.result;
  }
  return rv;
}

void MockSSLClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
  if (transport_->socket() != NULL)
    transport_->socket()->Disconnect();
}

bool MockSSLClientSocket::IsConnected() const {
  return transport_->socket()->IsConnected();
}

bool MockSSLClientSocket::WasEverUsed() const {
  return transport_->socket()->WasEverUsed();
}

bool MockSSLClientSocket::UsingTCPFastOpen() const {
  return transport_->socket()->UsingTCPFastOpen();
}

void MockSSLClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  ssl_info->Reset();
  ssl_info->cert = data_->cert_;
}

void MockSSLClientSocket::GetSSLCertRequestInfo(
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK(cert_request_info);
  if (data_->cert_request_info) {
    cert_request_info->host_and_port =
        data_->cert_request_info->host_and_port;
    cert_request_info->client_certs = data_->cert_request_info->client_certs;
  } else {
    cert_request_info->Reset();
  }
}

SSLClientSocket::NextProtoStatus MockSSLClientSocket::GetNextProto(
    std::string* proto) {
  *proto = data_->next_proto;
  return data_->next_proto_status;
}

bool MockSSLClientSocket::was_npn_negotiated() const {
  if (is_npn_state_set_)
    return new_npn_value_;
  return data_->was_npn_negotiated;
}

bool MockSSLClientSocket::set_was_npn_negotiated(bool negotiated) {
  is_npn_state_set_ = true;
  return new_npn_value_ = negotiated;
}

void MockSSLClientSocket::OnReadComplete(const MockRead& data) {
  NOTIMPLEMENTED();
}

TestSocketRequest::TestSocketRequest(
    std::vector<TestSocketRequest*>* request_order,
    size_t* completion_count)
    : request_order_(request_order),
      completion_count_(completion_count) {
  DCHECK(request_order);
  DCHECK(completion_count);
}

TestSocketRequest::~TestSocketRequest() {
}

int TestSocketRequest::WaitForResult() {
  return callback_.WaitForResult();
}

void TestSocketRequest::RunWithParams(const Tuple1<int>& params) {
  callback_.RunWithParams(params);
  (*completion_count_)++;
  request_order_->push_back(this);
}

// static
const int ClientSocketPoolTest::kIndexOutOfBounds = -1;

// static
const int ClientSocketPoolTest::kRequestNotFound = -2;

ClientSocketPoolTest::ClientSocketPoolTest() : completion_count_(0) {}
ClientSocketPoolTest::~ClientSocketPoolTest() {}

int ClientSocketPoolTest::GetOrderOfRequest(size_t index) const {
  index--;
  if (index >= requests_.size())
    return kIndexOutOfBounds;

  for (size_t i = 0; i < request_order_.size(); i++)
    if (requests_[index] == request_order_[i])
      return i + 1;

  return kRequestNotFound;
}

bool ClientSocketPoolTest::ReleaseOneConnection(KeepAlive keep_alive) {
  ScopedVector<TestSocketRequest>::iterator i;
  for (i = requests_.begin(); i != requests_.end(); ++i) {
    if ((*i)->handle()->is_initialized()) {
      if (keep_alive == NO_KEEP_ALIVE)
        (*i)->handle()->socket()->Disconnect();
      (*i)->handle()->Reset();
      MessageLoop::current()->RunAllPending();
      return true;
    }
  }
  return false;
}

void ClientSocketPoolTest::ReleaseAllConnections(KeepAlive keep_alive) {
  bool released_one;
  do {
    released_one = ReleaseOneConnection(keep_alive);
  } while (released_one);
}

MockTCPClientSocketPool::MockConnectJob::MockConnectJob(
    ClientSocket* socket,
    ClientSocketHandle* handle,
    CompletionCallback* callback)
    : socket_(socket),
      handle_(handle),
      user_callback_(callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          connect_callback_(this, &MockConnectJob::OnConnect)) {
}

MockTCPClientSocketPool::MockConnectJob::~MockConnectJob() {}

int MockTCPClientSocketPool::MockConnectJob::Connect() {
  int rv = socket_->Connect(&connect_callback_);
  if (rv == OK) {
    user_callback_ = NULL;
    OnConnect(OK);
  }
  return rv;
}

bool MockTCPClientSocketPool::MockConnectJob::CancelHandle(
    const ClientSocketHandle* handle) {
  if (handle != handle_)
    return false;
  socket_.reset();
  handle_ = NULL;
  user_callback_ = NULL;
  return true;
}

void MockTCPClientSocketPool::MockConnectJob::OnConnect(int rv) {
  if (!socket_.get())
    return;
  if (rv == OK) {
    handle_->set_socket(socket_.release());
  } else {
    socket_.reset();
  }

  handle_ = NULL;

  if (user_callback_) {
    CompletionCallback* callback = user_callback_;
    user_callback_ = NULL;
    callback->Run(rv);
  }
}

MockTCPClientSocketPool::MockTCPClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    ClientSocketPoolHistograms* histograms,
    ClientSocketFactory* socket_factory)
    : TCPClientSocketPool(max_sockets, max_sockets_per_group, histograms,
                          NULL, NULL, NULL),
      client_socket_factory_(socket_factory),
      release_count_(0),
      cancel_count_(0) {
}

MockTCPClientSocketPool::~MockTCPClientSocketPool() {}

int MockTCPClientSocketPool::RequestSocket(const std::string& group_name,
                                           const void* socket_params,
                                           RequestPriority priority,
                                           ClientSocketHandle* handle,
                                           CompletionCallback* callback,
                                           const BoundNetLog& net_log) {
  ClientSocket* socket = client_socket_factory_->CreateTCPClientSocket(
      AddressList(), net_log.net_log(), net::NetLog::Source());
  MockConnectJob* job = new MockConnectJob(socket, handle, callback);
  job_list_.push_back(job);
  handle->set_pool_id(1);
  return job->Connect();
}

void MockTCPClientSocketPool::CancelRequest(const std::string& group_name,
                                            ClientSocketHandle* handle) {
  std::vector<MockConnectJob*>::iterator i;
  for (i = job_list_.begin(); i != job_list_.end(); ++i) {
    if ((*i)->CancelHandle(handle)) {
      cancel_count_++;
      break;
    }
  }
}

void MockTCPClientSocketPool::ReleaseSocket(const std::string& group_name,
                                            ClientSocket* socket, int id) {
  EXPECT_EQ(1, id);
  release_count_++;
  delete socket;
}

DeterministicMockClientSocketFactory::DeterministicMockClientSocketFactory() {}

DeterministicMockClientSocketFactory::~DeterministicMockClientSocketFactory() {}

void DeterministicMockClientSocketFactory::AddSocketDataProvider(
    DeterministicSocketData* data) {
  mock_data_.Add(data);
}

void DeterministicMockClientSocketFactory::AddSSLSocketDataProvider(
    SSLSocketDataProvider* data) {
  mock_ssl_data_.Add(data);
}

void DeterministicMockClientSocketFactory::ResetNextMockIndexes() {
  mock_data_.ResetNextIndex();
  mock_ssl_data_.ResetNextIndex();
}

MockSSLClientSocket* DeterministicMockClientSocketFactory::
    GetMockSSLClientSocket(size_t index) const {
  DCHECK_LT(index, ssl_client_sockets_.size());
  return ssl_client_sockets_[index];
}

ClientSocket* DeterministicMockClientSocketFactory::CreateTCPClientSocket(
    const AddressList& addresses,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  DeterministicSocketData* data_provider = mock_data().GetNext();
  DeterministicMockTCPClientSocket* socket =
      new DeterministicMockTCPClientSocket(net_log, data_provider);
  data_provider->set_socket(socket->AsWeakPtr());
  tcp_client_sockets().push_back(socket);
  return socket;
}

SSLClientSocket* DeterministicMockClientSocketFactory::CreateSSLClientSocket(
    ClientSocketHandle* transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    SSLHostInfo* ssl_host_info,
    CertVerifier* cert_verifier,
    DnsCertProvenanceChecker* dns_cert_checker) {
  MockSSLClientSocket* socket =
      new MockSSLClientSocket(transport_socket, host_and_port, ssl_config,
                              ssl_host_info, mock_ssl_data_.GetNext());
  ssl_client_sockets_.push_back(socket);
  return socket;
}

void DeterministicMockClientSocketFactory::ClearSSLSessionCache() {
}

MockSOCKSClientSocketPool::MockSOCKSClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    ClientSocketPoolHistograms* histograms,
    TCPClientSocketPool* tcp_pool)
    : SOCKSClientSocketPool(max_sockets, max_sockets_per_group, histograms,
                            NULL, tcp_pool, NULL),
      tcp_pool_(tcp_pool) {
}

MockSOCKSClientSocketPool::~MockSOCKSClientSocketPool() {}

int MockSOCKSClientSocketPool::RequestSocket(const std::string& group_name,
                                             const void* socket_params,
                                             RequestPriority priority,
                                             ClientSocketHandle* handle,
                                             CompletionCallback* callback,
                                             const BoundNetLog& net_log) {
  return tcp_pool_->RequestSocket(group_name, socket_params, priority, handle,
                                  callback, net_log);
}

void MockSOCKSClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  return tcp_pool_->CancelRequest(group_name, handle);
}

void MockSOCKSClientSocketPool::ReleaseSocket(const std::string& group_name,
                                              ClientSocket* socket, int id) {
  return tcp_pool_->ReleaseSocket(group_name, socket, id);
}

const char kSOCKS5GreetRequest[] = { 0x05, 0x01, 0x00 };
const int kSOCKS5GreetRequestLength = arraysize(kSOCKS5GreetRequest);

const char kSOCKS5GreetResponse[] = { 0x05, 0x00 };
const int kSOCKS5GreetResponseLength = arraysize(kSOCKS5GreetResponse);

const char kSOCKS5OkRequest[] =
    { 0x05, 0x01, 0x00, 0x03, 0x04, 'h', 'o', 's', 't', 0x00, 0x50 };
const int kSOCKS5OkRequestLength = arraysize(kSOCKS5OkRequest);

const char kSOCKS5OkResponse[] =
    { 0x05, 0x00, 0x00, 0x01, 127, 0, 0, 1, 0x00, 0x50 };
const int kSOCKS5OkResponseLength = arraysize(kSOCKS5OkResponse);

}  // namespace net
