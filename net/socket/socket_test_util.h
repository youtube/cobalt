// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_TEST_UTIL_H_
#define NET_SOCKET_SOCKET_TEST_UTIL_H_

#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
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
#include "net/socket/tcp_client_socket_pool.h"
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

class ClientSocket;
class HttpRequestHeaders;
class HttpResponseHeaders;
class MockClientSocket;
class SSLClientSocket;

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
  // that it will be called via the MockClientSocket::OnReadComplete()
  // function at a later time.
  virtual MockRead GetNextRead() = 0;
  virtual MockWriteResult OnWrite(const std::string& data) = 0;
  virtual void Reset() = 0;

  // Accessor for the socket which is using the SocketDataProvider.
  MockClientSocket* socket() { return socket_; }
  void set_socket(MockClientSocket* socket) { socket_ = socket; }

  MockConnect connect_data() const { return connect_; }
  void set_connect_data(const MockConnect& connect) { connect_ = connect; }

 private:
  MockConnect connect_;
  MockClientSocket* socket_;

  DISALLOW_COPY_AND_ASSIGN(SocketDataProvider);
};

// SocketDataProvider which responds based on static tables of mock reads and
// writes.
class StaticSocketDataProvider : public SocketDataProvider {
 public:
  StaticSocketDataProvider() : reads_(NULL), read_index_(0), read_count_(0),
      writes_(NULL), write_index_(0), write_count_(0) {}
  StaticSocketDataProvider(MockRead* reads, size_t reads_count,
                           MockWrite* writes, size_t writes_count)
      : reads_(reads),
        read_index_(0),
        read_count_(reads_count),
        writes_(writes),
        write_index_(0),
        write_count_(writes_count) {
  }

  // SocketDataProvider methods:
  virtual MockRead GetNextRead();
  virtual MockWriteResult OnWrite(const std::string& data);
  virtual void Reset();

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

  // SocketDataProvider methods:
  virtual MockRead GetNextRead();
  virtual MockWriteResult OnWrite(const std::string& data) = 0;
  virtual void Reset();

  int short_read_limit() const { return short_read_limit_; }
  void set_short_read_limit(int limit) { short_read_limit_ = limit; }

  void allow_unconsumed_reads(bool allow) { allow_unconsumed_reads_ = allow; }

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
  SSLSocketDataProvider(bool async, int result)
      : connect(async, result),
        next_proto_status(SSLClientSocket::kNextProtoUnsupported),
        was_npn_negotiated(false) { }

  MockConnect connect;
  SSLClientSocket::NextProtoStatus next_proto_status;
  std::string next_proto;
  bool was_npn_negotiated;
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

  virtual MockRead GetNextRead();
  virtual MockWriteResult OnWrite(const std::string& data);
  virtual void Reset();
  void CompleteRead();

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

  virtual MockRead GetNextRead();
  virtual MockWriteResult OnWrite(const std::string& data);
  virtual void Reset();
  void SetCompletionCallback(CompletionCallback* callback) {
    callback_ = callback;
  }

  // Posts a quit message to the current message loop, if one is running.
  void EndLoop();

  void CompleteRead();

 private:
  int sequence_number_;
  int loop_stop_stage_;
  CompletionCallback* callback_;
  bool blocked_;
  ScopedRunnableMethodFactory<OrderedSocketData> factory_;
};

// Holds an array of SocketDataProvider elements.  As Mock{TCP,SSL}ClientSocket
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

class MockTCPClientSocket;
class MockSSLClientSocket;

// ClientSocketFactory which contains arrays of sockets of each type.
// You should first fill the arrays using AddMock{SSL,}Socket. When the factory
// is asked to create a socket, it takes next entry from appropriate array.
// You can use ResetNextMockIndexes to reset that next entry index for all mock
// socket types.
class MockClientSocketFactory : public ClientSocketFactory {
 public:
  void AddSocketDataProvider(SocketDataProvider* socket);
  void AddSSLSocketDataProvider(SSLSocketDataProvider* socket);
  void ResetNextMockIndexes();

  // Return |index|-th MockTCPClientSocket (starting from 0) that the factory
  // created.
  MockTCPClientSocket* GetMockTCPClientSocket(size_t index) const;

  // Return |index|-th MockSSLClientSocket (starting from 0) that the factory
  // created.
  MockSSLClientSocket* GetMockSSLClientSocket(size_t index) const;

  // ClientSocketFactory
  virtual ClientSocket* CreateTCPClientSocket(const AddressList& addresses,
                                              NetLog* net_log);
  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocketHandle* transport_socket,
      const std::string& hostname,
      const SSLConfig& ssl_config);

 private:
  SocketDataProviderArray<SocketDataProvider> mock_data_;
  SocketDataProviderArray<SSLSocketDataProvider> mock_ssl_data_;

  // Store pointers to handed out sockets in case the test wants to get them.
  std::vector<MockTCPClientSocket*> tcp_client_sockets_;
  std::vector<MockSSLClientSocket*> ssl_client_sockets_;
};

class MockClientSocket : public net::SSLClientSocket {
 public:
  explicit MockClientSocket(net::NetLog* net_log);

  // ClientSocket methods:
  virtual int Connect(net::CompletionCallback* callback) = 0;
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual int GetPeerAddress(AddressList* address) const;
  virtual const BoundNetLog& NetLog() const { return net_log_;}

  // SSLClientSocket methods:
  virtual void GetSSLInfo(net::SSLInfo* ssl_info);
  virtual void GetSSLCertRequestInfo(
      net::SSLCertRequestInfo* cert_request_info);
  virtual NextProtoStatus GetNextProto(std::string* proto);

  // Socket methods:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback) = 0;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback) = 0;
  virtual bool SetReceiveBufferSize(int32 size) { return true; }
  virtual bool SetSendBufferSize(int32 size) { return true; }

  // If an async IO is pending because the SocketDataProvider returned
  // ERR_IO_PENDING, then the MockClientSocket waits until this OnReadComplete
  // is called to complete the asynchronous read operation.
  // data.async is ignored, and this read is completed synchronously as
  // part of this call.
  virtual void OnReadComplete(const MockRead& data) = 0;

 protected:
  void RunCallbackAsync(net::CompletionCallback* callback, int result);
  void RunCallback(net::CompletionCallback*, int result);

  ScopedRunnableMethodFactory<MockClientSocket> method_factory_;

  // True if Connect completed successfully and Disconnect hasn't been called.
  bool connected_;

  net::BoundNetLog net_log_;
};

class MockTCPClientSocket : public MockClientSocket {
 public:
  MockTCPClientSocket(const net::AddressList& addresses, net::NetLog* net_log,
                      net::SocketDataProvider* socket);

  // ClientSocket methods:
  virtual int Connect(net::CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const { return IsConnected(); }

  // Socket methods:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);

  virtual void OnReadComplete(const MockRead& data);

  net::AddressList addresses() const { return addresses_; }

 private:
  int CompleteRead();

  net::AddressList addresses_;

  net::SocketDataProvider* data_;
  int read_offset_;
  net::MockRead read_data_;
  bool need_read_data_;

  // True if the peer has closed the connection.  This allows us to simulate
  // the recv(..., MSG_PEEK) call in the IsConnectedAndIdle method of the real
  // TCPClientSocket.
  bool peer_closed_connection_;

  // While an asynchronous IO is pending, we save our user-buffer state.
  net::IOBuffer* pending_buf_;
  int pending_buf_len_;
  net::CompletionCallback* pending_callback_;
};

class MockSSLClientSocket : public MockClientSocket {
 public:
  MockSSLClientSocket(
      net::ClientSocketHandle* transport_socket,
      const std::string& hostname,
      const net::SSLConfig& ssl_config,
      net::SSLSocketDataProvider* socket);
  ~MockSSLClientSocket();

  // ClientSocket methods:
  virtual int Connect(net::CompletionCallback* callback);
  virtual void Disconnect();

  // Socket methods:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);

  // SSLClientSocket methods:
  virtual void GetSSLInfo(net::SSLInfo* ssl_info);
  virtual NextProtoStatus GetNextProto(std::string* proto);
  virtual bool wasNpnNegotiated() const;
  virtual bool setWasNpnNegotiated(bool negotiated);

  // This MockSocket does not implement the manual async IO feature.
  virtual void OnReadComplete(const MockRead& data) { NOTIMPLEMENTED(); }

 private:
  class ConnectCallback;

  scoped_ptr<ClientSocketHandle> transport_;
  net::SSLSocketDataProvider* data_;
  bool is_npn_state_set_;
  bool new_npn_value_;
};

class TestSocketRequest : public CallbackRunner< Tuple1<int> > {
 public:
  TestSocketRequest(
      std::vector<TestSocketRequest*>* request_order,
      size_t* completion_count)
      : request_order_(request_order),
        completion_count_(completion_count) {
    DCHECK(request_order);
    DCHECK(completion_count);
  }

  ClientSocketHandle* handle() { return &handle_; }

  int WaitForResult();
  virtual void RunWithParams(const Tuple1<int>& params);

 private:
  ClientSocketHandle handle_;
  std::vector<TestSocketRequest*>* request_order_;
  size_t* completion_count_;
  TestCompletionCallback callback_;
};

class ClientSocketPoolTest : public testing::Test {
 protected:
  enum KeepAlive {
    KEEP_ALIVE,

    // A socket will be disconnected in addition to handle being reset.
    NO_KEEP_ALIVE,
  };

  static const int kIndexOutOfBounds;
  static const int kRequestNotFound;

  virtual void SetUp();
  virtual void TearDown();

  template <typename PoolType, typename SocketParams>
  int StartRequestUsingPool(const scoped_refptr<PoolType>& socket_pool,
                            const std::string& group_name,
                            RequestPriority priority,
                            const scoped_refptr<SocketParams>& socket_params) {
    DCHECK(socket_pool.get());
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
  int GetOrderOfRequest(size_t index);

  // Resets first initialized socket handle from |requests_|. If found such
  // a handle, returns true.
  bool ReleaseOneConnection(KeepAlive keep_alive);

  // Releases connections until there is nothing to release.
  void ReleaseAllConnections(KeepAlive keep_alive);

  ScopedVector<TestSocketRequest> requests_;
  std::vector<TestSocketRequest*> request_order_;
  size_t completion_count_;
};

class MockTCPClientSocketPool : public TCPClientSocketPool {
 public:
  class MockConnectJob {
   public:
    MockConnectJob(ClientSocket* socket, ClientSocketHandle* handle,
                   CompletionCallback* callback);

    int Connect();
    bool CancelHandle(const ClientSocketHandle* handle);

   private:
    void OnConnect(int rv);

    scoped_ptr<ClientSocket> socket_;
    ClientSocketHandle* handle_;
    CompletionCallback* user_callback_;
    CompletionCallbackImpl<MockConnectJob> connect_callback_;

    DISALLOW_COPY_AND_ASSIGN(MockConnectJob);
  };

  MockTCPClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      const scoped_refptr<ClientSocketPoolHistograms>& histograms,
      ClientSocketFactory* socket_factory);

  int release_count() const { return release_count_; };
  int cancel_count() const { return cancel_count_; };

  // TCPClientSocketPool methods.
  virtual int RequestSocket(const std::string& group_name,
                            const void* socket_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback,
                            const BoundNetLog& net_log);

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle);
  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket, int id);

 protected:
  virtual ~MockTCPClientSocketPool();

 private:
  ClientSocketFactory* client_socket_factory_;
  int release_count_;
  int cancel_count_;
  ScopedVector<MockConnectJob> job_list_;

  DISALLOW_COPY_AND_ASSIGN(MockTCPClientSocketPool);
};

class MockSOCKSClientSocketPool : public SOCKSClientSocketPool {
 public:
  MockSOCKSClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      const scoped_refptr<ClientSocketPoolHistograms>& histograms,
      const scoped_refptr<TCPClientSocketPool>& tcp_pool);

  // SOCKSClientSocketPool methods.
  virtual int RequestSocket(const std::string& group_name,
                            const void* socket_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback,
                            const BoundNetLog& net_log);

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle);
  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket, int id);

 protected:
  virtual ~MockSOCKSClientSocketPool();

 private:
  const scoped_refptr<TCPClientSocketPool> tcp_pool_;

  DISALLOW_COPY_AND_ASSIGN(MockSOCKSClientSocketPool);
};

struct MockHttpAuthControllerData {
  MockHttpAuthControllerData(std::string header) : auth_header(header) {}

  std::string auth_header;
};

class MockHttpAuthController : public HttpAuthController {
 public:
  MockHttpAuthController();
  void SetMockAuthControllerData(struct MockHttpAuthControllerData* data,
                                 size_t data_length);

  // HttpAuthController methods.
  virtual int MaybeGenerateAuthToken(const HttpRequestInfo* request,
                                     CompletionCallback* callback,
                                     const BoundNetLog& net_log);
  virtual void AddAuthorizationHeader(
      HttpRequestHeaders* authorization_headers);
  virtual int HandleAuthChallenge(scoped_refptr<HttpResponseHeaders> headers,
                                  bool do_not_send_server_auth,
                                  bool establishing_tunnel,
                                  const BoundNetLog& net_log);
  virtual void ResetAuth(const std::wstring& username,
                         const std::wstring& password);
  virtual bool HaveAuthHandler() const;
  virtual bool HaveAuth() const;

 private:
  virtual ~MockHttpAuthController() {}
  const struct MockHttpAuthControllerData& CurrentData() const {
    DCHECK(data_index_ < data_count_);
    return data_[data_index_];
  }

  MockHttpAuthControllerData* data_;
  size_t data_index_;
  size_t data_count_;
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
