// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_mac.h"

#include "base/scoped_cftyperef.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/io_buffer.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_info.h"

// Welcome to Mac SSL. We've been waiting for you.
//
// The Mac SSL implementation is, like the Windows and NSS implementations, a
// giant state machine. This design constraint is due to the asynchronous nature
// of our underlying transport mechanism. We can call down to read/write on the
// network, but what happens is that either it completes immediately or returns
// saying that we'll get a callback sometime in the future. In that case, we
// have to return to our caller but pick up where we left off when we
// resume. Thus the fun.
//
// On Windows, we use Security Contexts, which are driven by us. We fetch data
// from the network, we call the context to decrypt the data, and so on. On the
// Mac, however, we provide Secure Transport with callbacks to get data from the
// network, and it calls us back to fetch the data from the network for
// it. Therefore, there are different sets of states in our respective state
// machines, fewer on the Mac because Secure Transport keeps a lot of its own
// state. The discussion about what each of the states means lives in comments
// in the DoLoop() function.
//
// Secure Transport is designed for use by either blocking or non-blocking
// network I/O. If, for example, you called SSLRead() to fetch data, Secure
// Transport will, unless it has some cached data, issue a read to your network
// callback read function to fetch it some more encrypted data. It's expecting
// one of two things. If your function is hooked up to a blocking source, then
// it'll block pending receipt of the data from the other end. That's fine, as
// when you return with the data, Secure Transport will do its thing. On the
// other hand, suppose that your socket is non-blocking and tells your function
// that it would block. Then you let Secure Transport know, and it'll tell the
// original caller that it would have blocked and that they need to call it
// "later."
//
// When's "later," though? We have fully-asynchronous networking, so we get a
// callback when our data's ready. But Secure Transport has no way for us to
// tell it that data has arrived, so we must re-execute the call that triggered
// the I/O (we rely on our state machine to do this). When we do so Secure
// Transport will ask once again for the data. Chances are that it'll be the
// same request as the previous time, but that's not actually guaranteed. But as
// long as we buffer what we have and keep track of where we were, it works
// quite well.
//
// Except for network writes. They shoot this plan straight to hell.
//
// Faking a blocking connection with an asynchronous connection (theoretically
// more powerful) simply doesn't work for writing. Suppose that Secure Transport
// requests a write of data to the network. With blocking I/O, we'd just block
// until the write completed, and with non-blocking I/O we'd know how many bytes
// we wrote before we would have blocked. But with the asynchronous I/O, the
// transport underneath us can tell us that it'll let us know sometime "later"
// whether or not things succeeded, and how many bytes were written. What do we
// return to Secure Transport? We can't return a byte count, but we can't return
// "later" as we're not guaranteed to be called in the future with the same data
// to write.
//
// So, like in any good relationship, we're forced to lie. Whenever Secure
// Transport asks for data to be written, we take it all and lie about it always
// being written. We spin in a loop (see SSLWriteCallback() and
// OnTransportWriteComplete()) independent of the main state machine writing
// the data to the network, and get the data out. The main consequence of this
// independence from the state machine is that we require a full-duplex
// transport underneath us since we can't use it to keep our reading and
// writing straight. Fortunately, the NSS implementation also has this issue
// to deal with, so we share the same Libevent-based full-duplex TCP socket.
//
// A side comment on return values might be in order. Those who haven't taken
// the time to read the documentation (ahem, header comments) in our various
// files might be a bit surprised to see result values being treated as both
// lengths and errors. Like Shimmer, they are both. In both the case of
// immediate results as well as results returned in callbacks, a negative return
// value indicates an error, a zero return value indicates end-of-stream (for
// reads), and a positive return value indicates the number of bytes read or
// written. Thus, many functions start off with |if (result < 0) return
// result;|. That gets the error condition out of the way, and from that point
// forward the result can be treated as a length.

namespace net {

namespace {

// Declarations needed to call the 10.5.7 and later SSLSetSessionOption()
// function when building with the 10.5.0 SDK.
typedef enum {
  kSSLSessionOptionBreakOnServerAuthFlag
} SSLSetSessionOptionType;

enum {
  errSSLServerAuthCompletedFlag = -9841
};

typedef OSStatus (*SSLSetSessionOptionFuncPtr)(SSLContextRef,
                                               SSLSetSessionOptionType,
                                               Boolean);
// For an explanation of the Mac OS X error codes, please refer to:
// http://developer.apple.com/mac/library/documentation/Security/Reference/secureTransportRef/Reference/reference.html
int NetErrorFromOSStatus(OSStatus status) {
  switch (status) {
    case errSSLWouldBlock:
      return ERR_IO_PENDING;
    case errSSLBadCipherSuite:
    case errSSLBadConfiguration:
      return ERR_INVALID_ARGUMENT;
    case errSSLClosedNoNotify:
      return ERR_CONNECTION_RESET;
    case errSSLConnectionRefused:
      return ERR_CONNECTION_REFUSED;
    case errSSLClosedAbort:
      return ERR_CONNECTION_ABORTED;
    case errSSLInternal:
    case errSSLCrypto:
    case errSSLFatalAlert:
    case errSSLIllegalParam:  // Received an illegal_parameter alert.
    case errSSLPeerUnexpectedMsg:  // Received an unexpected_message alert.
    case errSSLProtocol:
    case errSSLPeerHandshakeFail:  // Received a handshake_failure alert.
      return ERR_SSL_PROTOCOL_ERROR;
    case errSSLHostNameMismatch:
      return ERR_CERT_COMMON_NAME_INVALID;
    case errSSLCertExpired:
    case errSSLCertNotYetValid:
      return ERR_CERT_DATE_INVALID;
    case errSSLNoRootCert:
    case errSSLUnknownRootCert:
      return ERR_CERT_AUTHORITY_INVALID;
    case errSSLXCertChainInvalid:
    case errSSLBadCert:
      return ERR_CERT_INVALID;
    case errSSLPeerCertRevoked:
      return ERR_CERT_REVOKED;

    case errSSLClosedGraceful:
    case noErr:
      return OK;

    case errSSLBadRecordMac:
    case errSSLBufferOverflow:
    case errSSLDecryptionFail:
    case errSSLModuleAttach:
    case errSSLNegotiation:
    case errSSLRecordOverflow:
    case errSSLSessionNotFound:
    default:
      LOG(WARNING) << "Unknown error " << status <<
          " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

OSStatus OSStatusFromNetError(int net_error) {
  switch (net_error) {
    case ERR_IO_PENDING:
      return errSSLWouldBlock;
    case ERR_INTERNET_DISCONNECTED:
    case ERR_TIMED_OUT:
    case ERR_CONNECTION_ABORTED:
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_REFUSED:
    case ERR_ADDRESS_UNREACHABLE:
    case ERR_ADDRESS_INVALID:
      return errSSLClosedAbort;
    case OK:
      return noErr;
    default:
      LOG(WARNING) << "Unknown error " << net_error <<
          " mapped to errSSLIllegalParam";
      return errSSLIllegalParam;
  }
}

// Converts from a cipher suite to its key size. If the suite is marked with a
// **, it's not actually implemented in Secure Transport and won't be returned
// (but we'll code for it anyway).  The reference here is
// http://www.opensource.apple.com/darwinsource/10.5.5/libsecurity_ssl-32463/lib/cipherSpecs.c
// Seriously, though, there has to be an API for this, but I can't find one.
// Anybody?
int KeySizeOfCipherSuite(SSLCipherSuite suite) {
  switch (suite) {
    // SSL 2 only

    case SSL_RSA_WITH_DES_CBC_MD5:
      return 56;
    case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
      return 112;
    case SSL_RSA_WITH_RC2_CBC_MD5:
    case SSL_RSA_WITH_IDEA_CBC_MD5:              // **
      return 128;
    case SSL_NO_SUCH_CIPHERSUITE:                // **
      return 0;

    // SSL 2, 3, TLS

    case SSL_NULL_WITH_NULL_NULL:
    case SSL_RSA_WITH_NULL_MD5:
    case SSL_RSA_WITH_NULL_SHA:                  // **
    case SSL_FORTEZZA_DMS_WITH_NULL_SHA:         // **
      return 0;
    case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
    case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
    case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:   // **
    case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:   // **
    case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
    case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
      return 40;
    case SSL_RSA_WITH_DES_CBC_SHA:
    case SSL_DH_DSS_WITH_DES_CBC_SHA:            // **
    case SSL_DH_RSA_WITH_DES_CBC_SHA:            // **
    case SSL_DHE_DSS_WITH_DES_CBC_SHA:
    case SSL_DHE_RSA_WITH_DES_CBC_SHA:
    case SSL_DH_anon_WITH_DES_CBC_SHA:
      return 56;
    case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA: // **
      return 80;
    case SSL_RSA_WITH_3DES_EDE_CBC_SHA:
    case SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA:       // **
    case SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA:       // **
    case SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
    case SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
    case SSL_DH_anon_WITH_3DES_EDE_CBC_SHA:
      return 112;
    case SSL_RSA_WITH_RC4_128_MD5:
    case SSL_RSA_WITH_RC4_128_SHA:
    case SSL_RSA_WITH_IDEA_CBC_SHA:              // **
    case SSL_DH_anon_WITH_RC4_128_MD5:
      return 128;

    // TLS AES options (see RFC 3268)

    case TLS_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_128_CBC_SHA:        // **
    case TLS_DH_RSA_WITH_AES_128_CBC_SHA:        // **
    case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_anon_WITH_AES_128_CBC_SHA:
      return 128;
    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_256_CBC_SHA:        // **
    case TLS_DH_RSA_WITH_AES_256_CBC_SHA:        // **
    case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_anon_WITH_AES_256_CBC_SHA:
      return 256;

    default:
      return -1;
  }
}

// Returns the server's certificate.  The caller must release a reference
// to the return value when done.  Returns NULL on failure.
X509Certificate* GetServerCert(SSLContextRef ssl_context) {
  CFArrayRef certs;
  OSStatus status = SSLCopyPeerCertificates(ssl_context, &certs);
  // SSLCopyPeerCertificates may succeed but return a null |certs|
  // (if we're using an anonymous cipher suite or if we call it
  // before the certificate message has arrived and been parsed).
  if (status != noErr || !certs)
    return NULL;
  scoped_cftyperef<CFArrayRef> scoped_certs(certs);

  DCHECK_GT(CFArrayGetCount(certs), 0);

  SecCertificateRef server_cert = static_cast<SecCertificateRef>(
      const_cast<void*>(CFArrayGetValueAtIndex(certs, 0)));
  CFRetain(server_cert);
  X509Certificate *x509_cert = X509Certificate::CreateFromHandle(
      server_cert, X509Certificate::SOURCE_FROM_NETWORK);
  if (!x509_cert)
    return NULL;

  // Add each of the intermediate certificates in the server's chain to the
  // server's X509Certificate object. This makes them available to
  // X509Certificate::Verify() for chain building.
  // TODO(wtc): Since X509Certificate::CreateFromHandle may return a cached
  // X509Certificate object, we may be adding intermediate CA certificates to
  // it repeatedly!
  CFIndex certs_length = CFArrayGetCount(certs);
  for (CFIndex i = 1; i < certs_length; ++i) {
    SecCertificateRef cert_ref = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(certs, i)));
    CFRetain(cert_ref);
    x509_cert->AddIntermediateCertificate(cert_ref);
  }

  return x509_cert;
}

}  // namespace

//-----------------------------------------------------------------------------

SSLClientSocketMac::SSLClientSocketMac(ClientSocket* transport_socket,
                                       const std::string& hostname,
                                       const SSLConfig& ssl_config)
    : handshake_io_callback_(this, &SSLClientSocketMac::OnHandshakeIOComplete),
      transport_read_callback_(this,
                               &SSLClientSocketMac::OnTransportReadComplete),
      transport_write_callback_(this,
                                &SSLClientSocketMac::OnTransportWriteComplete),
      transport_(transport_socket),
      hostname_(hostname),
      ssl_config_(ssl_config),
      user_connect_callback_(NULL),
      user_read_callback_(NULL),
      user_write_callback_(NULL),
      user_read_buf_len_(0),
      user_write_buf_len_(0),
      next_handshake_state_(STATE_NONE),
      completed_handshake_(false),
      handshake_interrupted_(false),
      ssl_context_(NULL),
      pending_send_error_(OK) {
}

SSLClientSocketMac::~SSLClientSocketMac() {
  Disconnect();
}

int SSLClientSocketMac::Connect(CompletionCallback* callback,
                                LoadLog* load_log) {
  DCHECK(transport_.get());
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(!user_connect_callback_);

  LoadLog::BeginEvent(load_log, LoadLog::TYPE_SSL_CONNECT);

  int rv = InitializeSSLContext();
  if (rv != OK) {
    LoadLog::EndEvent(load_log, LoadLog::TYPE_SSL_CONNECT);
    return rv;
  }

  next_handshake_state_ = STATE_HANDSHAKE_START;
  rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    load_log_ = load_log;
    user_connect_callback_ = callback;
  } else {
    LoadLog::EndEvent(load_log, LoadLog::TYPE_SSL_CONNECT);
  }
  return rv;
}

void SSLClientSocketMac::Disconnect() {
  completed_handshake_ = false;

  if (ssl_context_) {
    SSLClose(ssl_context_);
    SSLDisposeContext(ssl_context_);
    ssl_context_ = NULL;
  }

  // Shut down anything that may call us back.
  verifier_.reset();
  transport_->Disconnect();
}

bool SSLClientSocketMac::IsConnected() const {
  // Ideally, we should also check if we have received the close_notify alert
  // message from the server, and return false in that case.  We're not doing
  // that, so this function may return a false positive.  Since the upper
  // layer (HttpNetworkTransaction) needs to handle a persistent connection
  // closed by the server when we send a request anyway, a false positive in
  // exchange for simpler code is a good trade-off.
  return completed_handshake_ && transport_->IsConnected();
}

bool SSLClientSocketMac::IsConnectedAndIdle() const {
  // Unlike IsConnected, this method doesn't return a false positive.
  //
  // Strictly speaking, we should check if we have received the close_notify
  // alert message from the server, and return false in that case.  Although
  // the close_notify alert message means EOF in the SSL layer, it is just
  // bytes to the transport layer below, so transport_->IsConnectedAndIdle()
  // returns the desired false when we receive close_notify.
  return completed_handshake_ && transport_->IsConnectedAndIdle();
}

int SSLClientSocketMac::Read(IOBuffer* buf, int buf_len,
                             CompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK(!user_read_callback_);
  DCHECK(!user_read_buf_);

  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;

  int rv = DoPayloadRead();
  if (rv == ERR_IO_PENDING) {
    user_read_callback_ = callback;
  } else {
    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;
  }
  return rv;
}

int SSLClientSocketMac::Write(IOBuffer* buf, int buf_len,
                              CompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK(!user_write_callback_);
  DCHECK(!user_write_buf_);

  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  int rv = DoPayloadWrite();
  if (rv == ERR_IO_PENDING) {
    user_write_callback_ = callback;
  } else {
    user_write_buf_ = NULL;
    user_write_buf_len_ = 0;
  }
  return rv;
}

bool SSLClientSocketMac::SetReceiveBufferSize(int32 size) {
  return transport_->SetReceiveBufferSize(size);
}

bool SSLClientSocketMac::SetSendBufferSize(int32 size) {
  return transport_->SetSendBufferSize(size);
}

void SSLClientSocketMac::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();
  if (!server_cert_) {
    NOTREACHED();
    return;
  }

  // set cert
  ssl_info->cert = server_cert_;

  // update status
  ssl_info->cert_status = server_cert_verify_result_.cert_status;

  // security info
  SSLCipherSuite suite;
  OSStatus status = SSLGetNegotiatedCipher(ssl_context_, &suite);
  if (!status)
    ssl_info->security_bits = KeySizeOfCipherSuite(suite);
}

void SSLClientSocketMac::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  // TODO(wtc): implement this.
}

SSLClientSocket::NextProtoStatus
SSLClientSocketMac::GetNextProto(std::string* proto) {
  proto->clear();
  return kNextProtoUnsupported;
}

int SSLClientSocketMac::InitializeSSLContext() {
  OSStatus status = noErr;

  status = SSLNewContext(false, &ssl_context_);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetProtocolVersionEnabled(ssl_context_,
                                        kSSLProtocol2,
                                        ssl_config_.ssl2_enabled);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetProtocolVersionEnabled(ssl_context_,
                                        kSSLProtocol3,
                                        ssl_config_.ssl3_enabled);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetProtocolVersionEnabled(ssl_context_,
                                        kTLSProtocol1,
                                        ssl_config_.tls1_enabled);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetIOFuncs(ssl_context_, SSLReadCallback, SSLWriteCallback);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetConnection(ssl_context_, this);
  if (status)
    return NetErrorFromOSStatus(status);

  // Disable certificate verification within Secure Transport; we'll
  // be handling that ourselves.
  status = SSLSetEnableCertVerify(ssl_context_, false);
  if (status)
    return NetErrorFromOSStatus(status);

  // SSLSetSessionOption() was introduced in Mac OS X 10.5.7. It allows us
  // to perform certificate validation during the handshake, which is
  // required in order to properly enable session resumption.
  //
  // With the kSSLSessionOptionBreakOnServerAuth option set, SSLHandshake()
  // will return errSSLServerAuthCompleted after receiving the server's
  // Certificate during the handshake. That gives us an opportunity to verify
  // the server certificate and then re-enter that handshake (assuming the
  // certificate successfully validated).
  //
  // If SSLSetSessionOption() is not present, we do not enable session
  // resumption, because in that case we are verifying the server's certificate
  // after the handshake completes (but before any application data is
  // exchanged). If we were to enable session resumption in this situation,
  // the session would be cached before we verified the certificate, leaving
  // the potential for a session in which the certificate failed to validate
  // to still be able to be resumed.
  CFBundleRef bundle =
      CFBundleGetBundleWithIdentifier(CFSTR("com.apple.security"));
  if (bundle) {
    SSLSetSessionOptionFuncPtr ssl_set_session_options =
        reinterpret_cast<SSLSetSessionOptionFuncPtr>(
            CFBundleGetFunctionPointerForName(bundle,
                CFSTR("SSLSetSessionOption")));
    if (ssl_set_session_options) {
      status = ssl_set_session_options(ssl_context_,
                                       kSSLSessionOptionBreakOnServerAuthFlag,
                                       true);
      if (status)
        return NetErrorFromOSStatus(status);

      // Concatenate the hostname and peer address to use as the peer ID. To
      // resume a session, we must connect to the same server on the same port
      // using the same hostname (i.e., localhost and 127.0.0.1 are considered
      // different peers, which puts us through certificate validation again
      // and catches hostname/certificate name mismatches.
      struct sockaddr_storage addr;
      socklen_t addr_length = sizeof(struct sockaddr_storage);
      memset(&addr, 0, sizeof(addr));
      if (!transport_->GetPeerName(reinterpret_cast<struct sockaddr*>(&addr),
                                   &addr_length)) {
        // Assemble the socket hostname and address into a single buffer.
        std::vector<char> peer_id(hostname_.begin(), hostname_.end());
        peer_id.insert(peer_id.end(), reinterpret_cast<char*>(&addr),
                       reinterpret_cast<char*>(&addr) + addr_length);

        // SSLSetPeerID() treats peer_id as a binary blob, and makes its
        // own copy.
        status = SSLSetPeerID(ssl_context_, &peer_id[0], peer_id.size());
        if (status)
          return NetErrorFromOSStatus(status);
      }
    }
  }

  return OK;
}

void SSLClientSocketMac::DoConnectCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_connect_callback_);
  DCHECK(next_handshake_state_ == STATE_NONE);

  CompletionCallback* c = user_connect_callback_;
  user_connect_callback_ = NULL;
  c->Run(rv > OK ? OK : rv);
}

void SSLClientSocketMac::DoReadCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_read_callback_);

  // Since Run may result in Read being called, clear user_read_callback_ up
  // front.
  CompletionCallback* c = user_read_callback_;
  user_read_callback_ = NULL;
  user_read_buf_ = NULL;
  user_read_buf_len_ = 0;
  c->Run(rv);
}

void SSLClientSocketMac::DoWriteCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_write_callback_);

  // Since Run may result in Write being called, clear user_write_callback_ up
  // front.
  CompletionCallback* c = user_write_callback_;
  user_write_callback_ = NULL;
  user_write_buf_ = NULL;
  user_write_buf_len_ = 0;
  c->Run(rv);
}

void SSLClientSocketMac::OnHandshakeIOComplete(int result) {
  DCHECK(next_handshake_state_ != STATE_NONE);
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    LoadLog::EndEvent(load_log_, LoadLog::TYPE_SSL_CONNECT);
    load_log_ = NULL;
    DoConnectCallback(rv);
  }
}

void SSLClientSocketMac::OnTransportReadComplete(int result) {
  if (result > 0) {
    recv_buffer_.insert(recv_buffer_.end(),
                        read_io_buf_->data(),
                        read_io_buf_->data() + result);
  }
  read_io_buf_ = NULL;

  if (next_handshake_state_ != STATE_NONE) {
    int rv = DoHandshakeLoop(result);
    if (rv != ERR_IO_PENDING) {
      LoadLog::EndEvent(load_log_, LoadLog::TYPE_SSL_CONNECT);
      load_log_ = NULL;
      DoConnectCallback(rv);
    }
    return;
  }
  if (user_read_buf_) {
    if (result < 0) {
      DoReadCallback(result);
      return;
    }
    int rv = DoPayloadRead();
    if (rv != ERR_IO_PENDING)
      DoReadCallback(rv);
  }
}

void SSLClientSocketMac::OnTransportWriteComplete(int result) {
  write_io_buf_ = NULL;

  if (result < 0) {
    pending_send_error_ = result;
    return;
  }

  send_buffer_.erase(send_buffer_.begin(),
                     send_buffer_.begin() + result);
  if (!send_buffer_.empty())
    SSLWriteCallback(this, NULL, NULL);

  // Since SSLWriteCallback() lies to return noErr even if transport_->Write()
  // returns ERR_IO_PENDING, we don't need to call any callbacks here.
}

// This is the main loop driving the state machine. Most calls coming from the
// outside just set up a few variables and jump into here.
int SSLClientSocketMac::DoHandshakeLoop(int last_io_result) {
  DCHECK(next_handshake_state_ != STATE_NONE);
  int rv = last_io_result;
  do {
    State state = next_handshake_state_;
    next_handshake_state_ = STATE_NONE;
    switch (state) {
      case STATE_HANDSHAKE_START:
        // Do the SSL/TLS handshake, up to the server certificate message.
        rv = DoHandshakeStart();
        break;
      case STATE_VERIFY_CERT:
        // Kick off server certificate validation.
        rv = DoVerifyCert();
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        // Check the results of the  server certificate validation.
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_HANDSHAKE_FINISH:
        // Do the SSL/TLS handshake, after the server certificate message.
        rv = DoHandshakeFinish();
        break;
      default:
        rv = ERR_UNEXPECTED;
        NOTREACHED() << "unexpected state";
        break;
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketMac::DoHandshakeStart() {
  OSStatus status = SSLHandshake(ssl_context_);
  if (status == errSSLWouldBlock)
    next_handshake_state_ = STATE_HANDSHAKE_START;

  if (status == noErr || status == errSSLServerAuthCompletedFlag) {
    // TODO(hawk): we verify the certificate chain even on resumed sessions
    // so that we have the certificate status (valid, expired but overridden
    // by the user, EV, etc.) available. Eliminate this step once we have
    // a certificate validation result cache.
    next_handshake_state_ = STATE_VERIFY_CERT;
    if (status == errSSLServerAuthCompletedFlag) {
      // Override errSSLServerAuthCompletedFlag as it's not actually an error,
      // but rather an indication that we're only half way through the
      // handshake.
      handshake_interrupted_ = true;
      status = noErr;
    }
  }

  int net_error = NetErrorFromOSStatus(status);
  if (status == noErr || IsCertificateError(net_error)) {
    server_cert_ = GetServerCert(ssl_context_);
    if (!server_cert_)
      return ERR_UNEXPECTED;
  }
  return net_error;
}

int SSLClientSocketMac::DoVerifyCert() {
  next_handshake_state_ = STATE_VERIFY_CERT_COMPLETE;

  if (!server_cert_)
    return ERR_UNEXPECTED;

  int flags = 0;
  if (ssl_config_.rev_checking_enabled)
    flags |= X509Certificate::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= X509Certificate::VERIFY_EV_CERT;
  verifier_.reset(new CertVerifier);
  return verifier_->Verify(server_cert_, hostname_, flags,
                           &server_cert_verify_result_,
                           &handshake_io_callback_);
}

int SSLClientSocketMac::DoVerifyCertComplete(int result) {
  DCHECK(verifier_.get());
  verifier_.reset();

  if (IsCertificateError(result) && ssl_config_.IsAllowedBadCert(server_cert_))
    result = OK;

  if (handshake_interrupted_) {
    // With session resumption enabled the full handshake (i.e., the handshake
    // in a non-resumed session) occurs in two steps. Continue on to the second
    // step if the certificate is OK.
    if (result == OK)
      next_handshake_state_ = STATE_HANDSHAKE_FINISH;
  } else {
    // If the session was resumed or session resumption was disabled, we're
    // done with the handshake.
    completed_handshake_ = true;
    DCHECK(next_handshake_state_ == STATE_NONE);
  }

  return result;
}

int SSLClientSocketMac::DoHandshakeFinish() {
  OSStatus status = SSLHandshake(ssl_context_);

  if (status == errSSLWouldBlock)
    next_handshake_state_ = STATE_HANDSHAKE_FINISH;

  if (status == noErr) {
    completed_handshake_ = true;
    DCHECK(next_handshake_state_ == STATE_NONE);
  }

  return NetErrorFromOSStatus(status);
}

int SSLClientSocketMac::DoPayloadRead() {
  size_t processed = 0;
  OSStatus status = SSLRead(ssl_context_,
                            user_read_buf_->data(),
                            user_read_buf_len_,
                            &processed);

  // There's a subtle difference here in semantics of the "would block" errors.
  // In our code, ERR_IO_PENDING means the whole operation is async, while
  // errSSLWouldBlock means that the stream isn't ending (and is often returned
  // along with partial data). So even though "would block" is returned, if we
  // have data, let's just return it.

  if (processed > 0)
    return processed;

  if (status == errSSLClosedNoNotify) {
    // TODO(wtc): Unless we have received the close_notify alert, we need to
    // return an error code indicating that the SSL connection ended
    // uncleanly, a potential truncation attack.  See http://crbug.com/18586.
    return OK;
  }

  return NetErrorFromOSStatus(status);
}

int SSLClientSocketMac::DoPayloadWrite() {
  size_t processed = 0;
  OSStatus status = SSLWrite(ssl_context_,
                             user_write_buf_->data(),
                             user_write_buf_len_,
                             &processed);

  if (processed > 0)
    return processed;

  return NetErrorFromOSStatus(status);
}

// static
OSStatus SSLClientSocketMac::SSLReadCallback(SSLConnectionRef connection,
                                             void* data,
                                             size_t* data_length) {
  DCHECK(data);
  DCHECK(data_length);
  SSLClientSocketMac* us =
      const_cast<SSLClientSocketMac*>(
          static_cast<const SSLClientSocketMac*>(connection));

  if (us->read_io_buf_) {
    // We have I/O in flight; promise we'll get back to them and use the
    // existing callback to do so.
    *data_length = 0;
    return errSSLWouldBlock;
  }

  size_t total_read = us->recv_buffer_.size();

  int rv = 1;  // any old value to spin the loop below
  while (rv > 0 && total_read < *data_length) {
    us->read_io_buf_ = new IOBuffer(*data_length - total_read);
    rv = us->transport_->Read(us->read_io_buf_,
                              *data_length - total_read,
                              &us->transport_read_callback_);

    if (rv >= 0) {
      us->recv_buffer_.insert(us->recv_buffer_.end(),
                              us->read_io_buf_->data(),
                              us->read_io_buf_->data() + rv);
      us->read_io_buf_ = NULL;
      total_read += rv;
    }
  }

  *data_length = total_read;
  if (total_read) {
    memcpy(data, &us->recv_buffer_[0], total_read);
    us->recv_buffer_.clear();
  }

  if (rv != ERR_IO_PENDING)
    us->read_io_buf_ = NULL;

  if (rv < 0)
    return OSStatusFromNetError(rv);
  else if (rv == 0)  // stream closed
    return errSSLClosedGraceful;
  else
    return noErr;
}

// static
OSStatus SSLClientSocketMac::SSLWriteCallback(SSLConnectionRef connection,
                                              const void* data,
                                              size_t* data_length) {
  SSLClientSocketMac* us =
      const_cast<SSLClientSocketMac*>(
          static_cast<const SSLClientSocketMac*>(connection));

  if (us->pending_send_error_ != OK) {
    OSStatus status = OSStatusFromNetError(us->pending_send_error_);
    us->pending_send_error_ = OK;
    return status;
  }

  if (data)
    us->send_buffer_.insert(us->send_buffer_.end(),
                            static_cast<const char*>(data),
                            static_cast<const char*>(data) + *data_length);

  if (us->write_io_buf_) {
    // If we have I/O in flight, just add the data to the end of the buffer and
    // return to our caller. The existing callback will trigger the write of the
    // new data when it sees that data remains in the buffer after removing the
    // sent data. As always, lie to our caller.
    return noErr;
  }

  int rv;
  do {
    us->write_io_buf_ = new IOBuffer(us->send_buffer_.size());
    memcpy(us->write_io_buf_->data(), &us->send_buffer_[0],
           us->send_buffer_.size());
    rv = us->transport_->Write(us->write_io_buf_,
                               us->send_buffer_.size(),
                               &us->transport_write_callback_);
    if (rv > 0) {
      us->send_buffer_.erase(us->send_buffer_.begin(),
                             us->send_buffer_.begin() + rv);
      us->write_io_buf_ = NULL;
    }
  } while (rv > 0 && !us->send_buffer_.empty());

  if (rv < 0 && rv != ERR_IO_PENDING) {
    us->write_io_buf_ = NULL;
    return OSStatusFromNetError(rv);
  }

  // always lie to our caller
  return noErr;
}

}  // namespace net
