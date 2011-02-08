// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_mac.h"

#include <CoreServices/CoreServices.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>

#include "base/lazy_instance.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/string_util.h"
#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/ssl_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_error_params.h"

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
// in the DoHandshakeLoop() function.
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

// Pause if we have 2MB of data in flight, resume once we're down below 1MB.
const unsigned int kWriteSizePauseLimit = 2 * 1024 * 1024;
const unsigned int kWriteSizeResumeLimit = 1 * 1024 * 1024;

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_5
// When compiled against the Mac OS X 10.5 SDK, define symbolic constants for
// cipher suites added in Mac OS X 10.6.
enum {
  // ECC cipher suites from RFC 4492.
  TLS_ECDH_ECDSA_WITH_NULL_SHA           = 0xC001,
  TLS_ECDH_ECDSA_WITH_RC4_128_SHA        = 0xC002,
  TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA   = 0xC003,
  TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA    = 0xC004,
  TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA    = 0xC005,
  TLS_ECDHE_ECDSA_WITH_NULL_SHA          = 0xC006,
  TLS_ECDHE_ECDSA_WITH_RC4_128_SHA       = 0xC007,
  TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA  = 0xC008,
  TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA   = 0xC009,
  TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA   = 0xC00A,
  TLS_ECDH_RSA_WITH_NULL_SHA             = 0xC00B,
  TLS_ECDH_RSA_WITH_RC4_128_SHA          = 0xC00C,
  TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA     = 0xC00D,
  TLS_ECDH_RSA_WITH_AES_128_CBC_SHA      = 0xC00E,
  TLS_ECDH_RSA_WITH_AES_256_CBC_SHA      = 0xC00F,
  TLS_ECDHE_RSA_WITH_NULL_SHA            = 0xC010,
  TLS_ECDHE_RSA_WITH_RC4_128_SHA         = 0xC011,
  TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA    = 0xC012,
  TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA     = 0xC013,
  TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA     = 0xC014,
  TLS_ECDH_anon_WITH_NULL_SHA            = 0xC015,
  TLS_ECDH_anon_WITH_RC4_128_SHA         = 0xC016,
  TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA    = 0xC017,
  TLS_ECDH_anon_WITH_AES_128_CBC_SHA     = 0xC018,
  TLS_ECDH_anon_WITH_AES_256_CBC_SHA     = 0xC019,
};
#endif

// For an explanation of the Mac OS X error codes, please refer to:
// http://developer.apple.com/mac/library/documentation/Security/Reference/secureTransportRef/Reference/reference.html
int NetErrorFromOSStatus(OSStatus status) {
  switch (status) {
    case errSSLWouldBlock:
      return ERR_IO_PENDING;
    case paramErr:
    case errSSLBadCipherSuite:
    case errSSLBadConfiguration:
      return ERR_INVALID_ARGUMENT;
    case errSSLClosedNoNotify:
      return ERR_CONNECTION_RESET;
    case errSSLClosedAbort:
      return ERR_CONNECTION_ABORTED;
    case errSSLInternal:
      return ERR_UNEXPECTED;
    case errSSLBadRecordMac:
    case errSSLCrypto:
    case errSSLConnectionRefused:
    case errSSLDecryptionFail:
    case errSSLFatalAlert:
    case errSSLIllegalParam:  // Received an illegal_parameter alert.
    case errSSLPeerDecodeError:  // Received a decode_error alert.
    case errSSLPeerDecryptError:  // Received a decrypt_error alert.
    case errSSLPeerExportRestriction:  // Received an export_restriction alert.
    case errSSLPeerHandshakeFail:  // Received a handshake_failure alert.
    case errSSLPeerNoRenegotiation:  // Received a no_renegotiation alert
    case errSSLPeerUnexpectedMsg:  // Received an unexpected_message alert.
    case errSSLProtocol:
    case errSSLRecordOverflow:
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

    case errSSLClosedGraceful:
    case noErr:
      return OK;

    // (Note that all errSSLPeer* codes indicate errors reported by the peer,
    // so the cert-related ones refer to my _client_ cert.)
    // TODO(wtc): Add fine-grained error codes for client certificate errors
    // reported by the server using the following SSL/TLS alert messages:
    //   access_denied
    //   bad_certificate
    //   unsupported_certificate
    //   certificate_expired
    //   certificate_revoked
    //   certificate_unknown
    //   unknown_ca
    case errSSLPeerCertUnknown...errSSLPeerBadCert:
    case errSSLPeerUnknownCA:
    case errSSLPeerAccessDenied:
      LOG(WARNING) << "Server rejected client cert (OSStatus=" << status << ")";
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;

    case errSSLNegotiation:
    case errSSLPeerInsufficientSecurity:
    case errSSLPeerProtocolVersion:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;

    case errSSLBufferOverflow:
    case errSSLModuleAttach:
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
    case ERR_UNEXPECTED:
      return errSSLInternal;
    case ERR_INVALID_ARGUMENT:
      return paramErr;
    case OK:
      return noErr;
    default:
      LOG(WARNING) << "Unknown error " << net_error <<
          " mapped to paramErr";
      return paramErr;
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
    case TLS_ECDH_ECDSA_WITH_NULL_SHA:
    case TLS_ECDHE_ECDSA_WITH_NULL_SHA:
    case TLS_ECDH_RSA_WITH_NULL_SHA:
    case TLS_ECDHE_RSA_WITH_NULL_SHA:
    case TLS_ECDH_anon_WITH_NULL_SHA:
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
    case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:
      return 112;
    case SSL_RSA_WITH_RC4_128_MD5:
    case SSL_RSA_WITH_RC4_128_SHA:
    case SSL_RSA_WITH_IDEA_CBC_SHA:              // **
    case SSL_DH_anon_WITH_RC4_128_MD5:
    case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
    case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
    case TLS_ECDH_RSA_WITH_RC4_128_SHA:
    case TLS_ECDHE_RSA_WITH_RC4_128_SHA:
    case TLS_ECDH_anon_WITH_RC4_128_SHA:
      return 128;

    // TLS AES options (see RFC 3268 and RFC 4492)

    case TLS_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_128_CBC_SHA:        // **
    case TLS_DH_RSA_WITH_AES_128_CBC_SHA:        // **
    case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_anon_WITH_AES_128_CBC_SHA:
    case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
    case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
    case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
    case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
    case TLS_ECDH_anon_WITH_AES_128_CBC_SHA:
      return 128;
    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_256_CBC_SHA:        // **
    case TLS_DH_RSA_WITH_AES_256_CBC_SHA:        // **
    case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_anon_WITH_AES_256_CBC_SHA:
    case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
    case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
    case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
    case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
    case TLS_ECDH_anon_WITH_AES_256_CBC_SHA:
      return 256;

    default:
      return -1;
  }
}

// Whitelist the cipher suites we want to enable.  We disable the following
// cipher suites.
// - Null encryption cipher suites.
// - Weak cipher suites: < 80 bits of security strength.
// - FORTEZZA cipher suites (obsolete).
// - IDEA cipher suites (RFC 5469 explains why).
// - Anonymous cipher suites.
//
// Why don't we use a blacklist?  A blacklist that isn't updated for a new
// Mac OS X release is a potential security issue because the new release
// may have new null encryption or anonymous cipher suites, whereas a
// whitelist that isn't updated for a new Mac OS X release just means we
// won't support any new cipher suites in that release.
bool ShouldEnableCipherSuite(SSLCipherSuite suite) {
  switch (suite) {
    case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
    case SSL_RSA_WITH_RC2_CBC_MD5:

    case SSL_RSA_WITH_3DES_EDE_CBC_SHA:
    case SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA:       // **
    case SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA:       // **
    case SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
    case SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:

    case SSL_RSA_WITH_RC4_128_MD5:
    case SSL_RSA_WITH_RC4_128_SHA:
    case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:
    case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:
    case TLS_ECDH_RSA_WITH_RC4_128_SHA:
    case TLS_ECDHE_RSA_WITH_RC4_128_SHA:

    case TLS_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_128_CBC_SHA:        // **
    case TLS_DH_RSA_WITH_AES_128_CBC_SHA:        // **
    case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
    case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
    case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
    case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
    case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:

    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_256_CBC_SHA:        // **
    case TLS_DH_RSA_WITH_AES_256_CBC_SHA:        // **
    case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
    case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
    case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
    case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
    case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
      return true;

    default:
      return false;
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
  base::mac::ScopedCFTypeRef<CFArrayRef> scoped_certs(certs);

  DCHECK_GT(CFArrayGetCount(certs), 0);

  // Add each of the intermediate certificates in the server's chain to the
  // server's X509Certificate object. This makes them available to
  // X509Certificate::Verify() for chain building.
  std::vector<SecCertificateRef> intermediate_ca_certs;
  CFIndex certs_length = CFArrayGetCount(certs);
  for (CFIndex i = 1; i < certs_length; ++i) {
    SecCertificateRef cert_ref = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(certs, i)));
    intermediate_ca_certs.push_back(cert_ref);
  }

  SecCertificateRef server_cert = static_cast<SecCertificateRef>(
      const_cast<void*>(CFArrayGetValueAtIndex(certs, 0)));
  return X509Certificate::CreateFromHandle(
      server_cert, X509Certificate::SOURCE_FROM_NETWORK, intermediate_ca_certs);
}

// Dynamically look up a pointer to a function exported by a bundle.
template <typename FNTYPE>
FNTYPE LookupFunction(CFStringRef bundleName, CFStringRef fnName) {
  CFBundleRef bundle = CFBundleGetBundleWithIdentifier(bundleName);
  if (!bundle)
    return NULL;
  return reinterpret_cast<FNTYPE>(
      CFBundleGetFunctionPointerForName(bundle, fnName));
}

struct CipherSuiteIsDisabledFunctor {
  explicit CipherSuiteIsDisabledFunctor(
      const std::vector<uint16>& disabled_cipher_suites)
      : disabled_cipher_suites_(disabled_cipher_suites) {}

  // Returns true if the given |cipher_suite| appears within the set of
  // |disabled_cipher_suites|.
  bool operator()(SSLCipherSuite cipher_suite) const {
    return binary_search(disabled_cipher_suites_.begin(),
                         disabled_cipher_suites_.end(),
                         static_cast<uint16>(cipher_suite));
  }

  const std::vector<uint16>& disabled_cipher_suites_;
};

// Class to determine what cipher suites are available and which cipher
// suites should be enabled, based on the overall security policy.
class EnabledCipherSuites {
 public:
  const std::vector<SSLCipherSuite>& ciphers() const { return ciphers_; }

 private:
  friend struct base::DefaultLazyInstanceTraits<EnabledCipherSuites>;
  EnabledCipherSuites();
  ~EnabledCipherSuites() {}

  std::vector<SSLCipherSuite> ciphers_;

  DISALLOW_COPY_AND_ASSIGN(EnabledCipherSuites);
};

static base::LazyInstance<EnabledCipherSuites> g_enabled_cipher_suites(
    base::LINKER_INITIALIZED);

EnabledCipherSuites::EnabledCipherSuites() {
  SSLContextRef ssl_context;
  OSStatus status = SSLNewContext(false, &ssl_context);
  if (status != noErr)
    return;

  size_t num_supported_ciphers;
  status = SSLGetNumberSupportedCiphers(ssl_context, &num_supported_ciphers);
  if (status != noErr) {
    SSLDisposeContext(ssl_context);
    return;
  }
  DCHECK_NE(num_supported_ciphers, 0U);

  std::vector<SSLCipherSuite> supported_ciphers(num_supported_ciphers);
  status = SSLGetSupportedCiphers(ssl_context, &supported_ciphers[0],
                                  &num_supported_ciphers);
  SSLDisposeContext(ssl_context);
  if (status != noErr)
    return;

  for (size_t i = 0; i < num_supported_ciphers; ++i) {
    if (ShouldEnableCipherSuite(supported_ciphers[i]))
      ciphers_.push_back(supported_ciphers[i]);
  }
}

}  // namespace

//-----------------------------------------------------------------------------

SSLClientSocketMac::SSLClientSocketMac(ClientSocketHandle* transport_socket,
                                       const HostPortPair& host_and_port,
                                       const SSLConfig& ssl_config,
                                       CertVerifier* cert_verifier)
    : handshake_io_callback_(this, &SSLClientSocketMac::OnHandshakeIOComplete),
      transport_read_callback_(this,
                               &SSLClientSocketMac::OnTransportReadComplete),
      transport_write_callback_(this,
                                &SSLClientSocketMac::OnTransportWriteComplete),
      transport_(transport_socket),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      user_connect_callback_(NULL),
      user_read_callback_(NULL),
      user_write_callback_(NULL),
      user_read_buf_len_(0),
      user_write_buf_len_(0),
      next_handshake_state_(STATE_NONE),
      cert_verifier_(cert_verifier),
      renegotiating_(false),
      client_cert_requested_(false),
      ssl_context_(NULL),
      bytes_read_after_renegotiation_(0),
      pending_send_error_(OK),
      net_log_(transport_socket->socket()->NetLog()) {
  // Sort the list of ciphers to disable, since disabling ciphers on Mac
  // requires subtracting from a list of enabled ciphers while maintaining
  // ordering, as opposed to merely needing to iterate them as with NSS.
  sort(ssl_config_.disabled_cipher_suites.begin(),
       ssl_config_.disabled_cipher_suites.end());
}

SSLClientSocketMac::~SSLClientSocketMac() {
  Disconnect();
}

int SSLClientSocketMac::Connect(CompletionCallback* callback) {
  DCHECK(transport_.get());
  DCHECK(next_handshake_state_ == STATE_NONE);
  DCHECK(!user_connect_callback_);

  net_log_.BeginEvent(NetLog::TYPE_SSL_CONNECT, NULL);

  int rv = InitializeSSLContext();
  if (rv != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    return rv;
  }

  next_handshake_state_ = STATE_HANDSHAKE;
  rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
  } else {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
  }
  return rv;
}

void SSLClientSocketMac::Disconnect() {
  next_handshake_state_ = STATE_NONE;

  if (ssl_context_) {
    SSLClose(ssl_context_);
    SSLDisposeContext(ssl_context_);
    ssl_context_ = NULL;
    VLOG(1) << "----- Disposed SSLContext";
  }

  // Shut down anything that may call us back.
  verifier_.reset();
  transport_->socket()->Disconnect();
}

bool SSLClientSocketMac::IsConnected() const {
  // Ideally, we should also check if we have received the close_notify alert
  // message from the server, and return false in that case.  We're not doing
  // that, so this function may return a false positive.  Since the upper
  // layer (HttpNetworkTransaction) needs to handle a persistent connection
  // closed by the server when we send a request anyway, a false positive in
  // exchange for simpler code is a good trade-off.
  return completed_handshake() && transport_->socket()->IsConnected();
}

bool SSLClientSocketMac::IsConnectedAndIdle() const {
  // Unlike IsConnected, this method doesn't return a false positive.
  //
  // Strictly speaking, we should check if we have received the close_notify
  // alert message from the server, and return false in that case.  Although
  // the close_notify alert message means EOF in the SSL layer, it is just
  // bytes to the transport layer below, so
  // transport_->socket()->IsConnectedAndIdle() returns the desired false
  // when we receive close_notify.
  return completed_handshake() && transport_->socket()->IsConnectedAndIdle();
}

int SSLClientSocketMac::GetPeerAddress(AddressList* address) const {
  return transport_->socket()->GetPeerAddress(address);
}

void SSLClientSocketMac::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetSubresourceSpeculation();
  } else {
    NOTREACHED();
  }
}

void SSLClientSocketMac::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetOmniboxSpeculation();
  } else {
    NOTREACHED();
  }
}

bool SSLClientSocketMac::WasEverUsed() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->WasEverUsed();
  }
  NOTREACHED();
  return false;
}

bool SSLClientSocketMac::UsingTCPFastOpen() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->UsingTCPFastOpen();
  }
  NOTREACHED();
  return false;
}

int SSLClientSocketMac::Read(IOBuffer* buf, int buf_len,
                             CompletionCallback* callback) {
  DCHECK(completed_handshake());
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
  DCHECK(completed_handshake());
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
  return transport_->socket()->SetReceiveBufferSize(size);
}

bool SSLClientSocketMac::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
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
  if (!status) {
    ssl_info->security_bits = KeySizeOfCipherSuite(suite);
    ssl_info->connection_status |=
        (suite & SSL_CONNECTION_CIPHERSUITE_MASK) <<
        SSL_CONNECTION_CIPHERSUITE_SHIFT;
  }

  if (ssl_config_.ssl3_fallback)
    ssl_info->connection_status |= SSL_CONNECTION_SSL3_FALLBACK;
}

void SSLClientSocketMac::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  // I'm being asked for available client certs (identities).
  // First, get the cert issuer names allowed by the server.
  std::vector<CertPrincipal> valid_issuers;
  CFArrayRef valid_issuer_names = NULL;
  if (SSLCopyDistinguishedNames(ssl_context_, &valid_issuer_names) == noErr &&
      valid_issuer_names != NULL) {
    VLOG(1) << "Server has " << CFArrayGetCount(valid_issuer_names)
            << " valid issuer names";
    int n = CFArrayGetCount(valid_issuer_names);
    for (int i = 0; i < n; i++) {
      // Parse each name into a CertPrincipal object.
      CFDataRef issuer = reinterpret_cast<CFDataRef>(
          CFArrayGetValueAtIndex(valid_issuer_names, i));
      CertPrincipal p;
      if (p.ParseDistinguishedName(CFDataGetBytePtr(issuer),
                                   CFDataGetLength(issuer))) {
        valid_issuers.push_back(p);
      }
    }
    CFRelease(valid_issuer_names);
  }

  // Now get the available client certs whose issuers are allowed by the server.
  cert_request_info->host_and_port = host_and_port_.ToString();
  cert_request_info->client_certs.clear();
  // TODO(rch):  we should consider passing a host-port pair as the first
  // argument to X509Certificate::GetSSLClientCertificates.
  X509Certificate::GetSSLClientCertificates(host_and_port_.host(),
                                            valid_issuers,
                                            &cert_request_info->client_certs);
  VLOG(1) << "Asking user to choose between "
          << cert_request_info->client_certs.size() << " client certs...";
}

SSLClientSocket::NextProtoStatus
SSLClientSocketMac::GetNextProto(std::string* proto) {
  proto->clear();
  return kNextProtoUnsupported;
}

int SSLClientSocketMac::InitializeSSLContext() {
  VLOG(1) << "----- InitializeSSLContext";
  OSStatus status = noErr;

  status = SSLNewContext(false, &ssl_context_);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetProtocolVersionEnabled(ssl_context_,
                                        kSSLProtocol2,
                                        false);
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

  std::vector<SSLCipherSuite> enabled_ciphers =
      g_enabled_cipher_suites.Get().ciphers();

  CipherSuiteIsDisabledFunctor is_disabled_cipher(
      ssl_config_.disabled_cipher_suites);
  std::vector<SSLCipherSuite>::iterator new_end =
      std::remove_if(enabled_ciphers.begin(), enabled_ciphers.end(),
                     is_disabled_cipher);
  if (new_end != enabled_ciphers.end())
    enabled_ciphers.erase(new_end, enabled_ciphers.end());

  status = SSLSetEnabledCiphers(
      ssl_context_,
      enabled_ciphers.empty() ? NULL : &enabled_ciphers[0],
      enabled_ciphers.size());

  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetIOFuncs(ssl_context_, SSLReadCallback, SSLWriteCallback);
  if (status)
    return NetErrorFromOSStatus(status);

  status = SSLSetConnection(ssl_context_, this);
  if (status)
    return NetErrorFromOSStatus(status);

  // Passing the domain name enables the server_name TLS extension (SNI).
  status = SSLSetPeerDomainName(ssl_context_,
                                host_and_port_.host().data(),
                                host_and_port_.host().length());
  if (status)
    return NetErrorFromOSStatus(status);

  // Disable certificate verification within Secure Transport; we'll
  // be handling that ourselves.
  status = SSLSetEnableCertVerify(ssl_context_, false);
  if (status)
    return NetErrorFromOSStatus(status);

  if (ssl_config_.send_client_cert) {
    status = SetClientCert();
    if (status)
      return NetErrorFromOSStatus(status);
    return OK;
  }

  // Concatenate the hostname and peer address to use as the peer ID. To
  // resume a session, we must connect to the same server on the same port
  // using the same hostname (i.e., localhost and 127.0.0.1 are considered
  // different peers, which puts us through certificate validation again
  // and catches hostname/certificate name mismatches.
  AddressList address;
  int rv = transport_->socket()->GetPeerAddress(&address);
  if (rv != OK)
    return rv;
  const struct addrinfo* ai = address.head();
  std::string peer_id(host_and_port_.ToString());
  peer_id += std::string(reinterpret_cast<char*>(ai->ai_addr),
                         ai->ai_addrlen);
  // SSLSetPeerID() treats peer_id as a binary blob, and makes its
  // own copy.
  status = SSLSetPeerID(ssl_context_, peer_id.data(), peer_id.length());
  if (status)
    return NetErrorFromOSStatus(status);

  return OK;
}

void SSLClientSocketMac::DoConnectCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(user_connect_callback_);

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
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    // If there is no connect callback available to call, we are
    // renegotiating (which occurs because we are in the middle of a Read
    // when the renegotiation process starts).  So we complete the Read
    // here.
    if (!user_connect_callback_) {
      DoReadCallback(rv);
      return;
    }
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
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

  if (!completed_handshake()) {
    OnHandshakeIOComplete(result);
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

  if (!completed_handshake()) {
    OnHandshakeIOComplete(result);
    return;
  }

  // If paused because too much data is in flight, try writing again and make
  // the promised callback.
  if (user_write_buf_ && send_buffer_.size() < kWriteSizeResumeLimit) {
    int rv = DoPayloadWrite();
    if (rv != ERR_IO_PENDING)
      DoWriteCallback(rv);
  }
}

int SSLClientSocketMac::DoHandshakeLoop(int last_io_result) {
  DCHECK(next_handshake_state_ != STATE_NONE);
  int rv = last_io_result;
  do {
    State state = next_handshake_state_;
    next_handshake_state_ = STATE_NONE;
    switch (state) {
      case STATE_HANDSHAKE:
        // Do the SSL/TLS handshake.
        rv = DoHandshake();
        break;
      case STATE_VERIFY_CERT:
        // Kick off server certificate validation.
        rv = DoVerifyCert();
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        // Check the results of the server certificate validation.
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_COMPLETED_RENEGOTIATION:
        // The renegotiation handshake has completed, and the Read() call
        // that was interrupted by the renegotiation needs to be resumed in
        // order to to satisfy the original caller's request.
        rv = DoCompletedRenegotiation(rv);
        break;
      case STATE_COMPLETED_HANDSHAKE:
        next_handshake_state_ = STATE_COMPLETED_HANDSHAKE;
        // This is the end of our state machine, so return.
        return rv;
      default:
        rv = ERR_UNEXPECTED;
        NOTREACHED() << "unexpected state";
        break;
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketMac::DoHandshake() {
  client_cert_requested_ = false;

  OSStatus status;
  if (!renegotiating_) {
    status = SSLHandshake(ssl_context_);
  } else {
    // Renegotiation can only be detected by a call to DoPayloadRead(),
    // which means |user_read_buf_| should be valid.
    DCHECK(user_read_buf_);

    // On OS X 10.5.x, SSLSetSessionOption with
    // kSSLSessionOptionBreakOnServerAuth is broken for renegotiation, as
    // SSLRead() does not internally handle errSSLServerAuthCompleted being
    // returned during handshake. In order to support certificate validation
    // after a renegotiation, SSLRead() sets |renegotiating_| to be true and
    // returns errSSLWouldBlock when it detects an attempt to read the
    // ServerHello after responding to a HelloRequest. It would be
    // appropriate to call SSLHandshake() at this point to restart the
    // handshake state machine, however, on 10.5.x, SSLHandshake() is buggy
    // and will always return noErr (indicating handshake completion),
    // without doing any actual work. Because of this, the only way to
    // advance SecureTransport's internal handshake state machine is to
    // continuously call SSLRead() until the handshake is marked complete.
    // Once the handshake is completed, if it completed successfully, the
    // user read callback is invoked with |bytes_read_after_renegotiation_|
    // as the callback result. On 10.6.0+, both errSSLServerAuthCompleted
    // and SSLHandshake() work as expected, so this strange workaround is
    // only necessary while OS X 10.5.x is still supported.
    bytes_read_after_renegotiation_ = 0;
    status = SSLRead(ssl_context_, user_read_buf_->data(),
                     user_read_buf_len_, &bytes_read_after_renegotiation_);
    if (bytes_read_after_renegotiation_ > 0) {
      // With SecureTransport, as of 10.6.5, if application data is read,
      // then the handshake should be completed. This is because
      // SecureTransport does not (yet) support exchanging application data
      // in the midst of handshakes. This is permitted in the TLS
      // specification, as peers may exchange messages using the previous
      // cipher spec up until they exchange ChangeCipherSpec messages.
      // However, in addition to SecureTransport not supporting this, we do
      // not permit callers to enter Read() or Write() when a handshake is
      // occurring, in part due to the deception that happens in
      // SSLWriteCallback(). Thus we need to make sure the handshake is
      // truly completed before processing application data, and if any was
      // read before the handshake is completed, it will be dropped and the
      // connection aborted.
      SSLSessionState session_state = kSSLIdle;
      status = SSLGetSessionState(ssl_context_, &session_state);
      if (session_state != kSSLConnected)
        status = errSSLProtocol;
    }
  }

  SSLClientCertificateState client_cert_state;
  if (SSLGetClientCertificateState(ssl_context_, &client_cert_state) != noErr)
    client_cert_state = kSSLClientCertNone;
  if (client_cert_state > kSSLClientCertNone)
    client_cert_requested_ = true;

  int net_error = ERR_FAILED;
  switch (status) {
    case noErr:
      return DidCompleteHandshake();
    case errSSLWouldBlock:
      next_handshake_state_ = STATE_HANDSHAKE;
      return ERR_IO_PENDING;
    case errSSLClosedGraceful:
      // The server unexpectedly closed on us.
      net_error = ERR_SSL_PROTOCOL_ERROR;
      break;
    case errSSLClosedAbort:
    case errSSLPeerHandshakeFail:
      if (client_cert_requested_) {
        if (!ssl_config_.send_client_cert) {
          // The server aborted, likely due to requiring a client certificate
          // and one wasn't sent.
          VLOG(1) << "Server requested SSL cert during handshake";
          net_error = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
        } else {
          // The server aborted, likely due to not liking the client
          // certificate that was sent.
          LOG(WARNING) << "Server aborted SSL handshake";
          net_error = ERR_BAD_SSL_CLIENT_AUTH_CERT;
        }
        // Don't fall through - the error was intentionally remapped.
        break;
      }
      // Fall through if a client cert wasn't requested.
    default:
      net_error = NetErrorFromOSStatus(status);
      DCHECK(!IsCertificateError(net_error));
      if (!ssl_config_.send_client_cert &&
         (client_cert_state == kSSLClientCertRejected ||
          net_error == ERR_BAD_SSL_CLIENT_AUTH_CERT)) {
        // The server unexpectedly sent a peer certificate error alert when no
        // certificate had been sent.
        net_error = ERR_SSL_PROTOCOL_ERROR;
      }
      break;
  }

  net_log_.AddEvent(NetLog::TYPE_SSL_HANDSHAKE_ERROR,
                    new SSLErrorParams(net_error, status));
  return net_error;
}

int SSLClientSocketMac::DoVerifyCert() {
  next_handshake_state_ = STATE_VERIFY_CERT_COMPLETE;

  DCHECK(server_cert_);

  VLOG(1) << "DoVerifyCert...";
  int flags = 0;
  if (ssl_config_.rev_checking_enabled)
    flags |= X509Certificate::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= X509Certificate::VERIFY_EV_CERT;
  verifier_.reset(new SingleRequestCertVerifier(cert_verifier_));
  return verifier_->Verify(server_cert_, host_and_port_.host(), flags,
                           &server_cert_verify_result_,
                           &handshake_io_callback_);
}

int SSLClientSocketMac::DoVerifyCertComplete(int result) {
  DCHECK(verifier_.get());
  verifier_.reset();

  VLOG(1) << "...DoVerifyCertComplete (result=" << result << ")";
  if (IsCertificateError(result) && ssl_config_.IsAllowedBadCert(server_cert_))
    result = OK;

  if (result == OK && client_cert_requested_ &&
      !ssl_config_.send_client_cert) {
    // Caller hasn't specified a client cert, so let it know the server is
    // asking for one, and abort the connection.
    return ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
  }
  VLOG(1) << "Handshake finished! (DoVerifyCertComplete)";

  if (renegotiating_) {
    DidCompleteRenegotiation();
    return result;
  }

  // The initial handshake has completed.
  next_handshake_state_ = STATE_COMPLETED_HANDSHAKE;

  return result;
}

int SSLClientSocketMac::SetClientCert() {
  if (!ssl_config_.send_client_cert || !ssl_config_.client_cert)
    return noErr;

  base::mac::ScopedCFTypeRef<CFArrayRef> cert_refs(
      ssl_config_.client_cert->CreateClientCertificateChain());
  VLOG(1) << "SSLSetCertificate(" << CFArrayGetCount(cert_refs) << " certs)";
  OSStatus result = SSLSetCertificate(ssl_context_, cert_refs);
  if (result)
    LOG(ERROR) << "SSLSetCertificate returned OSStatus " << result;
  return result;
}

int SSLClientSocketMac::DoPayloadRead() {
  size_t processed = 0;
  OSStatus status = SSLRead(ssl_context_, user_read_buf_->data(),
                            user_read_buf_len_, &processed);
  if (status == errSSLWouldBlock && renegotiating_) {
    CHECK_EQ(static_cast<size_t>(0), processed);
    next_handshake_state_ = STATE_HANDSHAKE;
    return DoHandshakeLoop(OK);
  }
  // There's a subtle difference here in semantics of the "would block" errors.
  // In our code, ERR_IO_PENDING means the whole operation is async, while
  // errSSLWouldBlock means that the stream isn't ending (and is often returned
  // along with partial data). So even though "would block" is returned, if we
  // have data, let's just return it. This is further complicated by the fact
  // that errSSLWouldBlock is also used to short-circuit SSLRead()'s
  // transparent renegotiation, so that we can update our state machine above,
  // which otherwise would get out of sync with the SSLContextRef's internal
  // state machine.
  if (processed > 0)
    return processed;

  switch (status) {
    case errSSLClosedNoNotify:
      // TODO(wtc): Unless we have received the close_notify alert, we need to
      // return an error code indicating that the SSL connection ended
      // uncleanly, a potential truncation attack.  See http://crbug.com/18586.
      return OK;

    default:
      return NetErrorFromOSStatus(status);
  }
}

int SSLClientSocketMac::DoPayloadWrite() {
  // Too much data in flight?
  if (send_buffer_.size() > kWriteSizePauseLimit)
    return ERR_IO_PENDING;

  size_t processed = 0;
  OSStatus status = SSLWrite(ssl_context_,
                             user_write_buf_->data(),
                             user_write_buf_len_,
                             &processed);

  if (processed > 0)
    return processed;

  return NetErrorFromOSStatus(status);
}

int SSLClientSocketMac::DoCompletedRenegotiation(int result) {
  // The user had a read in progress, which was interrupted by the
  // renegotiation. Return the application data that was processed after the
  // handshake completed.
  next_handshake_state_ = STATE_COMPLETED_HANDSHAKE;
  if (result != OK)
    return result;
  return bytes_read_after_renegotiation_;
}

void SSLClientSocketMac::DidCompleteRenegotiation() {
  DCHECK(!user_connect_callback_);
  renegotiating_ = false;
  next_handshake_state_ = STATE_COMPLETED_RENEGOTIATION;
}

int SSLClientSocketMac::DidCompleteHandshake() {
  DCHECK(!server_cert_ || renegotiating_);
  VLOG(1) << "Handshake completed, next verify cert";

  scoped_refptr<X509Certificate> new_server_cert(
      GetServerCert(ssl_context_));
  if (!new_server_cert)
    return ERR_UNEXPECTED;

  if (renegotiating_ &&
      X509Certificate::IsSameOSCert(server_cert_->os_cert_handle(),
                                    new_server_cert->os_cert_handle())) {
    // We already verified the server certificate.  Either it is good or the
    // user has accepted the certificate error.
    DidCompleteRenegotiation();
  } else {
    server_cert_ = new_server_cert;
    next_handshake_state_ = STATE_VERIFY_CERT;
  }
  return OK;
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
  if (us->completed_handshake()) {
    // The state machine for SSLRead, located in libsecurity_ssl's
    // sslTransport.c, will attempt to fully complete the renegotiation
    // transparently in SSLRead once it reads the server's HelloRequest
    // message. In order to make sure that the server certificate is
    // (re-)verified and that any other parameters are logged (eg:
    // certificate request state), we try to detect that the
    // SSLClientSocketMac's state machine is out of sync with the
    // SSLContext's. When that happens, we break out by faking
    // errSSLWouldBlock, and set a flag so that DoPayloadRead() knows that
    // it's not actually blocked. DoPayloadRead() will then restart the
    // handshake state machine, and finally resume the original Read()
    // once it successfully completes, similar to the behaviour of
    // SSLClientSocketWin's DoDecryptPayload() and DoLoop() behave.
    SSLSessionState state;
    OSStatus status = SSLGetSessionState(us->ssl_context_, &state);
    if (status) {
      *data_length = 0;
      return status;
    }
    if (state == kSSLHandshake) {
      *data_length = 0;
      us->renegotiating_ = true;
      return errSSLWouldBlock;
    }
  }

  size_t total_read = us->recv_buffer_.size();

  int rv = 1;  // any old value to spin the loop below
  while (rv > 0 && total_read < *data_length) {
    us->read_io_buf_ = new IOBuffer(*data_length - total_read);
    rv = us->transport_->socket()->Read(us->read_io_buf_,
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
    rv = us->transport_->socket()->Write(us->write_io_buf_,
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
