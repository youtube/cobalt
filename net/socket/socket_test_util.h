// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_TEST_UTIL_H_
#define NET_SOCKET_SOCKET_TEST_UTIL_H_
#pragma once

#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/udp/datagram_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

enum {
  // A private network error code used by the socket test utility classes.
  // If the |result| member of a MockRead is
  // ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ, that MockRead is just a
  // marker that indicates the peer will close the connection after the next
  // MockRead.  The other members of that MockRead are ignored.
  ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ = -10000,
};

class AsyncSocket;
class MockClientSocket;
class SSLClientSocket;
class SSLHostInfo;
class StreamSocket;

struct MockConnect {
  // Asynchronous connection success.
  MockConnect() : async(true), result(OK) { }
  MockConnect(bool a, int r) : async(a), result(r) { }

  bool async;
  int result;
};

struct MockRead {
  // Flag to indicate that the message loop should be terminated.
  enum {
    STOPLOOP = 1 << 31
  };

  // Default
  MockRead() : async(false), result(0), data(NULL), data_len(0),
      sequence_number(0), time_stamp(base::Time::Now()) {}

  // Read failure (no data).
  MockRead(bool async, int result) : async(async) , result(result), data(NULL),
      data_len(0), sequence_number(0), time_stamp(base::Time::Now()) { }

  // Read failure (no data), with sequence information.
  MockRead(bool async, int result, int seq) : async(async) , result(result),
      data(NULL), data_len(0), sequence_number(seq),
      time_stamp(base::Time::Now()) { }

  // Asynchronous read success (inferred data length).
  explicit MockRead(const char* data) : async(true),  result(0), data(data),
      data_len(strlen(data)), sequence_number(0),
      time_stamp(base::Time::Now()) { }

  // Read success (inferred data length).
  MockRead(bool async, const char* data) : async(async), result(0), data(data),
      data_len(strlen(data)), sequence_number(0),
      time_stamp(base::Time::Now()) { }

  // Read success.
  MockRead(bool async, const char* data, int data_len) : async(async),
      result(0), data(data), data_len(data_len), sequence_number(0),
      time_stamp(base::Time::Now()) { }

  // Read success (inferred data length) with sequence information.
  MockRead(bool async, int seq, const char* data) : async(async),
      result(0), data(data), data_len(strlen(data)), sequence_number(seq),
      time_stamp(base::Time::Now()) { }

  // Read success with sequence information.
  MockRead(bool async, const char* data, int data_len, int seq) : async(async),
      result(0), data(data), data_len(data_len), sequence_number(seq),
      time_stamp(base::Time::Now()) { }

  bool async;
  int result;
  const char* data;
  int data_len;

  // For OrderedSocketData, which only allows reads to occur in a particular
  // sequence.  If a read occurs before the given |sequence_number| is reached,
  // an ERR_IO_PENDING is returned.
  int sequence_number;      // The sequence number at which a read is allowed
                            // to occur.
  base::Time time_stamp;    // The time stamp at which the operation occurred.
};

// MockWrite uses the same member fields as MockRead, but with different
// meanings. The expected input to MockTCPClientSocket::Write() is given
// by {data, data_len}, and the return value of Write() is controlled by
// {async, result}.
typedef MockRead MockWrite;

struct MockWriteResult {
  MockWriteResult(bool async, int result) : async(async), result(result) {}

  bool async;
  int result;
};

// The SocketDataProvider is an interface used by the MockClientSocket
// for getting data about individual reads and writes on the socket.
class SocketDataProvider {
 public:
  SocketDataProvider() : socket_(NULL) {}

  virtual ~SocketDataProvider() {}

  // Returns the buffer and result code for the next simulated read.
  // If the |MockRead.result| is ERR_IO_PENDING, it informs the caller
  // that it will be called via the AsyncSocket::OnReadComplete()
  // function at a later time.
  virtual MockRead GetNextRead() = 0;
  virtual MockWriteResult OnWrite(const std::string& data) = 0;
  virtual void Reset() = 0;

  // Accessor for the socket which is using the SocketDataProvider.
  AsyncSocket* socket() { return socket_; }
  void set_socket(AsyncSocket* socket) { socket_ = socket; }

  MockConnect connect_data() const { return connect_; }
  void set_connect_data(const MockConnect& connect) { connect_ = connect; }

 private:
  MockConnect connect_;
  AsyncSocket* socket_;

  DISALLOW_COPY_AND_ASSIGN(SocketDataProvider);
};

// The AsyncSocket is an interface used by the SocketDataProvider to
// complete the asynchronous read operation.
class AsyncSocket {
 public:
  // If an async IO is pending because the SocketDataProvider returned
  // ERR_IO_PENDING, then the AsyncSocket waits until this OnReadComplete
  // is called to complete the asynchronous read operation.
  // data.async is ignored, and this read is completed synchronously as
  // part of this call.
  virtual void OnReadComplete(const MockRead& data) = 0;
};

// SocketDataProvider which responds based on static tables of mock reads and
// writes.
class StaticSocketDataProvider : public SocketDataProvider {
 public:
  StaticSocketDataProvider();
  StaticSocketDataProvider(MockRead* reads, size_t reads_count,
                           MockWrite* writes, size_t writes_count);
  virtual ~StaticSocketDataProvider();

  // These functions get access to the next available read and write data.
  const MockRead& PeekRead() const;
  const MockWrite& PeekWrite() const;
  // These functions get random access to the read and write data, for timing.
  const MockRead& PeekRead(size_t index) const;
  const MockWrite& PeekWrite(size_t index) const;
  size_t read_index() const { return read_index_; }
  size_t write_index() const { return write_index_; }
  size_t read_count() const { return read_count_; }
  size_t write_count() const { return write_count_; }

  bool at_read_eof() const { return read_index_ >= read_count_; }
  bool at_write_eof() const { return write_index_ >= write_count_; }

  virtual void CompleteRead() {}

  // SocketDataProvider methods:
  virtual MockRead GetNextRead() OVERRIDE;
  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE;
  virtual void Reset() OVERRIDE;

 private:
  MockRead* reads_;
  size_t read_index_;
  size_t read_count_;
  MockWrite* writes_;
  size_t write_index_;
  size_t write_count_;

  DISALLOW_COPY_AND_ASSIGN(StaticSocketDataProvider);
};

// SocketDataProvider which can make decisions about next mock reads based on
// received writes. It can also be used to enforce order of operations, for
// example that tested code must send the "Hello!" message before receiving
// response. This is useful for testing conversation-like protocols like FTP.
class DynamicSocketDataProvider : public SocketDataProvider {
 public:
  DynamicSocketDataProvider();
  virtual ~DynamicSocketDataProvider();

  int short_read_limit() const { return short_read_limit_; }
  void set_short_read_limit(int limit) { short_read_limit_ = limit; }

  void allow_unconsumed_reads(bool allow) { allow_unconsumed_reads_ = allow; }

  // SocketDataProvider methods:
  virtual MockRead GetNextRead() OVERRIDE;
  virtual MockWriteResult OnWrite(const std::string& data) = 0;
  virtual void Reset() OVERRIDE;

 protected:
  // The next time there is a read from this socket, it will return |data|.
  // Before calling SimulateRead next time, the previous data must be consumed.
  void SimulateRead(const char* data, size_t length);
  void SimulateRead(const char* data) {
    SimulateRead(data, std::strlen(data));
  }

 private:
  std::deque<MockRead> reads_;

  // Max number of bytes we will read at a time. 0 means no limit.
  int short_read_limit_;

  // If true, we'll not require the client to consume all data before we
  // mock the next read.
  bool allow_unconsumed_reads_;

  DISALLOW_COPY_AND_ASSIGN(DynamicSocketDataProvider);
};

// SSLSocketDataProviders only need to keep track of the return code from calls
// to Connect().
struct SSLSocketDataProvider {
  SSLSocketDataProvider(bool async, int result);
  ~SSLSocketDataProvider();

  MockConnect connect;
  SSLClientSocket::NextProtoStatus next_proto_status;
  std::string next_proto;
  std::string server_protos;
  bool was_npn_negotiated;
  bool client_cert_sent;
  net::SSLCertRequestInfo* cert_request_info;
  scoped_refptr<X509Certificate> cert;
};

// A DataProvider where the client must write a request before the reads (e.g.
// the response) will complete.
class DelayedSocketData : public StaticSocketDataProvider,
                          public base::RefCounted<DelayedSocketData> {
 public:
  // |write_delay| the number of MockWrites to complete before allowing
  //               a MockRead to complete.
  // |reads| the list of MockRead completions.
  // |writes| the list of MockWrite completions.
  // Note: All MockReads and MockWrites must be async.
  // Note: The MockRead and MockWrite lists musts end with a EOF
  //       e.g. a MockRead(true, 0, 0);
  DelayedSocketData(int write_delay,
                    MockRead* reads, size_t reads_count,
                    MockWrite* writes, size_t writes_count);

  // |connect| the result for the connect phase.
  // |reads| the list of MockRead completions.
  // |write_delay| the number of MockWrites to complete before allowing
  //               a MockRead to complete.
  // |writes| the list of MockWrite completions.
  // Note: All MockReads and MockWrites must be async.
  // Note: The MockRead and MockWrite lists musts end with a EOF
  //       e.g. a MockRead(true, 0, 0);
  DelayedSocketData(const MockConnect& connect, int write_delay,
                    MockRead* reads, size_t reads_count,
                    MockWrite* writes, size_t writes_count);
  virtual ~DelayedSocketData();

  void ForceNextRead();

  // StaticSocketDataProvider:
  virtual MockRead GetNextRead() OVERRIDE;
  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void CompleteRead() OVERRIDE;

 private:
  int write_delay_;
  ScopedRunnableMethodFactory<DelayedSocketData> factory_;
};

// A DataProvider where the reads are ordered.
// If a read is requested before its sequence number is reached, we return an
// ERR_IO_PENDING (that way we don't have to explicitly add a MockRead just to
// wait).
// The sequence number is incremented on every read and write operation.
// The message loop may be interrupted by setting the high bit of the sequence
// number in the MockRead's sequence number.  When that MockRead is reached,
// we post a Quit message to the loop.  This allows us to interrupt the reading
// of data before a complete message has arrived, and provides support for
// testing server push when the request is issued while the response is in the
// middle of being received.
class OrderedSocketData : public StaticSocketDataProvider,
                          public base::RefCounted<OrderedSocketData> {
 public:
  // |reads| the list of MockRead completions.
  // |writes| the list of MockWrite completions.
  // Note: All MockReads and MockWrites must be async.
  // Note: The MockRead and MockWrite lists musts end with a EOF
  //       e.g. a MockRead(true, 0, 0);
  OrderedSocketData(MockRead* reads, size_t reads_count,
                    MockWrite* writes, size_t writes_count);

  // |connect| the result for the connect phase.
  // |reads| the list of MockRead completions.
  // |writes| the list of MockWrite completions.
  // Note: All MockReads and MockWrites must be async.
  // Note: The MockRead and MockWrite lists musts end with a EOF
  //       e.g. a MockRead(true, 0, 0);
  OrderedSocketData(const MockConnect& connect,
                    MockRead* reads, size_t reads_count,
                    MockWrite* writes, size_t writes_count);

  void SetOldCompletionCallback(OldCompletionCallback* callback) {
    callback_ = callback;
  }

  // Posts a quit message to the current message loop, if one is running.
  void EndLoop();

  // StaticSocketDataProvider:
  virtual MockRead GetNextRead() OVERRIDE;
  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void CompleteRead() OVERRIDE;

 private:
  friend class base::RefCounted<OrderedSocketData>;
  virtual ~OrderedSocketData();

  int sequence_number_;
  int loop_stop_stage_;
  OldCompletionCallback* callback_;
  bool blocked_;
  ScopedRunnableMethodFactory<OrderedSocketData> factory_;
};

class DeterministicMockTCPClientSocket;

// This class gives the user full control over the network activity,
// specifically the timing of the COMPLETION of I/O operations.  Regardless of
// the order in which I/O operations are initiated, this class ensures that they
// complete in the correct order.
//
// Network activity is modeled as a sequence of numbered steps which is
// incremented whenever an I/O operation completes.  This can happen under two
// different circumstances:
//
// 1) Performing a synchronous I/O operation.  (Invoking Read() or Write()
//    when the corresponding MockRead or MockWrite is marked !async).
// 2) Running the Run() method of this class.  The run method will invoke
//    the current MessageLoop, running all pending events, and will then
//    invoke any pending IO callbacks.
//
// In addition, this class allows for I/O processing to "stop" at a specified
// step, by calling SetStop(int) or StopAfter(int).  Initiating an I/O operation
// by calling Read() or Write() while stopped is permitted if the operation is
// asynchronous.  It is an error to perform synchronous I/O while stopped.
//
// When creating the MockReads and MockWrites, note that the sequence number
// refers to the number of the step in which the I/O will complete.  In the
// case of synchronous I/O, this will be the same step as the I/O is initiated.
// However, in the case of asynchronous I/O, this I/O may be initiated in
// a much earlier step. Furthermore, when the a Read() or Write() is separated
// from its completion by other Read() or Writes()'s, it can not be marked
// synchronous.  If it is, ERR_UNUEXPECTED will be returned indicating that a
// synchronous Read() or Write() could not be completed synchronously because of
// the specific ordering constraints.
//
// Sequence numbers are preserved across both reads and writes. There should be
// no gaps in sequence numbers, and no repeated sequence numbers. i.e.
//  MockRead reads[] = {
//    MockRead(false, "first read", length, 0)   // sync
//    MockRead(true, "second read", length, 2)   // async
//  };
//  MockWrite writes[] = {
//    MockWrite(true, "first write", length, 1),    // async
//    MockWrite(false, "second write", length, 3),  // sync
//  };
//
// Example control flow:
// Read() is called.  The current step is 0.  The first available read is
// synchronous, so the call to Read() returns length.  The current step is
// now 1.  Next, Read() is called again.  The next available read can
// not be completed until step 2, so Read() returns ERR_IO_PENDING.  The current
// step is still 1.  Write is called().  The first available write is able to
// complete in this step, but is marked asynchronous.  Write() returns
// ERR_IO_PENDING.  The current step is still 1.  At this point RunFor(1) is
// called which will cause the write callback to be invoked, and will then
// stop.  The current state is now 2.  RunFor(1) is called again, which
// causes the read callback to be invoked, and will then stop.  Then current
// step is 2.  Write() is called again.  Then next available write is
// synchronous so the call to Write() returns length.
//
// For examples of how to use this class, see:
//   deterministic_socket_data_unittests.cc
class DeterministicSocketData : public StaticSocketDataProvider,
    public base::RefCounted<DeterministicSocketData> {
 public:
  // |reads| the list of MockRead completions.
  // |writes| the list of MockWrite completions.
  DeterministicSocketData(MockRead* reads, size_t reads_count,
                          MockWrite* writes, size_t writes_count);
  virtual ~DeterministicSocketData();

  // Consume all the data up to the give stop point (via SetStop()).
  void Run();

  // Set the stop point to be |steps| from now, and then invoke Run().
  void RunFor(int steps);

  // Stop at step |seq|, which must be in the future.
  virtual void SetStop(int seq);

  // Stop |seq| steps after the current step.
  virtual void StopAfter(int seq);
  bool stopped() const { return stopped_; }
  void SetStopped(bool val) { stopped_ = val; }
  MockRead& current_read() { return current_read_; }
  MockRead& current_write() { return current_write_; }
  int sequence_number() const { return sequence_number_; }
  void set_socket(base::WeakPtr<DeterministicMockTCPClientSocket> socket) {
    socket_ = socket;
  }

  // StaticSocketDataProvider:

  // When the socket calls Read(), that calls GetNextRead(), and expects either
  // ERR_IO_PENDING or data.
  virtual MockRead GetNextRead() OVERRIDE;

  // When the socket calls Write(), it always completes synchronously. OnWrite()
  // checks to make sure the written data matches the expected data. The
  // callback will not be invoked until its sequence number is reached.
  virtual MockWriteResult OnWrite(const std::string& data) OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void CompleteRead() OVERRIDE {}

 private:
  // Invoke the read and write callbacks, if the timing is appropriate.
  void InvokeCallbacks();

  void NextStep();

  int sequence_number_;
  MockRead current_read_;
  MockWrite current_write_;
  int stopping_sequence_number_;
  bool stopped_;
  base::WeakPtr<DeterministicMockTCPClientSocket> socket_;
  bool print_debug_;
};

// Holds an array of SocketDataProvider elements.  As Mock{TCP,SSL}StreamSocket
// objects get instantiated, they take their data from the i'th element of this
// array.
template<typename T>
class SocketDataProviderArray {
 public:
  SocketDataProviderArray() : next_index_(0) {
  }

  T* GetNext() {
    DCHECK_LT(next_index_, data_providers_.size());
    return data_providers_[next_index_++];
  }

  void Add(T* data_provider) {
    DCHECK(data_provider);
    data_providers_.push_back(data_provider);
  }

  void ResetNextIndex() {
    next_index_ = 0;
  }

 private:
  // Index of the next |data_providers_| element to use. Not an iterator
  // because those are invalidated on vector reallocation.
  size_t next_index_;

  // SocketDataProviders to be returned.
  std::vector<T*> data_providers_;
};

class MockUDPClientSocket;
class MockTCPClientSocket;
class MockSSLClientSocket;

// ClientSocketFactory which contains arrays of sockets of each type.
// You should first fill the arrays using AddMock{SSL,}Socket. When the factory
// is asked to create a socket, it takes next entry from appropriate array.
// You can use ResetNextMockIndexes to reset that next entry index for all mock
// socket types.
class MockClientSocketFactory : public ClientSocketFactory {
 public:
  MockClientSocketFactory();
  virtual ~MockClientSocketFactory();

  void AddSocketDataProvider(SocketDataProvider* socket);
  void AddSSLSocketDataProvider(SSLSocketDataProvider* socket);
  void ResetNextMockIndexes();

  // Return |index|-th MockTCPClientSocket (starting from 0) that the factory
  // created.
  MockTCPClientSocket* GetMockTCPClientSocket(size_t index) const;

  // Return |index|-th MockSSLClientSocket (starting from 0) that the factory
  // created.
  MockSSLClientSocket* GetMockSSLClientSocket(size_t index) const;

  SocketDataProviderArray<SocketDataProvider>& mock_data() {
    return mock_data_;
  }
  std::vector<MockTCPClientSocket*>& tcp_client_sockets() {
    return tcp_client_sockets_;
  }
  std::vector<MockUDPClientSocket*>& udp_client_sockets() {
    return udp_client_sockets_;
  }

  // ClientSocketFactory
  virtual DatagramClientSocket* CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      NetLog* net_log,
      const NetLog::Source& source) OVERRIDE;
  virtual StreamSocket* CreateTransportClientSocket(
      const AddressList& addresses,
      NetLog* net_log,
      const NetLog::Source& source) OVERRIDE;
  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocketHandle* transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config,
      SSLHostInfo* ssl_host_info,
      const SSLClientSocketContext& context) OVERRIDE;
  virtual void ClearSSLSessionCache() OVERRIDE;

 private:
  SocketDataProviderArray<SocketDataProvider> mock_data_;
  SocketDataProviderArray<SSLSocketDataProvider> mock_ssl_data_;

  // Store pointers to handed out sockets in case the test wants to get them.
  std::vector<MockUDPClientSocket*> udp_client_sockets_;
  std::vector<MockTCPClientSocket*> tcp_client_sockets_;
  std::vector<MockSSLClientSocket*> ssl_client_sockets_;
};

class MockClientSocket : public net::SSLClientSocket {
 public:
  explicit MockClientSocket(net::NetLog* net_log);

  // Socket methods:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback) = 0;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback) = 0;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

  // StreamSocket methods:
  virtual int Connect(net::OldCompletionCallback* callback) = 0;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(AddressList* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE {}
  virtual void SetOmniboxSpeculation() OVERRIDE {}

  // SSLClientSocket methods:
  virtual void GetSSLInfo(net::SSLInfo* ssl_info) OVERRIDE;
  virtual void GetSSLCertRequestInfo(
      net::SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual int ExportKeyingMaterial(const base::StringPiece& label,
                                   const base::StringPiece& context,
                                   unsigned char *out,
                                   unsigned int outlen) OVERRIDE;
  virtual NextProtoStatus GetNextProto(std::string* proto,
                                       std::string* server_protos) OVERRIDE;

 protected:
  virtual ~MockClientSocket();
  void RunCallbackAsync(net::OldCompletionCallback* callback, int result);
  void RunCallback(net::OldCompletionCallback*, int result);

  ScopedRunnableMethodFactory<MockClientSocket> method_factory_;

  // True if Connect completed successfully and Disconnect hasn't been called.
  bool connected_;

  net::BoundNetLog net_log_;
};

class MockTCPClientSocket : public MockClientSocket, public AsyncSocket {
 public:
  MockTCPClientSocket(const net::AddressList& addresses, net::NetLog* net_log,
                      net::SocketDataProvider* socket);

  net::AddressList addresses() const { return addresses_; }

  // Socket methods:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback) OVERRIDE;

  // StreamSocket methods:
  virtual int Connect(net::OldCompletionCallback* callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(AddressList* address) const OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;

  // AsyncSocket:
  virtual void OnReadComplete(const MockRead& data) OVERRIDE;

 private:
  int CompleteRead();

  net::AddressList addresses_;

  net::SocketDataProvider* data_;
  int read_offset_;
  int num_bytes_read_;
  net::MockRead read_data_;
  bool need_read_data_;

  // True if the peer has closed the connection.  This allows us to simulate
  // the recv(..., MSG_PEEK) call in the IsConnectedAndIdle method of the real
  // TCPClientSocket.
  bool peer_closed_connection_;

  // While an asynchronous IO is pending, we save our user-buffer state.
  net::IOBuffer* pending_buf_;
  int pending_buf_len_;
  net::OldCompletionCallback* pending_callback_;
  bool was_used_to_convey_data_;
};

class DeterministicMockTCPClientSocket : public MockClientSocket,
    public AsyncSocket,
    public base::SupportsWeakPtr<DeterministicMockTCPClientSocket> {
 public:
  DeterministicMockTCPClientSocket(net::NetLog* net_log,
      net::DeterministicSocketData* data);
  virtual ~DeterministicMockTCPClientSocket();

  bool write_pending() const { return write_pending_; }
  bool read_pending() const { return read_pending_; }

  void CompleteWrite();
  int CompleteRead();

  // Socket:
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback) OVERRIDE;
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback) OVERRIDE;

  // StreamSocket:
  virtual int Connect(net::OldCompletionCallback* callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;

  // AsyncSocket:
  virtual void OnReadComplete(const MockRead& data) OVERRIDE;

 private:
  bool write_pending_;
  net::OldCompletionCallback* write_callback_;
  int write_result_;

  net::MockRead read_data_;

  net::IOBuffer* read_buf_;
  int read_buf_len_;
  bool read_pending_;
  net::OldCompletionCallback* read_callback_;
  net::DeterministicSocketData* data_;
  bool was_used_to_convey_data_;
};

class MockSSLClientSocket : public MockClientSocket, public AsyncSocket {
 public:
  MockSSLClientSocket(
      net::ClientSocketHandle* transport_socket,
      const HostPortPair& host_and_port,
      const net::SSLConfig& ssl_config,
      SSLHostInfo* ssl_host_info,
      net::SSLSocketDataProvider* socket);
  virtual ~MockSSLClientSocket();

  // Socket methods:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback) OVERRIDE;

  // StreamSocket methods:
  virtual int Connect(net::OldCompletionCallback* callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;

  // SSLClientSocket methods:
  virtual void GetSSLInfo(net::SSLInfo* ssl_info) OVERRIDE;
  virtual void GetSSLCertRequestInfo(
      net::SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual NextProtoStatus GetNextProto(std::string* proto,
                                       std::string* server_protos) OVERRIDE;
  virtual bool was_npn_negotiated() const OVERRIDE;
  virtual bool set_was_npn_negotiated(bool negotiated) OVERRIDE;

  // This MockSocket does not implement the manual async IO feature.
  virtual void OnReadComplete(const MockRead& data) OVERRIDE;

 private:
  class ConnectCallback;

  scoped_ptr<ClientSocketHandle> transport_;
  net::SSLSocketDataProvider* data_;
  bool is_npn_state_set_;
  bool new_npn_value_;
  bool was_used_to_convey_data_;
};

class MockUDPClientSocket : public DatagramClientSocket,
    public AsyncSocket {
 public:
  MockUDPClientSocket(SocketDataProvider* data, net::NetLog* net_log);
  virtual ~MockUDPClientSocket();

  // Socket interface
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

  // DatagramSocket interface
  virtual void Close() OVERRIDE;
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;

  // DatagramClientSocket interface
  virtual int Connect(const IPEndPoint& address) OVERRIDE;

  // AsyncSocket interface
  virtual void OnReadComplete(const MockRead& data) OVERRIDE;

 private:
  int CompleteRead();

  void RunCallbackAsync(net::OldCompletionCallback* callback, int result);
  void RunCallback(net::OldCompletionCallback* callback, int result);

  bool connected_;
  SocketDataProvider* data_;
  int read_offset_;
  net::MockRead read_data_;
  bool need_read_data_;

  // While an asynchronous IO is pending, we save our user-buffer state.
  net::IOBuffer* pending_buf_;
  int pending_buf_len_;
  net::OldCompletionCallback* pending_callback_;

  BoundNetLog net_log_;

  ScopedRunnableMethodFactory<MockUDPClientSocket> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockUDPClientSocket);
};

class TestSocketRequest : public CallbackRunner< Tuple1<int> > {
 public:
  TestSocketRequest(
      std::vector<TestSocketRequest*>* request_order,
      size_t* completion_count);
  virtual ~TestSocketRequest();

  ClientSocketHandle* handle() { return &handle_; }

  int WaitForResult();
  virtual void RunWithParams(const Tuple1<int>& params) OVERRIDE;

 private:
  ClientSocketHandle handle_;
  std::vector<TestSocketRequest*>* request_order_;
  size_t* completion_count_;
  TestOldCompletionCallback callback_;
};

class ClientSocketPoolTest {
 public:
  enum KeepAlive {
    KEEP_ALIVE,

    // A socket will be disconnected in addition to handle being reset.
    NO_KEEP_ALIVE,
  };

  static const int kIndexOutOfBounds;
  static const int kRequestNotFound;

  ClientSocketPoolTest();
  ~ClientSocketPoolTest();

  template <typename PoolType, typename SocketParams>
  int StartRequestUsingPool(PoolType* socket_pool,
                            const std::string& group_name,
                            RequestPriority priority,
                            const scoped_refptr<SocketParams>& socket_params) {
    DCHECK(socket_pool);
    TestSocketRequest* request = new TestSocketRequest(&request_order_,
                                                       &completion_count_);
    requests_.push_back(request);
    int rv = request->handle()->Init(
        group_name, socket_params, priority, request,
        socket_pool, BoundNetLog());
    if (rv != ERR_IO_PENDING)
      request_order_.push_back(request);
    return rv;
  }

  // Provided there were n requests started, takes |index| in range 1..n
  // and returns order in which that request completed, in range 1..n,
  // or kIndexOutOfBounds if |index| is out of bounds, or kRequestNotFound
  // if that request did not complete (for example was canceled).
  int GetOrderOfRequest(size_t index) const;

  // Resets first initialized socket handle from |requests_|. If found such
  // a handle, returns true.
  bool ReleaseOneConnection(KeepAlive keep_alive);

  // Releases connections until there is nothing to release.
  void ReleaseAllConnections(KeepAlive keep_alive);

  TestSocketRequest* request(int i) { return requests_[i]; }
  size_t requests_size() const { return requests_.size(); }
  ScopedVector<TestSocketRequest>* requests() { return &requests_; }
  size_t completion_count() const { return completion_count_; }

 private:
  ScopedVector<TestSocketRequest> requests_;
  std::vector<TestSocketRequest*> request_order_;
  size_t completion_count_;
};

class MockTransportClientSocketPool : public TransportClientSocketPool {
 public:
  class MockConnectJob {
   public:
    MockConnectJob(StreamSocket* socket, ClientSocketHandle* handle,
                   OldCompletionCallback* callback);
    ~MockConnectJob();

    int Connect();
    bool CancelHandle(const ClientSocketHandle* handle);

   private:
    void OnConnect(int rv);

    scoped_ptr<StreamSocket> socket_;
    ClientSocketHandle* handle_;
    OldCompletionCallback* user_callback_;
    OldCompletionCallbackImpl<MockConnectJob> connect_callback_;

    DISALLOW_COPY_AND_ASSIGN(MockConnectJob);
  };

  MockTransportClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      ClientSocketPoolHistograms* histograms,
      ClientSocketFactory* socket_factory);

  virtual ~MockTransportClientSocketPool();

  int release_count() const { return release_count_; }
  int cancel_count() const { return cancel_count_; }

  // TransportClientSocketPool methods.
  virtual int RequestSocket(const std::string& group_name,
                            const void* socket_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            OldCompletionCallback* callback,
                            const BoundNetLog& net_log) OVERRIDE;

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) OVERRIDE;
  virtual void ReleaseSocket(const std::string& group_name,
                             StreamSocket* socket, int id) OVERRIDE;

 private:
  ClientSocketFactory* client_socket_factory_;
  ScopedVector<MockConnectJob> job_list_;
  int release_count_;
  int cancel_count_;

  DISALLOW_COPY_AND_ASSIGN(MockTransportClientSocketPool);
};

class DeterministicMockClientSocketFactory : public ClientSocketFactory {
 public:
  DeterministicMockClientSocketFactory();
  virtual ~DeterministicMockClientSocketFactory();

  void AddSocketDataProvider(DeterministicSocketData* socket);
  void AddSSLSocketDataProvider(SSLSocketDataProvider* socket);
  void ResetNextMockIndexes();

  // Return |index|-th MockSSLClientSocket (starting from 0) that the factory
  // created.
  MockSSLClientSocket* GetMockSSLClientSocket(size_t index) const;

  SocketDataProviderArray<DeterministicSocketData>& mock_data() {
    return mock_data_;
  }
  std::vector<DeterministicMockTCPClientSocket*>& tcp_client_sockets() {
    return tcp_client_sockets_;
  }

  // ClientSocketFactory
  virtual DatagramClientSocket* CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      NetLog* net_log,
      const NetLog::Source& source) OVERRIDE;
  virtual StreamSocket* CreateTransportClientSocket(
      const AddressList& addresses,
      NetLog* net_log,
      const NetLog::Source& source) OVERRIDE;
  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocketHandle* transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config,
      SSLHostInfo* ssl_host_info,
      const SSLClientSocketContext& context) OVERRIDE;
  virtual void ClearSSLSessionCache() OVERRIDE;

 private:
  SocketDataProviderArray<DeterministicSocketData> mock_data_;
  SocketDataProviderArray<SSLSocketDataProvider> mock_ssl_data_;

  // Store pointers to handed out sockets in case the test wants to get them.
  std::vector<DeterministicMockTCPClientSocket*> tcp_client_sockets_;
  std::vector<MockSSLClientSocket*> ssl_client_sockets_;
};

class MockSOCKSClientSocketPool : public SOCKSClientSocketPool {
 public:
  MockSOCKSClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      ClientSocketPoolHistograms* histograms,
      TransportClientSocketPool* transport_pool);

  virtual ~MockSOCKSClientSocketPool();

  // SOCKSClientSocketPool methods.
  virtual int RequestSocket(const std::string& group_name,
                            const void* socket_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            OldCompletionCallback* callback,
                            const BoundNetLog& net_log) OVERRIDE;

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) OVERRIDE;
  virtual void ReleaseSocket(const std::string& group_name,
                             StreamSocket* socket, int id) OVERRIDE;

 private:
  TransportClientSocketPool* const transport_pool_;

  DISALLOW_COPY_AND_ASSIGN(MockSOCKSClientSocketPool);
};

// Constants for a successful SOCKS v5 handshake.
extern const char kSOCKS5GreetRequest[];
extern const int kSOCKS5GreetRequestLength;

extern const char kSOCKS5GreetResponse[];
extern const int kSOCKS5GreetResponseLength;

extern const char kSOCKS5OkRequest[];
extern const int kSOCKS5OkRequestLength;

extern const char kSOCKS5OkResponse[];
extern const int kSOCKS5OkResponseLength;

}  // namespace net

#endif  // NET_SOCKET_SOCKET_TEST_UTIL_H_
