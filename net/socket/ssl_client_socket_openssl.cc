// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OpenSSL binding for SSLClientSocket. The class layout and general principle
// of operation is derived from SSLClientSocketNSS.

#include "net/socket/ssl_client_socket_openssl.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "net/base/net_errors.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/ssl_info.h"
#include "net/base/test_certificate_data.h"  // TODO(joth): Remove!!

namespace net {

namespace {

// Enable this to see logging for state machine state transitions.
#if 0
#define GotoState(s) do { LOG(INFO) << (void *)this << " " << __FUNCTION__ << \
                           " jump to state " << s; \
                           next_handshake_state_ = s; } while (0)
#else
#define GotoState(s) next_handshake_state_ = s
#endif

const size_t kMaxRecvBufferSize = 4096;

void MaybeLogSSLError() {
  int error_num;
  while ((error_num = ERR_get_error()) != 0) {
    char buf[128];  // this buffer must be at least 120 chars long.
    ERR_error_string_n(error_num, buf, arraysize(buf));
    LOG(INFO) << "SSL error " << error_num << ": " << buf;
  }
}

int MapOpenSSLError(int err) {
  switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return ERR_IO_PENDING;
    default:
      // TODO(joth): Implement full mapping.
      LOG(WARNING) << "Unknown OpenSSL error " << err;
      MaybeLogSSLError();
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

}  // namespace

// static
SSL_CTX* SSLClientSocketOpenSSL::g_ctx = NULL;

SSLClientSocketOpenSSL::SSLClientSocketOpenSSL(
    ClientSocketHandle* transport_socket,
    const std::string& hostname,
    const SSLConfig& ssl_config)
    : ALLOW_THIS_IN_INITIALIZER_LIST(buffer_send_callback_(
          this, &SSLClientSocketOpenSSL::BufferSendComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(buffer_recv_callback_(
          this, &SSLClientSocketOpenSSL::BufferRecvComplete)),
      transport_send_busy_(false),
      transport_recv_busy_(false),
      user_connect_callback_(NULL),
      user_read_callback_(NULL),
      user_write_callback_(NULL),
      client_auth_cert_needed_(false),
      ssl_(NULL),
      transport_bio_(NULL),
      transport_(transport_socket),
      hostname_(hostname),
      ssl_config_(ssl_config),
      completed_handshake_(false),
      net_log_(transport_socket->socket()->NetLog()) {
}

SSLClientSocketOpenSSL::~SSLClientSocketOpenSSL() {
  Disconnect();
}

// TODO(joth): This method needs to be thread-safe.
bool SSLClientSocketOpenSSL::InitOpenSSL() {
  if (g_ctx)
    return true;

  SSL_load_error_strings();
  MaybeLogSSLError();
  SSL_library_init();
  MaybeLogSSLError();

  g_ctx = SSL_CTX_new(TLSv1_client_method());

  if (!g_ctx) {
    MaybeLogSSLError();
    return false;
  }

  SSL_CTX_set_verify(g_ctx, SSL_VERIFY_NONE, NULL /*callback*/);

  return true;
}

bool SSLClientSocketOpenSSL::Init() {
  DCHECK(g_ctx);
  ssl_ = SSL_new(g_ctx);
  if (!ssl_) {
    MaybeLogSSLError();
    return false;
  }

  if (!SSL_set_tlsext_host_name(ssl_, hostname_.c_str())) {
    MaybeLogSSLError();
    return false;
  }

  BIO* ssl_bio = NULL;
  // TODO(joth): Provide explicit write buffer sizes, rather than use defaults?
  if (!BIO_new_bio_pair(&ssl_bio, 0, &transport_bio_, 0)) {
    MaybeLogSSLError();
    return false;
  }
  DCHECK(ssl_bio);
  DCHECK(transport_bio_);

  SSL_set_bio(ssl_, ssl_bio, ssl_bio);

  return true;
}

// SSLClientSocket methods

void SSLClientSocketOpenSSL::GetSSLInfo(SSLInfo* ssl_info) {
  // Chrome DCHECKs that https pages provide a valid cert. For now (whilst in
  // early dev) we just create a spoof cert.
  // TODO(joth): implement X509Certificate for OpenSSL and remove this hack.
  LOG(WARNING) << "Filling in certificate with bogus content";
  ssl_info->Reset();
  ssl_info->cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  DCHECK(ssl_info->cert);
  ssl_info->cert_status = 0;
  ssl_info->security_bits = 56;
  ssl_info->connection_status =
      ((TLS1_CK_RSA_EXPORT1024_WITH_DES_CBC_SHA) &
          SSL_CONNECTION_CIPHERSUITE_MASK) << SSL_CONNECTION_CIPHERSUITE_SHIFT;

  // Silence compiler about unused vars.
  (void)google_der;
  (void)webkit_der;
  (void)thawte_der;
  (void)paypal_null_der;
}

void SSLClientSocketOpenSSL::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
}

SSLClientSocket::NextProtoStatus SSLClientSocketOpenSSL::GetNextProto(
    std::string* proto) {
  proto->clear();
  return kNextProtoUnsupported;
}

void SSLClientSocketOpenSSL::DoReadCallback(int rv) {
  // Since Run may result in Read being called, clear |user_read_callback_|
  // up front.
  CompletionCallback* c = user_read_callback_;
  user_read_callback_ = NULL;
  user_read_buf_ = NULL;
  user_read_buf_len_ = 0;
  c->Run(rv);
}

void SSLClientSocketOpenSSL::DoWriteCallback(int rv) {
  // Since Run may result in Write being called, clear |user_write_callback_|
  // up front.
  CompletionCallback* c = user_write_callback_;
  user_write_callback_ = NULL;
  user_write_buf_ = NULL;
  user_write_buf_len_ = 0;
  c->Run(rv);
}

// ClientSocket methods

int SSLClientSocketOpenSSL::Connect(CompletionCallback* callback) {
  net_log_.BeginEvent(NetLog::TYPE_SSL_CONNECT, NULL);

  if (!InitOpenSSL()) {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
    return ERR_UNEXPECTED;
  }

  // Set up new ssl object.
  if (!Init()) {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
    return ERR_UNEXPECTED;
  }

  // Set SSL to client mode. Handshake happens in the loop below.
  SSL_set_connect_state(ssl_);

  GotoState(STATE_HANDSHAKE);
  int rv = DoHandshakeLoop(net::OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
  } else {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
  }

  return rv > OK ? OK : rv;
}

void SSLClientSocketOpenSSL::Disconnect() {
  // Null all callbacks, delete all buffers.
  transport_send_busy_ = false;
  send_buffer_ = NULL;
  transport_recv_busy_ = false;
  recv_buffer_ = NULL;

  user_connect_callback_ = NULL;
  user_read_callback_    = NULL;
  user_write_callback_   = NULL;
  user_read_buf_         = NULL;
  user_read_buf_len_     = 0;
  user_write_buf_        = NULL;
  user_write_buf_len_    = 0;

  client_certs_.clear();
  client_auth_cert_needed_ = false;

  if (ssl_) {
    SSL_free(ssl_);
    ssl_ = NULL;
  }
  if (transport_bio_) {
    BIO_free(transport_bio_);
    transport_bio_ = NULL;
  }

  completed_handshake_ = false;
  transport_->socket()->Disconnect();
}

int SSLClientSocketOpenSSL::DoHandshakeLoop(int last_io_result) {
  bool network_moved;
  int rv = last_io_result;
  do {
    // Default to STATE_NONE for next state.
    // (This is a quirk carried over from the windows
    // implementation.  It makes reading the logs a bit harder.)
    // State handlers can and often do call GotoState just
    // to stay in the current state.
    State state = next_handshake_state_;
    GotoState(STATE_NONE);
    switch (state) {
      case STATE_NONE:
        // we're just pumping data between the buffer and the network
        break;
      case STATE_HANDSHAKE:
        rv = DoHandshake();
        break;
#if 0  // TODO(joth): Implement cert verification.
      case STATE_VERIFY_CERT:
        DCHECK(rv == OK);
        rv = DoVerifyCert(rv);
       break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
#endif
      default:
        rv = ERR_UNEXPECTED;
        NOTREACHED() << "unexpected state" << state;
        break;
    }

    // To avoid getting an ERR_IO_PENDING here after handshake complete.
    if (next_handshake_state_ == STATE_NONE)
      break;

    // Do the actual network I/O.
    network_moved = DoTransportIO();
  } while ((rv != ERR_IO_PENDING || network_moved) &&
            next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketOpenSSL::DoHandshake() {
  int net_error = net::OK;
  int rv = SSL_do_handshake(ssl_);

  if (rv == 1) {
    // SSL handshake is completed.  Let's verify the certificate.
    // For now we are done, certificates not implemented yet.
    // TODO(joth): Implement certificates
    GotoState(STATE_NONE);
    completed_handshake_ = true;
  } else {
    int ssl_error = SSL_get_error(ssl_, rv);

    // If not done, stay in this state
    GotoState(STATE_HANDSHAKE);
    net_error = MapOpenSSLError(ssl_error);
  }

  return net_error;
}

bool SSLClientSocketOpenSSL::DoTransportIO() {
  bool network_moved = false;
  int nsent = BufferSend();
  int nreceived = BufferRecv();
  network_moved = (nsent > 0 || nreceived >= 0);
  return network_moved;
}

int SSLClientSocketOpenSSL::BufferSend(void) {
  if (transport_send_busy_)
    return ERR_IO_PENDING;

  if (!send_buffer_) {
    // Get a fresh send buffer out of the send BIO.
    size_t max_read = BIO_ctrl_pending(transport_bio_);
    if (max_read > 0) {
      send_buffer_ = new DrainableIOBuffer(new IOBuffer(max_read), max_read);
      int read_bytes = BIO_read(transport_bio_, send_buffer_->data(), max_read);
      DCHECK_GT(read_bytes, 0);
      CHECK_EQ(static_cast<int>(max_read), read_bytes);
    }
  }

  int rv = 0;
  while (send_buffer_) {
    rv = transport_->socket()->Write(send_buffer_,
                                     send_buffer_->BytesRemaining(),
                                     &buffer_send_callback_);
    if (rv == ERR_IO_PENDING) {
      transport_send_busy_ = true;
      return rv;
    }
    TransportWriteComplete(rv);
  }
  return rv;
}

void SSLClientSocketOpenSSL::BufferSendComplete(int result) {
  transport_send_busy_ = false;
  TransportWriteComplete(result);
  OnSendComplete(result);
}

void SSLClientSocketOpenSSL::TransportWriteComplete(int result) {
  if (result < 0) {
    // Got a socket write error; close the BIO to indicate this upward.
    (void)BIO_shutdown_wr(transport_bio_);
    send_buffer_ = NULL;
  } else {
    DCHECK(send_buffer_);
    send_buffer_->DidConsume(result);
    DCHECK_GE(send_buffer_->BytesRemaining(), 0);
    if (send_buffer_->BytesRemaining() <= 0)
      send_buffer_ = NULL;
  }
}

int SSLClientSocketOpenSSL::BufferRecv(void) {
  if (transport_recv_busy_)
    return ERR_IO_PENDING;

  size_t max_write = BIO_ctrl_get_write_guarantee(transport_bio_);
  if (max_write > kMaxRecvBufferSize)
    max_write = kMaxRecvBufferSize;

  if (!max_write)
    return ERR_IO_PENDING;

  recv_buffer_ = new IOBuffer(max_write);
  int rv = transport_->socket()->Read(recv_buffer_, max_write,
                                      &buffer_recv_callback_);
  if (rv == ERR_IO_PENDING) {
    transport_recv_busy_ = true;
  } else {
    TransportReadComplete(rv);
  }
  return rv;
}

void SSLClientSocketOpenSSL::BufferRecvComplete(int result) {
  TransportReadComplete(result);
  OnRecvComplete(result);
}

void SSLClientSocketOpenSSL::TransportReadComplete(int result) {
  if (result > 0) {
    int ret = BIO_write(transport_bio_, recv_buffer_->data(), result);
    // A write into a memory BIO should always succeed.
    CHECK_EQ(result, ret);
  } else {
    // Received end of file: bubble it up to the SSL layer via the BIO.
    BIO_set_mem_eof_return(transport_bio_, 0);
    (void)BIO_shutdown_wr(transport_bio_);
  }
  recv_buffer_ = NULL;
  transport_recv_busy_ = false;
}

void SSLClientSocketOpenSSL::DoConnectCallback(int rv) {
  CompletionCallback* c = user_connect_callback_;
  user_connect_callback_ = NULL;
  c->Run(rv > OK ? OK : rv);
}

void SSLClientSocketOpenSSL::OnHandshakeIOComplete(int result) {
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    net_log_.EndEvent(NetLog::TYPE_SSL_CONNECT, NULL);
    DoConnectCallback(rv);
  }
}

void SSLClientSocketOpenSSL::OnSendComplete(int result) {
  if (next_handshake_state_ != STATE_NONE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
    return;
  }

  // OnSendComplete may need to call DoPayloadRead while the renegotiation
  // handshake is in progress.
  int rv_read = ERR_IO_PENDING;
  int rv_write = ERR_IO_PENDING;
  bool network_moved;
  do {
      if (user_read_buf_)
          rv_read = DoPayloadRead();
      if (user_write_buf_)
          rv_write = DoPayloadWrite();
      network_moved = DoTransportIO();
  } while (rv_read == ERR_IO_PENDING &&
           rv_write == ERR_IO_PENDING &&
           network_moved);

  if (user_read_buf_ && rv_read != ERR_IO_PENDING)
      DoReadCallback(rv_read);
  if (user_write_buf_ && rv_write != ERR_IO_PENDING)
      DoWriteCallback(rv_write);
}

void SSLClientSocketOpenSSL::OnRecvComplete(int result) {
  if (next_handshake_state_ != STATE_NONE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
    return;
  }

  // Network layer received some data, check if client requested to read
  // decrypted data.
  if (!user_read_buf_)
    return;

  int rv = DoReadLoop(result);
  if (rv != ERR_IO_PENDING)
    DoReadCallback(rv);
}

bool SSLClientSocketOpenSSL::IsConnected() const {
  bool ret = completed_handshake_ && transport_->socket()->IsConnected();
  return ret;
}

bool SSLClientSocketOpenSSL::IsConnectedAndIdle() const {
  bool ret = completed_handshake_ && transport_->socket()->IsConnectedAndIdle();
  return ret;
}

int SSLClientSocketOpenSSL::GetPeerAddress(AddressList* addressList) const {
  return transport_->socket()->GetPeerAddress(addressList);
}

const BoundNetLog& SSLClientSocketOpenSSL::NetLog() const {
  return net_log_;
}

void SSLClientSocketOpenSSL::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetSubresourceSpeculation();
  } else {
    NOTREACHED();
  }
}

void SSLClientSocketOpenSSL::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetOmniboxSpeculation();
  } else {
    NOTREACHED();
  }
}

bool SSLClientSocketOpenSSL::WasEverUsed() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->WasEverUsed();

  NOTREACHED();
  return false;
}

// Socket methods

int SSLClientSocketOpenSSL::Read(IOBuffer* buf,
                                 int buf_len,
                                 CompletionCallback* callback) {
  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;

  int rv = DoReadLoop(OK);

  if (rv == ERR_IO_PENDING) {
    user_read_callback_ = callback;
  } else {
    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;
  }

  return rv;
}

int SSLClientSocketOpenSSL::DoReadLoop(int result) {
  if (result < 0)
    return result;

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadRead();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  return rv;
}

int SSLClientSocketOpenSSL::Write(IOBuffer* buf,
                                  int buf_len,
                                  CompletionCallback* callback) {
  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  int rv = DoWriteLoop(OK);

  if (rv == ERR_IO_PENDING) {
    user_write_callback_ = callback;
  } else {
    user_write_buf_ = NULL;
    user_write_buf_len_ = 0;
  }

  return rv;
}

int SSLClientSocketOpenSSL::DoWriteLoop(int result) {
  if (result < 0)
    return result;

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadWrite();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  return rv;
}

bool SSLClientSocketOpenSSL::SetReceiveBufferSize(int32 size) {
  return transport_->socket()->SetReceiveBufferSize(size);
}

bool SSLClientSocketOpenSSL::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
}

int SSLClientSocketOpenSSL::DoPayloadRead() {
  int rv = SSL_read(ssl_, user_read_buf_->data(), user_read_buf_len_);
  // We don't need to invalidate the non-client-authenticated SSL session
  // because the server will renegotiate anyway.
  if (client_auth_cert_needed_)
    return ERR_SSL_CLIENT_AUTH_CERT_NEEDED;

  if (rv >= 0)
    return rv;

  int err = SSL_get_error(ssl_, rv);
  return MapOpenSSLError(err);
}

int SSLClientSocketOpenSSL::DoPayloadWrite() {
  int rv = SSL_write(ssl_, user_write_buf_->data(), user_write_buf_len_);

  if (rv >= 0)
    return rv;

  int err = SSL_get_error(ssl_, rv);
  return MapOpenSSLError(err);
}

} // namespace net
