// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_server_socket_nss.h"

#if defined(OS_WIN)
#include <winsock2.h>
#endif

#if defined(USE_SYSTEM_SSL)
#include <dlfcn.h>
#endif
#if defined(OS_MACOSX)
#include <Security/Security.h>
#endif
#include <certdb.h>
#include <cryptohi.h>
#include <hasht.h>
#include <keyhi.h>
#include <nspr.h>
#include <nss.h>
#include <pk11pub.h>
#include <secerr.h>
#include <sechash.h>
#include <ssl.h>
#include <sslerr.h>
#include <sslproto.h>

#include <limits>

#include "base/crypto/rsa_private_key.h"
#include "base/nss_util_internal.h"
#include "base/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/ocsp/nss_ocsp.h"
#include "net/socket/nss_ssl_util.h"
#include "net/socket/ssl_error_params.h"

static const int kRecvBufferSize = 4096;

#define GotoState(s) next_handshake_state_ = s

namespace net {

SSLServerSocket* CreateSSLServerSocket(
    Socket* socket, X509Certificate* cert, base::RSAPrivateKey* key,
    const SSLConfig& ssl_config) {
  return new SSLServerSocketNSS(socket, cert, key, ssl_config);
}

SSLServerSocketNSS::SSLServerSocketNSS(
    Socket* transport_socket,
    scoped_refptr<X509Certificate> cert,
    base::RSAPrivateKey* key,
    const SSLConfig& ssl_config)
    : ALLOW_THIS_IN_INITIALIZER_LIST(buffer_send_callback_(
          this, &SSLServerSocketNSS::BufferSendComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(buffer_recv_callback_(
          this, &SSLServerSocketNSS::BufferRecvComplete)),
      transport_send_busy_(false),
      transport_recv_busy_(false),
      user_accept_callback_(NULL),
      user_read_callback_(NULL),
      user_write_callback_(NULL),
      nss_fd_(NULL),
      nss_bufs_(NULL),
      transport_socket_(transport_socket),
      ssl_config_(ssl_config),
      cert_(cert),
      next_handshake_state_(STATE_NONE),
      completed_handshake_(false) {
  ssl_config_.false_start_enabled = false;
  ssl_config_.ssl3_enabled = true;
  ssl_config_.tls1_enabled = true;

  // TODO(hclam): Need a better way to clone a key.
  std::vector<uint8> key_bytes;
  CHECK(key->ExportPrivateKey(&key_bytes));
  key_.reset(base::RSAPrivateKey::CreateFromPrivateKeyInfo(key_bytes));
  CHECK(key_.get());
}

SSLServerSocketNSS::~SSLServerSocketNSS() {
  if (nss_fd_ != NULL) {
    PR_Close(nss_fd_);
    nss_fd_ = NULL;
  }
}

int SSLServerSocketNSS::Init() {
  // Initialize the NSS SSL library in a threadsafe way.  This also
  // initializes the NSS base library.
  EnsureNSSSSLInit();
  if (!NSS_IsInitialized())
    return ERR_UNEXPECTED;
#if !defined(OS_MACOSX) && !defined(OS_WIN)
  // We must call EnsureOCSPInit() here, on the IO thread, to get the IO loop
  // by MessageLoopForIO::current().
  // X509Certificate::Verify() runs on a worker thread of CertVerifier.
  EnsureOCSPInit();
#endif

  return OK;
}

int SSLServerSocketNSS::Accept(CompletionCallback* callback) {
  net_log_.BeginEvent(NetLog::TYPE_SSL_ACCEPT, NULL);

  int rv = Init();
  if (rv != OK) {
    LOG(ERROR) << "Failed to initialize NSS";
    net_log_.EndEvent(NetLog::TYPE_SSL_ACCEPT, NULL);
    return rv;
  }

  rv = InitializeSSLOptions();
  if (rv != OK) {
    LOG(ERROR) << "Failed to initialize SSL options";
    net_log_.EndEvent(NetLog::TYPE_SSL_ACCEPT, NULL);
    return rv;
  }

  // Set peer address. TODO(hclam): This should be in a separate method.
  PRNetAddr peername;
  memset(&peername, 0, sizeof(peername));
  peername.raw.family = AF_INET;
  memio_SetPeerName(nss_fd_, &peername);

  GotoState(STATE_HANDSHAKE);
  rv = DoHandshakeLoop(net::OK);
  if (rv == ERR_IO_PENDING) {
    user_accept_callback_ = callback;
  } else {
    net_log_.EndEvent(NetLog::TYPE_SSL_ACCEPT, NULL);
  }

  return rv > OK ? OK : rv;
}

int SSLServerSocketNSS::Read(IOBuffer* buf, int buf_len,
                             CompletionCallback* callback) {
  DCHECK(!user_read_callback_);
  DCHECK(!user_accept_callback_);
  DCHECK(!user_read_buf_);
  DCHECK(nss_bufs_);

  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;

  DCHECK(completed_handshake_);

  int rv = DoReadLoop(OK);

  if (rv == ERR_IO_PENDING) {
    user_read_callback_ = callback;
  } else {
    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;
  }
  return rv;
}

int SSLServerSocketNSS::Write(IOBuffer* buf, int buf_len,
                              CompletionCallback* callback) {
  DCHECK(!user_write_callback_);
  DCHECK(!user_write_buf_);
  DCHECK(nss_bufs_);

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

// static
// NSS calls this if an incoming certificate needs to be verified.
// Do nothing but return SECSuccess.
// This is called only in full handshake mode.
// Peer certificate is retrieved in HandshakeCallback() later, which is called
// in full handshake mode or in resumption handshake mode.
SECStatus SSLServerSocketNSS::OwnAuthCertHandler(void* arg,
                                                 PRFileDesc* socket,
                                                 PRBool checksig,
                                                 PRBool is_server) {
  // TODO(hclam): Implement.
  // Tell NSS to not verify the certificate.
  return SECSuccess;
}

// static
// NSS calls this when handshake is completed.
// After the SSL handshake is finished we need to verify the certificate.
void SSLServerSocketNSS::HandshakeCallback(PRFileDesc* socket,
                                           void* arg) {
  // TODO(hclam): Implement.
}

int SSLServerSocketNSS::InitializeSSLOptions() {
  // Transport connected, now hook it up to nss
  // TODO(port): specify rx and tx buffer sizes separately
  nss_fd_ = memio_CreateIOLayer(kRecvBufferSize);
  if (nss_fd_ == NULL) {
    return ERR_OUT_OF_MEMORY;  // TODO(port): map NSPR error code.
  }

  // Grab pointer to buffers
  nss_bufs_ = memio_GetSecret(nss_fd_);

  /* Create SSL state machine */
  /* Push SSL onto our fake I/O socket */
  nss_fd_ = SSL_ImportFD(NULL, nss_fd_);
  if (nss_fd_ == NULL) {
    LogFailedNSSFunction(net_log_, "SSL_ImportFD", "");
    return ERR_OUT_OF_MEMORY;  // TODO(port): map NSPR/NSS error code.
  }
  // TODO(port): set more ssl options!  Check errors!

  int rv;

  rv = SSL_OptionSet(nss_fd_, SSL_SECURITY, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_SECURITY");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL2, PR_FALSE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_SSL2");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SSL3, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_SSL3");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_TLS, ssl_config_.tls1_enabled);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_ENABLE_TLS");
    return ERR_UNEXPECTED;
  }

  for (std::vector<uint16>::const_iterator it =
           ssl_config_.disabled_cipher_suites.begin();
       it != ssl_config_.disabled_cipher_suites.end(); ++it) {
    // This will fail if the specified cipher is not implemented by NSS, but
    // the failure is harmless.
    SSL_CipherPrefSet(nss_fd_, *it, PR_FALSE);
  }

  // Server socket doesn't need session tickets.
  rv = SSL_OptionSet(nss_fd_, SSL_ENABLE_SESSION_TICKETS, PR_FALSE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(
        net_log_, "SSL_OptionSet", "SSL_ENABLE_SESSION_TICKETS");
  }

  // Doing this will force PR_Accept perform handshake as server.
  rv = SSL_OptionSet(nss_fd_, SSL_HANDSHAKE_AS_CLIENT, PR_FALSE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_HANDSHAKE_AS_CLIENT");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_HANDSHAKE_AS_SERVER, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_HANDSHAKE_AS_SERVER");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_REQUEST_CERTIFICATE, PR_FALSE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_REQUEST_CERTIFICATE");
    return ERR_UNEXPECTED;
  }

  rv = SSL_OptionSet(nss_fd_, SSL_REQUIRE_CERTIFICATE, PR_FALSE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_OptionSet", "SSL_REQUIRE_CERTIFICATE");
    return ERR_UNEXPECTED;
  }

  rv = SSL_ConfigServerSessionIDCache(1024, 5, 5, NULL);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_ConfigureServerSessionIDCache", "");
    return ERR_UNEXPECTED;
  }

  rv = SSL_AuthCertificateHook(nss_fd_, OwnAuthCertHandler, this);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_AuthCertificateHook", "");
    return ERR_UNEXPECTED;
  }

  rv = SSL_HandshakeCallback(nss_fd_, HandshakeCallback, this);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_HandshakeCallback", "");
    return ERR_UNEXPECTED;
  }

  // Get a certificate of CERTCertificate structure.
  std::string der_string;
  if (!cert_->GetDEREncoded(&der_string))
    return ERR_UNEXPECTED;

  SECItem der_cert;
  der_cert.data = reinterpret_cast<unsigned char*>(const_cast<char*>(
      der_string.data()));
  der_cert.len  = der_string.length();
  der_cert.type = siDERCertBuffer;

  // Parse into a CERTCertificate structure.
  CERTCertificate* cert = CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &der_cert, NULL, PR_FALSE, PR_TRUE);

  // Get a key of SECKEYPrivateKey* structure.
  std::vector<uint8> key_vector;
  if (!key_->ExportPrivateKey(&key_vector)) {
    CERT_DestroyCertificate(cert);
    return ERR_UNEXPECTED;
  }

  SECKEYPrivateKeyStr* private_key = NULL;
  PK11SlotInfo *slot = base::GetDefaultNSSKeySlot();
  if (!slot) {
    CERT_DestroyCertificate(cert);
    return ERR_UNEXPECTED;
  }

  SECItem der_private_key_info;
  der_private_key_info.data =
      const_cast<unsigned char*>(&key_vector.front());
  der_private_key_info.len = key_vector.size();
  // The server's RSA private key must be imported into NSS with the
  // following key usage bits:
  // - KU_KEY_ENCIPHERMENT, required for the RSA key exchange algorithm.
  // - KU_DIGITAL_SIGNATURE, required for the DHE_RSA and ECDHE_RSA key
  //   exchange algorithms.
  const unsigned int key_usage = KU_KEY_ENCIPHERMENT | KU_DIGITAL_SIGNATURE;
  rv =  PK11_ImportDERPrivateKeyInfoAndReturnKey(
      slot, &der_private_key_info, NULL, NULL, PR_FALSE, PR_FALSE,
      key_usage, &private_key, NULL);
  PK11_FreeSlot(slot);
  if (rv != SECSuccess) {
    CERT_DestroyCertificate(cert);
    return ERR_UNEXPECTED;
  }

  // Assign server certificate and private key.
  SSLKEAType cert_kea = NSS_FindCertKEAType(cert);
  rv = SSL_ConfigSecureServer(nss_fd_, cert, private_key, cert_kea);
  CERT_DestroyCertificate(cert);
  SECKEY_DestroyPrivateKey(private_key);

  if (rv != SECSuccess) {
    PRErrorCode prerr = PR_GetError();
    LOG(ERROR) << "Failed to config SSL server: " << prerr;
    LogFailedNSSFunction(net_log_, "SSL_ConfigureSecureServer", "");
    return ERR_UNEXPECTED;
  }

  // Tell SSL we're a server; needed if not letting NSPR do socket I/O
  rv = SSL_ResetHandshake(nss_fd_, PR_TRUE);
  if (rv != SECSuccess) {
    LogFailedNSSFunction(net_log_, "SSL_ResetHandshake", "");
    return ERR_UNEXPECTED;
  }

  return OK;
}

// Return 0 for EOF,
// > 0 for bytes transferred immediately,
// < 0 for error (or the non-error ERR_IO_PENDING).
int SSLServerSocketNSS::BufferSend(void) {
  if (transport_send_busy_)
    return ERR_IO_PENDING;

  const char* buf1;
  const char* buf2;
  unsigned int len1, len2;
  memio_GetWriteParams(nss_bufs_, &buf1, &len1, &buf2, &len2);
  const unsigned int len = len1 + len2;

  int rv = 0;
  if (len) {
    scoped_refptr<IOBuffer> send_buffer(new IOBuffer(len));
    memcpy(send_buffer->data(), buf1, len1);
    memcpy(send_buffer->data() + len1, buf2, len2);
    rv = transport_socket_->Write(send_buffer, len,
                                  &buffer_send_callback_);
    if (rv == ERR_IO_PENDING) {
      transport_send_busy_ = true;
    } else {
      memio_PutWriteResult(nss_bufs_, MapErrorToNSS(rv));
    }
  }

  return rv;
}

void SSLServerSocketNSS::BufferSendComplete(int result) {
  memio_PutWriteResult(nss_bufs_, MapErrorToNSS(result));
  transport_send_busy_ = false;
  OnSendComplete(result);
}

int SSLServerSocketNSS::BufferRecv(void) {
  if (transport_recv_busy_) return ERR_IO_PENDING;

  char *buf;
  int nb = memio_GetReadParams(nss_bufs_, &buf);
  int rv;
  if (!nb) {
    // buffer too full to read into, so no I/O possible at moment
    rv = ERR_IO_PENDING;
  } else {
    recv_buffer_ = new IOBuffer(nb);
    rv = transport_socket_->Read(recv_buffer_, nb, &buffer_recv_callback_);
    if (rv == ERR_IO_PENDING) {
      transport_recv_busy_ = true;
    } else {
      if (rv > 0)
        memcpy(buf, recv_buffer_->data(), rv);
      memio_PutReadResult(nss_bufs_, MapErrorToNSS(rv));
      recv_buffer_ = NULL;
    }
  }
  return rv;
}

void SSLServerSocketNSS::BufferRecvComplete(int result) {
  if (result > 0) {
    char *buf;
    memio_GetReadParams(nss_bufs_, &buf);
    memcpy(buf, recv_buffer_->data(), result);
  }
  recv_buffer_ = NULL;
  memio_PutReadResult(nss_bufs_, MapErrorToNSS(result));
  transport_recv_busy_ = false;
  OnRecvComplete(result);
}

void SSLServerSocketNSS::OnSendComplete(int result) {
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
    return;
  }

  if (!user_write_buf_ || !completed_handshake_)
    return;

  int rv = DoWriteLoop(result);
  if (rv != ERR_IO_PENDING)
    DoWriteCallback(rv);
}

void SSLServerSocketNSS::OnRecvComplete(int result) {
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
    return;
  }

  // Network layer received some data, check if client requested to read
  // decrypted data.
  if (!user_read_buf_ || !completed_handshake_)
    return;

  int rv = DoReadLoop(result);
  if (rv != ERR_IO_PENDING)
    DoReadCallback(rv);
}

void SSLServerSocketNSS::OnHandshakeIOComplete(int result) {
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    net_log_.EndEvent(net::NetLog::TYPE_SSL_ACCEPT, NULL);
    if (user_accept_callback_)
      DoAcceptCallback(rv);
  }
}

void SSLServerSocketNSS::DoAcceptCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);

  CompletionCallback* c = user_accept_callback_;
  user_accept_callback_ = NULL;
  c->Run(rv > OK ? OK : rv);
}

void SSLServerSocketNSS::DoReadCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_read_callback_);

  // Since Run may result in Read being called, clear |user_read_callback_|
  // up front.
  CompletionCallback* c = user_read_callback_;
  user_read_callback_ = NULL;
  user_read_buf_ = NULL;
  user_read_buf_len_ = 0;
  c->Run(rv);
}

void SSLServerSocketNSS::DoWriteCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_write_callback_);

  // Since Run may result in Write being called, clear |user_write_callback_|
  // up front.
  CompletionCallback* c = user_write_callback_;
  user_write_callback_ = NULL;
  user_write_buf_ = NULL;
  user_write_buf_len_ = 0;
  c->Run(rv);
}

// Do network I/O between the given buffer and the given socket.
// Return true if some I/O performed, false otherwise (error or ERR_IO_PENDING)
bool SSLServerSocketNSS::DoTransportIO() {
  bool network_moved = false;
  if (nss_bufs_ != NULL) {
    int nsent = BufferSend();
    int nreceived = BufferRecv();
    network_moved = (nsent > 0 || nreceived >= 0);
  }
  return network_moved;
}

int SSLServerSocketNSS::DoPayloadRead() {
  DCHECK(user_read_buf_);
  DCHECK_GT(user_read_buf_len_, 0);
  int rv = PR_Read(nss_fd_, user_read_buf_->data(), user_read_buf_len_);
  if (rv >= 0)
    return rv;
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR) {
    return ERR_IO_PENDING;
  }
  rv = MapNSSError(prerr);
  net_log_.AddEvent(NetLog::TYPE_SSL_READ_ERROR,
                    make_scoped_refptr(new SSLErrorParams(rv, prerr)));
  return rv;
}

int SSLServerSocketNSS::DoPayloadWrite() {
  DCHECK(user_write_buf_);
  int rv = PR_Write(nss_fd_, user_write_buf_->data(), user_write_buf_len_);
  if (rv >= 0)
    return rv;
  PRErrorCode prerr = PR_GetError();
  if (prerr == PR_WOULD_BLOCK_ERROR) {
    return ERR_IO_PENDING;
  }
  rv = MapNSSError(prerr);
  net_log_.AddEvent(NetLog::TYPE_SSL_WRITE_ERROR,
                    make_scoped_refptr(new SSLErrorParams(rv, prerr)));
  return rv;
}

int SSLServerSocketNSS::DoHandshakeLoop(int last_io_result) {
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
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }

    // Do the actual network I/O
    network_moved = DoTransportIO();
  } while ((rv != ERR_IO_PENDING || network_moved) &&
           next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLServerSocketNSS::DoReadLoop(int result) {
  DCHECK(completed_handshake_);
  DCHECK(next_handshake_state_ == STATE_NONE);

  if (result < 0)
    return result;

  if (!nss_bufs_) {
    LOG(DFATAL) << "!nss_bufs_";
    int rv = ERR_UNEXPECTED;
    net_log_.AddEvent(NetLog::TYPE_SSL_READ_ERROR,
                      make_scoped_refptr(new SSLErrorParams(rv, 0)));
    return rv;
  }

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadRead();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);
  return rv;
}

int SSLServerSocketNSS::DoWriteLoop(int result) {
  DCHECK(completed_handshake_);
  DCHECK(next_handshake_state_ == STATE_NONE);

  if (result < 0)
    return result;

  if (!nss_bufs_) {
    LOG(DFATAL) << "!nss_bufs_";
    int rv = ERR_UNEXPECTED;
    net_log_.AddEvent(NetLog::TYPE_SSL_WRITE_ERROR,
                      make_scoped_refptr(new SSLErrorParams(rv, 0)));
    return rv;
  }

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadWrite();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);
  return rv;
}

int SSLServerSocketNSS::DoHandshake() {
  int net_error = net::OK;
  SECStatus rv = SSL_ForceHandshake(nss_fd_);

  if (rv == SECSuccess) {
    completed_handshake_ = true;
  } else {
    PRErrorCode prerr = PR_GetError();
    net_error = MapNSSHandshakeError(prerr);

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      LOG(ERROR) << "handshake failed; NSS error code " << prerr
                 << ", net_error " << net_error;
      net_log_.AddEvent(
          NetLog::TYPE_SSL_HANDSHAKE_ERROR,
          make_scoped_refptr(new SSLErrorParams(net_error, prerr)));
    }
  }
  return net_error;
}

}  // namespace net
