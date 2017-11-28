// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include "net/base/address_list.h"
#include "net/base/cert_test_util.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/mock_cert_verifier.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/ssl_config_service.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/base/test_root_certs.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

const net::SSLConfig kDefaultSSLConfig;

// WrappedStreamSocket is a base class that wraps an existing StreamSocket,
// forwarding the Socket and StreamSocket interfaces to the underlying
// transport.
// This is to provide a common base class for subclasses to override specific
// StreamSocket methods for testing, while still communicating with a 'real'
// StreamSocket.
class WrappedStreamSocket : public net::StreamSocket {
 public:
  explicit WrappedStreamSocket(scoped_ptr<net::StreamSocket> transport)
      : transport_(transport.Pass()) {
  }
  virtual ~WrappedStreamSocket() {}

  // StreamSocket implementation:
  virtual int Connect(const net::CompletionCallback& callback) override {
    return transport_->Connect(callback);
  }
  virtual void Disconnect() override {
    transport_->Disconnect();
  }
  virtual bool IsConnected() const override {
    return transport_->IsConnected();
  }
  virtual bool IsConnectedAndIdle() const override {
    return transport_->IsConnectedAndIdle();
  }
  virtual int GetPeerAddress(net::IPEndPoint* address) const override {
    return transport_->GetPeerAddress(address);
  }
  virtual int GetLocalAddress(net::IPEndPoint* address) const override {
    return transport_->GetLocalAddress(address);
  }
  virtual const net::BoundNetLog& NetLog() const override {
    return transport_->NetLog();
  }
  virtual void SetSubresourceSpeculation() override {
    transport_->SetSubresourceSpeculation();
  }
  virtual void SetOmniboxSpeculation() override {
    transport_->SetOmniboxSpeculation();
  }
  virtual bool WasEverUsed() const override {
    return transport_->WasEverUsed();
  }
  virtual bool UsingTCPFastOpen() const override {
    return transport_->UsingTCPFastOpen();
  }
  virtual bool WasNpnNegotiated() const override {
    return transport_->WasNpnNegotiated();
  }
  virtual net::NextProto GetNegotiatedProtocol() const override {
    return transport_->GetNegotiatedProtocol();
  }
  virtual bool GetSSLInfo(net::SSLInfo* ssl_info) override {
    return transport_->GetSSLInfo(ssl_info);
  }

  // Socket implementation:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) override {
    return transport_->Read(buf, buf_len, callback);
  }
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) override {
    return transport_->Write(buf, buf_len, callback);
  }
  virtual bool SetReceiveBufferSize(int32 size) override {
    return transport_->SetReceiveBufferSize(size);
  }
  virtual bool SetSendBufferSize(int32 size) override {
    return transport_->SetSendBufferSize(size);
  }

 protected:
  scoped_ptr<net::StreamSocket> transport_;
};

// ReadBufferingStreamSocket is a wrapper for an existing StreamSocket that
// will ensure a certain amount of data is internally buffered before
// satisfying a Read() request. It exists to mimic OS-level internal
// buffering, but in a way to guarantee that X number of bytes will be
// returned to callers of Read(), regardless of how quickly the OS receives
// them from the TestServer.
class ReadBufferingStreamSocket : public WrappedStreamSocket {
 public:
  explicit ReadBufferingStreamSocket(scoped_ptr<net::StreamSocket> transport);
  virtual ~ReadBufferingStreamSocket() {}

  // Socket implementation:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) override;

  // Sets the internal buffer to |size|. This must not be greater than
  // the largest value supplied to Read() - that is, it does not handle
  // having "leftovers" at the end of Read().
  // Each call to Read() will be prevented from completion until at least
  // |size| data has been read.
  // Set to 0 to turn off buffering, causing Read() to transparently
  // read via the underlying transport.
  void SetBufferSize(int size);

 private:
  enum State {
    STATE_NONE,
    STATE_READ,
    STATE_READ_COMPLETE,
  };

  int DoLoop(int result);
  int DoRead();
  int DoReadComplete(int result);
  void OnReadCompleted(int result);

  State state_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
  int buffer_size_;

  scoped_refptr<net::IOBuffer> user_read_buf_;
  net::CompletionCallback user_read_callback_;
};

ReadBufferingStreamSocket::ReadBufferingStreamSocket(
    scoped_ptr<net::StreamSocket> transport)
    : WrappedStreamSocket(transport.Pass()),
      read_buffer_(new net::GrowableIOBuffer()),
      buffer_size_(0) {
}

void ReadBufferingStreamSocket::SetBufferSize(int size) {
  DCHECK(!user_read_buf_);
  buffer_size_ = size;
  read_buffer_->SetCapacity(size);
}

int ReadBufferingStreamSocket::Read(net::IOBuffer* buf,
                                    int buf_len,
                                    const net::CompletionCallback& callback) {
  if (buffer_size_ == 0)
    return transport_->Read(buf, buf_len, callback);

  if (buf_len < buffer_size_)
    return net::ERR_UNEXPECTED;

  state_ = STATE_READ;
  user_read_buf_ = buf;
  int result = DoLoop(net::OK);
  if (result == net::ERR_IO_PENDING)
    user_read_callback_ = callback;
  else
    user_read_buf_ = NULL;
  return result;
}

int ReadBufferingStreamSocket::DoLoop(int result) {
  int rv = result;
  do {
    State current_state = state_;
    state_ = STATE_NONE;
    switch (current_state) {
      case STATE_READ:
        rv = DoRead();
        break;
      case STATE_READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case STATE_NONE:
      default:
        NOTREACHED() << "Unexpected state: " << current_state;
        rv = net::ERR_UNEXPECTED;
        break;
    }
  } while (rv != net::ERR_IO_PENDING && state_ != STATE_NONE);
  return rv;
}

int ReadBufferingStreamSocket::DoRead() {
  state_ = STATE_READ_COMPLETE;
  int rv = transport_->Read(
      read_buffer_,
      read_buffer_->RemainingCapacity(),
      base::Bind(&ReadBufferingStreamSocket::OnReadCompleted,
                 base::Unretained(this)));
  return rv;
}

int ReadBufferingStreamSocket::DoReadComplete(int result) {
  state_ = STATE_NONE;
  if (result <= 0)
    return result;

  read_buffer_->set_offset(read_buffer_->offset() + result);
  if (read_buffer_->RemainingCapacity() > 0) {
    state_ = STATE_READ;
    return net::OK;
  }

  memcpy(user_read_buf_->data(), read_buffer_->StartOfBuffer(),
         read_buffer_->capacity());
  read_buffer_->set_offset(0);
  return read_buffer_->capacity();
}

void ReadBufferingStreamSocket::OnReadCompleted(int result) {
  result = DoLoop(result);
  if (result == net::ERR_IO_PENDING)
    return;

  user_read_buf_ = NULL;
  base::ResetAndReturn(&user_read_callback_).Run(result);
}

// Simulates synchronously receiving an error during Read() or Write()
class SynchronousErrorStreamSocket : public WrappedStreamSocket {
 public:
  explicit SynchronousErrorStreamSocket(scoped_ptr<StreamSocket> transport);
  virtual ~SynchronousErrorStreamSocket() {}

  // Socket implementation:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) override;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) override;

  // Sets the the next Read() call to return |error|.
  // If there is already a pending asynchronous read, the configured error
  // will not be returned until that asynchronous read has completed and Read()
  // is called again.
  void SetNextReadError(net::Error error) {
    DCHECK_GE(0, error);
    have_read_error_ = true;
    pending_read_error_ = error;
  }

  // Sets the the next Write() call to return |error|.
  // If there is already a pending asynchronous write, the configured error
  // will not be returned until that asynchronous write has completed and
  // Write() is called again.
  void SetNextWriteError(net::Error error) {
    DCHECK_GE(0, error);
    have_write_error_ = true;
    pending_write_error_ = error;
  }

 private:
  bool have_read_error_;
  int pending_read_error_;

  bool have_write_error_;
  int pending_write_error_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousErrorStreamSocket);
};

SynchronousErrorStreamSocket::SynchronousErrorStreamSocket(
    scoped_ptr<StreamSocket> transport)
    : WrappedStreamSocket(transport.Pass()),
      have_read_error_(false),
      pending_read_error_(net::OK),
      have_write_error_(false),
      pending_write_error_(net::OK) {
}

int SynchronousErrorStreamSocket::Read(
    net::IOBuffer* buf,
    int buf_len,
    const net::CompletionCallback& callback) {
  if (have_read_error_) {
    have_read_error_ = false;
    return pending_read_error_;
  }
  return transport_->Read(buf, buf_len, callback);
}

int SynchronousErrorStreamSocket::Write(
    net::IOBuffer* buf,
    int buf_len,
    const net::CompletionCallback& callback) {
  if (have_write_error_) {
    have_write_error_ = false;
    return pending_write_error_;
  }
  return transport_->Write(buf, buf_len, callback);
}

// FakeBlockingStreamSocket wraps an existing StreamSocket and simulates the
// underlying transport needing to complete things asynchronously in a
// deterministic manner (e.g.: independent of the TestServer and the OS's
// semantics).
class FakeBlockingStreamSocket : public WrappedStreamSocket {
 public:
  explicit FakeBlockingStreamSocket(scoped_ptr<StreamSocket> transport);
  virtual ~FakeBlockingStreamSocket() {}

  // Socket implementation:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) override {
    return read_state_.RunWrappedFunction(buf, buf_len, callback);
  }
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) override {
    return write_state_.RunWrappedFunction(buf, buf_len, callback);
  }

  // Causes the next call to Read() to return ERR_IO_PENDING, not completing
  // (invoking the callback) until UnblockRead() has been called and the
  // underlying transport has completed.
  void SetNextReadShouldBlock() { read_state_.SetShouldBlock(); }
  void UnblockRead() { read_state_.Unblock(); }

  // Causes the next call to Write() to return ERR_IO_PENDING, not completing
  // (invoking the callback) until UnblockWrite() has been called and the
  // underlying transport has completed.
  void SetNextWriteShouldBlock() { write_state_.SetShouldBlock(); }
  void UnblockWrite() { write_state_.Unblock(); }

 private:
  // Tracks the state for simulating a blocking Read/Write operation.
  class BlockingState {
   public:
    // Wrapper for the underlying Socket function to call (ie: Read/Write).
    typedef base::Callback<
        int(net::IOBuffer*, int,
            const net::CompletionCallback&)> WrappedSocketFunction;

    explicit BlockingState(const WrappedSocketFunction& function);
    ~BlockingState() {}

    // Sets the next call to RunWrappedFunction() to block, returning
    // ERR_IO_PENDING and not invoking the user callback until Unblock() is
    // called.
    void SetShouldBlock();

    // Unblocks the currently blocked pending function, invoking the user
    // callback if the results are immediately available.
    // Note: It's not valid to call this unless SetShouldBlock() has been
    // called beforehand.
    void Unblock();

    // Performs the wrapped socket function on the underlying transport. If
    // configured to block via SetShouldBlock(), then |user_callback| will not
    // be invoked until Unblock() has been called.
    int RunWrappedFunction(net::IOBuffer* buf, int len,
                           const net::CompletionCallback& user_callback);

   private:
    // Handles completion from the underlying wrapped socket function.
    void OnCompleted(int result);

    WrappedSocketFunction wrapped_function_;
    bool should_block_;
    bool have_result_;
    int pending_result_;
    net::CompletionCallback user_callback_;
  };

  BlockingState read_state_;
  BlockingState write_state_;

  DISALLOW_COPY_AND_ASSIGN(FakeBlockingStreamSocket);
};

FakeBlockingStreamSocket::FakeBlockingStreamSocket(
    scoped_ptr<StreamSocket> transport)
    : WrappedStreamSocket(transport.Pass()),
      read_state_(base::Bind(&Socket::Read,
                             base::Unretained(transport_.get()))),
      write_state_(base::Bind(&Socket::Write,
                              base::Unretained(transport_.get()))) {
}

FakeBlockingStreamSocket::BlockingState::BlockingState(
    const WrappedSocketFunction& function)
    : wrapped_function_(function),
      should_block_(false),
      have_result_(false),
      pending_result_(net::OK) {
}

void FakeBlockingStreamSocket::BlockingState::SetShouldBlock() {
  DCHECK(!should_block_);
  should_block_ = true;
}

void FakeBlockingStreamSocket::BlockingState::Unblock() {
  DCHECK(should_block_);
  should_block_ = false;

  // If the operation is still pending in the underlying transport, immediately
  // return - OnCompleted() will handle invoking the callback once the transport
  // has completed.
  if (!have_result_)
    return;

  have_result_ = false;

  base::ResetAndReturn(&user_callback_).Run(pending_result_);
}

int FakeBlockingStreamSocket::BlockingState::RunWrappedFunction(
    net::IOBuffer* buf,
    int len,
    const net::CompletionCallback& callback) {

  // The callback to be called by the underlying transport. Either forward
  // directly to the user's callback if not set to block, or intercept it with
  // OnCompleted so that the user's callback is not invoked until Unblock() is
  // called.
  net::CompletionCallback transport_callback =
      !should_block_ ? callback : base::Bind(&BlockingState::OnCompleted,
                                             base::Unretained(this));
  int rv = wrapped_function_.Run(buf, len, transport_callback);
  if (should_block_) {
    user_callback_ = callback;
    // May have completed synchronously.
    have_result_ = (rv != net::ERR_IO_PENDING);
    pending_result_ = rv;
    return net::ERR_IO_PENDING;
  }

  return rv;
}

void FakeBlockingStreamSocket::BlockingState::OnCompleted(int result) {
  if (should_block_) {
    // Store the result so that the callback can be invoked once Unblock() is
    // called.
    have_result_ = true;
    pending_result_ = result;
    return;
  }

  // Otherwise, the Unblock() function was called before the underlying
  // transport completed, so run the user's callback immediately.
  base::ResetAndReturn(&user_callback_).Run(result);
}

// CompletionCallback that will delete the associated net::StreamSocket when
// the callback is invoked.
class DeleteSocketCallback : public net::TestCompletionCallbackBase {
 public:
  explicit DeleteSocketCallback(net::StreamSocket* socket)
      : socket_(socket),
        callback_(base::Bind(&DeleteSocketCallback::OnComplete,
                             base::Unretained(this))) {
  }
  virtual ~DeleteSocketCallback() {}

  const net::CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
   if (socket_) {
     delete socket_;
     socket_ = NULL;
   } else {
     ADD_FAILURE() << "Deleting socket twice";
   }
   SetResult(result);
  }

  net::StreamSocket* socket_;
  net::CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteSocketCallback);
};

}  // namespace

class SSLClientSocketTest : public PlatformTest {
 public:
  SSLClientSocketTest()
      : socket_factory_(net::ClientSocketFactory::GetDefaultFactory()),
        cert_verifier_(new net::MockCertVerifier) {
    cert_verifier_->set_default_result(net::OK);
    context_.cert_verifier = cert_verifier_.get();
  }

 protected:
  net::SSLClientSocket* CreateSSLClientSocket(
      net::StreamSocket* transport_socket,
      const net::HostPortPair& host_and_port,
      const net::SSLConfig& ssl_config) {
    return socket_factory_->CreateSSLClientSocket(transport_socket,
                                                  host_and_port,
                                                  ssl_config,
                                                  context_);
  }

  net::ClientSocketFactory* socket_factory_;
  scoped_ptr<net::MockCertVerifier> cert_verifier_;
  net::SSLClientSocketContext context_;
};

//-----------------------------------------------------------------------------

// LogContainsSSLConnectEndEvent returns true if the given index in the given
// log is an SSL connect end event. The NSS sockets will cork in an attempt to
// merge the first application data record with the Finished message when false
// starting. However, in order to avoid the server timing out the handshake,
// they'll give up waiting for application data and send the Finished after a
// timeout. This means that an SSL connect end event may appear as a socket
// write.
static bool LogContainsSSLConnectEndEvent(
    const net::CapturingNetLog::CapturedEntryList& log, int i) {
  return net::LogContainsEndEvent(log, i, net::NetLog::TYPE_SSL_CONNECT) ||
         net::LogContainsEvent(log, i, net::NetLog::TYPE_SOCKET_BYTES_SENT,
                                net::NetLog::PHASE_NONE);
};

TEST_F(SSLClientSocketTest, Connect) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::CapturingNetLog log;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_F(SSLClientSocketTest, ConnectExpired) {
  net::TestServer::SSLOptions ssl_options(
      net::TestServer::SSLOptions::CERT_EXPIRED);
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              ssl_options,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  cert_verifier_->set_default_result(net::ERR_CERT_DATE_INVALID);

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::CapturingNetLog log;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, rv);

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));
}

TEST_F(SSLClientSocketTest, ConnectMismatched) {
  net::TestServer::SSLOptions ssl_options(
      net::TestServer::SSLOptions::CERT_MISMATCHED_NAME);
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              ssl_options,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  cert_verifier_->set_default_result(net::ERR_CERT_COMMON_NAME_INVALID);

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::CapturingNetLog log;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, rv);

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));
}

// Attempt to connect to a page which requests a client certificate. It should
// return an error code on connect.
TEST_F(SSLClientSocketTest, ConnectClientAuthCertRequested) {
  net::TestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              ssl_options,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::CapturingNetLog log;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  log.GetEntries(&entries);
  // Because we prematurely kill the handshake at CertificateRequest,
  // the server may still send data (notably the ServerHelloDone)
  // after the error is returned. As a result, the SSL_CONNECT may not
  // be the last entry. See http://crbug.com/54445. We use
  // ExpectLogContainsSomewhere instead of
  // LogContainsSSLConnectEndEvent to avoid assuming, e.g., only one
  // extra read instead of two. This occurs before the handshake ends,
  // so the corking logic of LogContainsSSLConnectEndEvent isn't
  // necessary.
  //
  // TODO(davidben): When SSL_RestartHandshakeAfterCertReq in NSS is
  // fixed and we can respond to the first CertificateRequest
  // without closing the socket, add a unit test for sending the
  // certificate. This test may still be useful as we'll want to close
  // the socket on a timeout if the user takes a long time to pick a
  // cert. Related bug: https://bugzilla.mozilla.org/show_bug.cgi?id=542832
  net::ExpectLogContainsSomewhere(
      entries, 0, net::NetLog::TYPE_SSL_CONNECT, net::NetLog::PHASE_END);
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED, rv);
  EXPECT_FALSE(sock->IsConnected());
}

// Connect to a server requesting optional client authentication. Send it a
// null certificate. It should allow the connection.
//
// TODO(davidben): Also test providing an actual certificate.
TEST_F(SSLClientSocketTest, ConnectClientAuthSendNullCert) {
  net::TestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              ssl_options,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::CapturingNetLog log;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  net::SSLConfig ssl_config = kDefaultSSLConfig;
  ssl_config.send_client_cert = true;
  ssl_config.client_cert = NULL;

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  // Our test server accepts certificate-less connections.
  // TODO(davidben): Add a test which requires them and verify the error.
  rv = sock->Connect(callback.callback());

  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

  // We responded to the server's certificate request with a Certificate
  // message with no client certificate in it.  ssl_info.client_cert_sent
  // should be false in this case.
  net::SSLInfo ssl_info;
  sock->GetSSLInfo(&ssl_info);
  EXPECT_FALSE(ssl_info.client_cert_sent);

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// TODO(wtc): Add unit tests for IsConnectedAndIdle:
//   - Server closes an SSL connection (with a close_notify alert message).
//   - Server closes the underlying TCP connection directly.
//   - Server sends data unexpectedly.

TEST_F(SSLClientSocketTest, Read) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(request_buffer, arraysize(request_text) - 1,
                   callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(4096));
  for (;;) {
    rv = sock->Read(buf, 4096, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

// Tests that the SSLClientSocket properly handles when the underlying transport
// synchronously returns an error code - such as if an intermediary terminates
// the socket connection uncleanly.
// This is a regression test for http://crbug.com/238536
TEST_F(SSLClientSocketTest, Read_WithSynchronousError) {
  net::SpawnedTestServer test_server(net::SpawnedTestServer::TYPE_HTTPS,
                                     net::SpawnedTestServer::kLocalhost,
                                     base::FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  scoped_ptr<net::StreamSocket> real_transport(new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source()));
  SynchronousErrorStreamSocket* transport = new SynchronousErrorStreamSocket(
      real_transport.Pass());
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(net::OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  net::SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(arraysize(request_text) - 1);
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::IOBuffer(kRequestTextSize));
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  rv = callback.GetResult(sock->Write(request_buffer, kRequestTextSize,
                                      callback.callback()));
  EXPECT_EQ(kRequestTextSize, rv);

  // Simulate an unclean/forcible shutdown.
  transport->SetNextReadError(net::ERR_CONNECTION_RESET);

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(4096));

  // Note: This test will hang if this bug has regressed. Simply checking that
  // rv != ERR_IO_PENDING is insufficient, as ERR_IO_PENDING is a legitimate
  // result when using a dedicated task runner for NSS.
  rv = callback.GetResult(sock->Read(buf, 4096, callback.callback()));

#if !defined(USE_OPENSSL)
  // NSS records the error exactly
  EXPECT_EQ(net::ERR_CONNECTION_RESET, rv);
#else
  // OpenSSL treats any errors as a simple EOF.
  EXPECT_EQ(0, rv);
#endif
}

// Test the full duplex mode, with Read and Write pending at the same time.
// This test also serves as a regression test for http://crbug.com/29815.
TEST_F(SSLClientSocketTest, Read_FullDuplex) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;  // Used for everything except Write.

  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  // Issue a "hanging" Read first.
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(4096));
  rv = sock->Read(buf, 4096, callback.callback());
  // We haven't written the request, so there should be no response yet.
  ASSERT_EQ(net::ERR_IO_PENDING, rv);

  // Write the request.
  // The request is padded with a User-Agent header to a size that causes the
  // memio circular buffer (4k bytes) in SSLClientSocketNSS to wrap around.
  // This tests the fix for http://crbug.com/29815.
  std::string request_text = "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  for (int i = 0; i < 3770; ++i)
    request_text.push_back('*');
  request_text.append("\r\n\r\n");
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::StringIOBuffer(request_text));

  net::TestCompletionCallback callback2;  // Used for Write only.
  rv = sock->Write(request_buffer, request_text.size(), callback2.callback());
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback2.WaitForResult();
  EXPECT_EQ(static_cast<int>(request_text.size()), rv);

  // Now get the Read result.
  rv = callback.WaitForResult();
  EXPECT_GT(rv, 0);
}

// Attempts to Read() and Write() from an SSLClientSocketNSS in full duplex
// mode when the underlying transport is blocked on sending data. When the
// underlying transport completes due to an error, it should invoke both the
// Read() and Write() callbacks. If the socket is deleted by the Read()
// callback, the Write() callback should not be invoked.
// Regression test for http://crbug.com/232633
TEST_F(SSLClientSocketTest, Read_DeleteWhilePendingFullDuplex) {
  net::SpawnedTestServer test_server(net::SpawnedTestServer::TYPE_HTTPS,
                                     net::SpawnedTestServer::kLocalhost,
                                     base::FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  scoped_ptr<net::StreamSocket> real_transport(new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source()));
  // Note: |error_socket|'s ownership is handed to |transport|, but the pointer
  // is retained in order to configure additional errors.
  SynchronousErrorStreamSocket* error_socket = new SynchronousErrorStreamSocket(
      real_transport.Pass());
  FakeBlockingStreamSocket* transport = new FakeBlockingStreamSocket(
      scoped_ptr<net::StreamSocket>(error_socket));

  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(net::OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  net::SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  net::SSLClientSocket* sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  std::string request_text = "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  request_text.append(20 * 1024, '*');
  request_text.append("\r\n\r\n");
  scoped_refptr<net::DrainableIOBuffer> request_buffer(
      new net::DrainableIOBuffer(new net::StringIOBuffer(request_text),
                                 request_text.size()));

  // Simulate errors being returned from the underlying Read() and Write() ...
  error_socket->SetNextReadError(net::ERR_CONNECTION_RESET);
  error_socket->SetNextWriteError(net::ERR_CONNECTION_RESET);
  // ... but have those errors returned asynchronously. Because the Write() will
  // return first, this will trigger the error.
  transport->SetNextReadShouldBlock();
  transport->SetNextWriteShouldBlock();

  // Enqueue a Read() before calling Write(), which should "hang" due to
  // the ERR_IO_PENDING caused by SetReadShouldBlock() and thus return.
  DeleteSocketCallback read_callback(sock);
  scoped_refptr<net::IOBuffer> read_buf(new net::IOBuffer(4096));
  rv = sock->Read(read_buf, 4096, read_callback.callback());

  // Ensure things didn't complete synchronously, otherwise |sock| is invalid.
  ASSERT_EQ(net::ERR_IO_PENDING, rv);
  ASSERT_FALSE(read_callback.have_result());

#if !defined(USE_OPENSSL)
  // NSS follows a pattern where a call to PR_Write will only consume as
  // much data as it can encode into application data records before the
  // internal memio buffer is full, which should only fill if writing a large
  // amount of data and the underlying transport is blocked. Once this happens,
  // NSS will return (total size of all application data records it wrote) - 1,
  // with the caller expected to resume with the remaining unsent data.
  //
  // This causes SSLClientSocketNSS::Write to return that it wrote some data
  // before it will return ERR_IO_PENDING, so make an extra call to Write() to
  // get the socket in the state needed for the test below.
  //
  // This is not needed for OpenSSL, because for OpenSSL,
  // SSL_MODE_ENABLE_PARTIAL_WRITE is not specified - thus
  // SSLClientSocketOpenSSL::Write() will not return until all of
  // |request_buffer| has been written to the underlying BIO (although not
  // necessarily the underlying transport).
  rv = callback.GetResult(sock->Write(request_buffer,
                                      request_buffer->BytesRemaining(),
                                      callback.callback()));
  ASSERT_LT(0, rv);
  request_buffer->DidConsume(rv);

  // Guard to ensure that |request_buffer| was larger than all of the internal
  // buffers (transport, memio, NSS) along the way - otherwise the next call
  // to Write() will crash with an invalid buffer.
  ASSERT_LT(0, request_buffer->BytesRemaining());
#endif

  // Attempt to write the remaining data. NSS will not be able to consume the
  // application data because the internal buffers are full, while OpenSSL will
  // return that its blocked because the underlying transport is blocked.
  rv = sock->Write(request_buffer, request_buffer->BytesRemaining(),
                   callback.callback());
  ASSERT_EQ(net::ERR_IO_PENDING, rv);
  ASSERT_FALSE(callback.have_result());

  // Now unblock Write(), which will invoke OnSendComplete and (eventually)
  // call the Read() callback, deleting the socket and thus aborting calling
  // the Write() callback.
  transport->UnblockWrite();

  rv = read_callback.WaitForResult();

#if !defined(USE_OPENSSL)
  // NSS records the error exactly.
  EXPECT_EQ(net::ERR_CONNECTION_RESET, rv);
#else
  // OpenSSL treats any errors as a simple EOF.
  EXPECT_EQ(0, rv);
#endif

  // The Write callback should not have been called.
  EXPECT_FALSE(callback.have_result());
}

TEST_F(SSLClientSocketTest, Read_SmallChunks) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(request_buffer, arraysize(request_text) - 1,
                   callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(1));
  for (;;) {
    rv = sock->Read(buf, 1, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(SSLClientSocketTest, Read_Interrupted) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(request_buffer, arraysize(request_text) - 1,
                   callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  // Do a partial read and then exit.  This test should not crash!
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(512));
  rv = sock->Read(buf, 512, callback.callback());
  EXPECT_TRUE(rv > 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_GT(rv, 0);
}

TEST_F(SSLClientSocketTest, Read_FullLogging) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::CapturingNetLog log;
  log.SetLogLevel(net::NetLog::LOG_ALL);
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<net::IOBuffer> request_buffer(
      new net::IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(request_buffer, arraysize(request_text) - 1,
                   callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  size_t last_index = net::ExpectLogContainsSomewhereAfter(
      entries, 5, net::NetLog::TYPE_SSL_SOCKET_BYTES_SENT,
      net::NetLog::PHASE_NONE);

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(4096));
  for (;;) {
    rv = sock->Read(buf, 4096, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == net::ERR_IO_PENDING);

    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;

    log.GetEntries(&entries);
    last_index = net::ExpectLogContainsSomewhereAfter(
        entries, last_index + 1, net::NetLog::TYPE_SSL_SOCKET_BYTES_RECEIVED,
        net::NetLog::PHASE_NONE);
  }
}

// Regression test for http://crbug.com/42538
TEST_F(SSLClientSocketTest, PrematureApplicationData) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  net::TestCompletionCallback callback;

  static const unsigned char application_data[] = {
    0x17, 0x03, 0x01, 0x00, 0x4a, 0x02, 0x00, 0x00, 0x46, 0x03, 0x01, 0x4b,
    0xc2, 0xf8, 0xb2, 0xc1, 0x56, 0x42, 0xb9, 0x57, 0x7f, 0xde, 0x87, 0x46,
    0xf7, 0xa3, 0x52, 0x42, 0x21, 0xf0, 0x13, 0x1c, 0x9c, 0x83, 0x88, 0xd6,
    0x93, 0x0c, 0xf6, 0x36, 0x30, 0x05, 0x7e, 0x20, 0xb5, 0xb5, 0x73, 0x36,
    0x53, 0x83, 0x0a, 0xfc, 0x17, 0x63, 0xbf, 0xa0, 0xe4, 0x42, 0x90, 0x0d,
    0x2f, 0x18, 0x6d, 0x20, 0xd8, 0x36, 0x3f, 0xfc, 0xe6, 0x01, 0xfa, 0x0f,
    0xa5, 0x75, 0x7f, 0x09, 0x00, 0x04, 0x00, 0x16, 0x03, 0x01, 0x11, 0x57,
    0x0b, 0x00, 0x11, 0x53, 0x00, 0x11, 0x50, 0x00, 0x06, 0x22, 0x30, 0x82,
    0x06, 0x1e, 0x30, 0x82, 0x05, 0x06, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02,
    0x0a
  };

  // All reads and writes complete synchronously (async=false).
  net::MockRead data_reads[] = {
    net::MockRead(net::SYNCHRONOUS,
                  reinterpret_cast<const char*>(application_data),
                  arraysize(application_data)),
    net::MockRead(net::SYNCHRONOUS, net::OK),
  };

  net::StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                     NULL, 0);

  net::StreamSocket* transport =
      new net::MockTCPClientSocket(addr, NULL, &data);
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::ERR_SSL_PROTOCOL_ERROR, rv);
}

// TODO(rsleevi): Not implemented for Schannel. As Schannel is only used when
// performing client authentication, it will not be tested here.
TEST_F(SSLClientSocketTest, CipherSuiteDisables) {
  // Rather than exhaustively disabling every RC4 ciphersuite defined at
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml,
  // only disabling those cipher suites that the test server actually
  // implements.
  const uint16 kCiphersToDisable[] = {
    0x0005,  // TLS_RSA_WITH_RC4_128_SHA
  };

  net::TestServer::SSLOptions ssl_options;
  // Enable only RC4 on the test server.
  ssl_options.bulk_ciphers =
      net::TestServer::SSLOptions::BULK_CIPHER_RC4;
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              ssl_options,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::CapturingNetLog log;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  net::SSLConfig ssl_config;
  for (size_t i = 0; i < arraysize(kCiphersToDisable); ++i)
    ssl_config.disabled_cipher_suites.push_back(kCiphersToDisable[i]);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));

  // NSS has special handling that maps a handshake_failure alert received
  // immediately after a client_hello to be a mismatched cipher suite error,
  // leading to ERR_SSL_VERSION_OR_CIPHER_MISMATCH. When using OpenSSL or
  // Secure Transport (OS X), the handshake_failure is bubbled up without any
  // interpretation, leading to ERR_SSL_PROTOCOL_ERROR. Either way, a failure
  // indicates that no cipher suite was negotiated with the test server.
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_TRUE(rv == net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH ||
              rv == net::ERR_SSL_PROTOCOL_ERROR);
  // The exact ordering differs between SSLClientSocketNSS (which issues an
  // extra read) and SSLClientSocketMac (which does not). Just make sure the
  // error appears somewhere in the log.
  log.GetEntries(&entries);
  net::ExpectLogContainsSomewhere(entries, 0,
                                  net::NetLog::TYPE_SSL_HANDSHAKE_ERROR,
                                  net::NetLog::PHASE_NONE);

  // We cannot test sock->IsConnected(), as the NSS implementation disconnects
  // the socket when it encounters an error, whereas other implementations
  // leave it connected.
  // Because this an error that the test server is mutually aware of, as opposed
  // to being an error such as a certificate name mismatch, which is
  // client-only, the exact index of the SSL connect end depends on how
  // quickly the test server closes the underlying socket. If the test server
  // closes before the IO message loop pumps messages, there may be a 0-byte
  // Read event in the NetLog due to TCPClientSocket picking up the EOF. As a
  // result, the SSL connect end event will be the second-to-last entry,
  // rather than the last entry.
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1) ||
              LogContainsSSLConnectEndEvent(entries, -2));
}

// When creating an SSLClientSocket, it is allowed to pass in a
// ClientSocketHandle that is not obtained from a client socket pool.
// Here we verify that such a simple ClientSocketHandle, not associated with any
// client socket pool, can be destroyed safely.
TEST_F(SSLClientSocketTest, ClientSocketHandleNotFromPool) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  net::ClientSocketHandle* socket_handle = new net::ClientSocketHandle();
  socket_handle->set_socket(transport);

  scoped_ptr<net::SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(
          socket_handle, test_server.host_port_pair(), kDefaultSSLConfig,
          context_));

  EXPECT_FALSE(sock->IsConnected());
  rv = sock->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);
}

// Verifies that SSLClientSocket::ExportKeyingMaterial return a success
// code and different keying label results in different keying material.
TEST_F(SSLClientSocketTest, ExportKeyingMaterial) {
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath());
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;

  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, NULL, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const int kKeyingMaterialSize = 32;
  const char* kKeyingLabel1 = "client-socket-test-1";
  const char* kKeyingContext = "";
  unsigned char client_out1[kKeyingMaterialSize];
  memset(client_out1, 0, sizeof(client_out1));
  rv = sock->ExportKeyingMaterial(kKeyingLabel1, false, kKeyingContext,
                                  client_out1, sizeof(client_out1));
  EXPECT_EQ(rv, net::OK);

  const char* kKeyingLabel2 = "client-socket-test-2";
  unsigned char client_out2[kKeyingMaterialSize];
  memset(client_out2, 0, sizeof(client_out2));
  rv = sock->ExportKeyingMaterial(kKeyingLabel2, false, kKeyingContext,
                                  client_out2, sizeof(client_out2));
  EXPECT_EQ(rv, net::OK);
  EXPECT_NE(memcmp(client_out1, client_out2, kKeyingMaterialSize), 0);
}

// Verifies that SSLClientSocket::ClearSessionCache can be called without
// explicit NSS initialization.
TEST(SSLClientSocket, ClearSessionCache) {
  net::SSLClientSocket::ClearSessionCache();
}

// This tests that SSLInfo contains a properly re-constructed certificate
// chain. That, in turn, verifies that GetSSLInfo is giving us the chain as
// verified, not the chain as served by the server. (They may be different.)
//
// CERT_CHAIN_WRONG_ROOT is redundant-server-chain.pem. It contains A
// (end-entity) -> B -> C, and C is signed by D. redundant-validated-chain.pem
// contains a chain of A -> B -> C2, where C2 is the same public key as C, but
// a self-signed root. Such a situation can occur when a new root (C2) is
// cross-certified by an old root (D) and has two different versions of its
// floating around. Servers may supply C2 as an intermediate, but the
// SSLClientSocket should return the chain that was verified, from
// verify_result, instead.
TEST_F(SSLClientSocketTest, VerifyReturnChainProperlyOrdered) {
  // By default, cause the CertVerifier to treat all certificates as
  // expired.
  cert_verifier_->set_default_result(net::ERR_CERT_DATE_INVALID);

  // We will expect SSLInfo to ultimately contain this chain.
  net::CertificateList certs = CreateCertificateListFromFile(
      net::GetTestCertsDirectory(), "redundant-validated-chain.pem",
      net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  net::X509Certificate::OSCertHandles temp_intermediates;
  temp_intermediates.push_back(certs[1]->os_cert_handle());
  temp_intermediates.push_back(certs[2]->os_cert_handle());

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                             temp_intermediates);

  // Add a rule that maps the server cert (A) to the chain of A->B->C2
  // rather than A->B->C.
  cert_verifier_->AddResultForCert(certs[0], verify_result, net::OK);

  // Load and install the root for the validated chain.
  scoped_refptr<net::X509Certificate> root_cert =
    net::ImportCertFromFile(net::GetTestCertsDirectory(),
                           "redundant-validated-chain-root.pem");
  ASSERT_NE(static_cast<net::X509Certificate*>(NULL), root_cert);
  net::ScopedTestRoot scoped_root(root_cert);

  // Set up a test server with CERT_CHAIN_WRONG_ROOT.
  net::TestServer::SSLOptions ssl_options(
      net::TestServer::SSLOptions::CERT_CHAIN_WRONG_ROOT);
  net::TestServer test_server(net::TestServer::TYPE_HTTPS,
                              ssl_options,
                              FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  net::AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  net::TestCompletionCallback callback;
  net::CapturingNetLog log;
  net::StreamSocket* transport = new net::TCPClientSocket(
      addr, &log, net::NetLog::Source());
  int rv = transport->Connect(callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  scoped_ptr<net::SSLClientSocket> sock(
      CreateSSLClientSocket(transport, test_server.host_port_pair(),
                            kDefaultSSLConfig));
  EXPECT_FALSE(sock->IsConnected());
  rv = sock->Connect(callback.callback());

  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 5, net::NetLog::TYPE_SSL_CONNECT));
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

  net::SSLInfo ssl_info;
  sock->GetSSLInfo(&ssl_info);

  // Verify that SSLInfo contains the corrected re-constructed chain A -> B
  // -> C2.
  const net::X509Certificate::OSCertHandles& intermediates =
      ssl_info.cert->GetIntermediateCertificates();
  ASSERT_EQ(2U, intermediates.size());
  EXPECT_TRUE(net::X509Certificate::IsSameOSCert(
      ssl_info.cert->os_cert_handle(), certs[0]->os_cert_handle()));
  EXPECT_TRUE(net::X509Certificate::IsSameOSCert(
      intermediates[0], certs[1]->os_cert_handle()));
  EXPECT_TRUE(net::X509Certificate::IsSameOSCert(
      intermediates[1], certs[2]->os_cert_handle()));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

