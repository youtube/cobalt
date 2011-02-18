// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/nss_ssl_util.h"

#include <nss.h>
#include <secerr.h>
#include <ssl.h>
#include <sslerr.h>

#include <string>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/nss_util.h"
#include "base/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"

namespace net {

class NSSSSLInitSingleton {
 public:
  NSSSSLInitSingleton() {
    base::EnsureNSSInit();

    NSS_SetDomesticPolicy();

#if defined(USE_SYSTEM_SSL)
    // Use late binding to avoid scary but benign warning
    // "Symbol `SSL_ImplementedCiphers' has different size in shared object,
    //  consider re-linking"
    // TODO(wtc): Use the new SSL_GetImplementedCiphers and
    // SSL_GetNumImplementedCiphers functions when we require NSS 3.12.6.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=496993.
    const PRUint16* pSSL_ImplementedCiphers = static_cast<const PRUint16*>(
        dlsym(RTLD_DEFAULT, "SSL_ImplementedCiphers"));
    if (pSSL_ImplementedCiphers == NULL) {
      NOTREACHED() << "Can't get list of supported ciphers";
      return;
    }
#else
#define pSSL_ImplementedCiphers SSL_ImplementedCiphers
#endif

    // Explicitly enable exactly those ciphers with keys of at least 80 bits
    for (int i = 0; i < SSL_NumImplementedCiphers; i++) {
      SSLCipherSuiteInfo info;
      if (SSL_GetCipherSuiteInfo(pSSL_ImplementedCiphers[i], &info,
                                 sizeof(info)) == SECSuccess) {
        SSL_CipherPrefSetDefault(pSSL_ImplementedCiphers[i],
                                 (info.effectiveKeyBits >= 80));
      }
    }

    // Enable SSL.
    SSL_OptionSetDefault(SSL_SECURITY, PR_TRUE);

    // All other SSL options are set per-session by SSLClientSocket and
    // SSLServerSocket.
  }

  ~NSSSSLInitSingleton() {
    // Have to clear the cache, or NSS_Shutdown fails with SEC_ERROR_BUSY.
    SSL_ClearSessionCache();
  }
};

static base::LazyInstance<NSSSSLInitSingleton> g_nss_ssl_init_singleton(
    base::LINKER_INITIALIZED);

// Initialize the NSS SSL library if it isn't already initialized.  This must
// be called before any other NSS SSL functions.  This function is
// thread-safe, and the NSS SSL library will only ever be initialized once.
// The NSS SSL library will be properly shut down on program exit.
void EnsureNSSSSLInit() {
  // Initializing SSL causes us to do blocking IO.
  // Temporarily allow it until we fix
  //   http://code.google.com/p/chromium/issues/detail?id=59847
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  g_nss_ssl_init_singleton.Get();
}

// Map a Chromium net error code to an NSS error code.
// See _MD_unix_map_default_error in the NSS source
// tree for inspiration.
PRErrorCode MapErrorToNSS(int result) {
  if (result >=0)
    return result;

  switch (result) {
    case ERR_IO_PENDING:
      return PR_WOULD_BLOCK_ERROR;
    case ERR_ACCESS_DENIED:
    case ERR_NETWORK_ACCESS_DENIED:
      // For connect, this could be mapped to PR_ADDRESS_NOT_SUPPORTED_ERROR.
      return PR_NO_ACCESS_RIGHTS_ERROR;
    case ERR_NOT_IMPLEMENTED:
      return PR_NOT_IMPLEMENTED_ERROR;
    case ERR_INTERNET_DISCONNECTED:  // Equivalent to ENETDOWN.
      return PR_NETWORK_UNREACHABLE_ERROR;  // Best approximation.
    case ERR_CONNECTION_TIMED_OUT:
    case ERR_TIMED_OUT:
      return PR_IO_TIMEOUT_ERROR;
    case ERR_CONNECTION_RESET:
      return PR_CONNECT_RESET_ERROR;
    case ERR_CONNECTION_ABORTED:
      return PR_CONNECT_ABORTED_ERROR;
    case ERR_CONNECTION_REFUSED:
      return PR_CONNECT_REFUSED_ERROR;
    case ERR_ADDRESS_UNREACHABLE:
      return PR_HOST_UNREACHABLE_ERROR;  // Also PR_NETWORK_UNREACHABLE_ERROR.
    case ERR_ADDRESS_INVALID:
      return PR_ADDRESS_NOT_AVAILABLE_ERROR;
    case ERR_NAME_NOT_RESOLVED:
      return PR_DIRECTORY_LOOKUP_ERROR;
    default:
      LOG(WARNING) << "MapErrorToNSS " << result
                   << " mapped to PR_UNKNOWN_ERROR";
      return PR_UNKNOWN_ERROR;
  }
}

// The default error mapping function.
// Maps an NSS error code to a network error code.
int MapNSSError(PRErrorCode err) {
  // TODO(port): fill this out as we learn what's important
  switch (err) {
    case PR_WOULD_BLOCK_ERROR:
      return ERR_IO_PENDING;
    case PR_ADDRESS_NOT_SUPPORTED_ERROR:  // For connect.
    case PR_NO_ACCESS_RIGHTS_ERROR:
      return ERR_ACCESS_DENIED;
    case PR_IO_TIMEOUT_ERROR:
      return ERR_TIMED_OUT;
    case PR_CONNECT_RESET_ERROR:
      return ERR_CONNECTION_RESET;
    case PR_CONNECT_ABORTED_ERROR:
      return ERR_CONNECTION_ABORTED;
    case PR_CONNECT_REFUSED_ERROR:
      return ERR_CONNECTION_REFUSED;
    case PR_HOST_UNREACHABLE_ERROR:
    case PR_NETWORK_UNREACHABLE_ERROR:
      return ERR_ADDRESS_UNREACHABLE;
    case PR_ADDRESS_NOT_AVAILABLE_ERROR:
      return ERR_ADDRESS_INVALID;
    case PR_INVALID_ARGUMENT_ERROR:
      return ERR_INVALID_ARGUMENT;
    case PR_END_OF_FILE_ERROR:
      return ERR_CONNECTION_CLOSED;
    case PR_NOT_IMPLEMENTED_ERROR:
      return ERR_NOT_IMPLEMENTED;

    case SEC_ERROR_INVALID_ARGS:
      return ERR_INVALID_ARGUMENT;
    case SEC_ERROR_NO_KEY:
      return ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY;
    case SEC_ERROR_INVALID_KEY:
    case SSL_ERROR_SIGN_HASHES_FAILURE:
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    // A handshake (initial or renegotiation) may fail because some signature
    // (for example, the signature in the ServerKeyExchange message for an
    // ephemeral Diffie-Hellman cipher suite) is invalid.
    case SEC_ERROR_BAD_SIGNATURE:
      return ERR_SSL_PROTOCOL_ERROR;

    case SSL_ERROR_SSL_DISABLED:
      return ERR_NO_SSL_VERSIONS_ENABLED;
    case SSL_ERROR_NO_CYPHER_OVERLAP:
    case SSL_ERROR_UNSUPPORTED_VERSION:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_ERROR_HANDSHAKE_FAILURE_ALERT:
    case SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT:
    case SSL_ERROR_ILLEGAL_PARAMETER_ALERT:
      return ERR_SSL_PROTOCOL_ERROR;
    case SSL_ERROR_DECOMPRESSION_FAILURE_ALERT:
      return ERR_SSL_DECOMPRESSION_FAILURE_ALERT;
    case SSL_ERROR_BAD_MAC_ALERT:
      return ERR_SSL_BAD_RECORD_MAC_ALERT;
    case SSL_ERROR_UNSAFE_NEGOTIATION:
      return ERR_SSL_UNSAFE_NEGOTIATION;
    case SSL_ERROR_WEAK_SERVER_EPHEMERAL_DH_KEY:
      return ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY;

    default: {
      if (IS_SSL_ERROR(err)) {
        LOG(WARNING) << "Unknown SSL error " << err
                     << " mapped to net::ERR_SSL_PROTOCOL_ERROR";
        return ERR_SSL_PROTOCOL_ERROR;
      }
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
    }
  }
}

// Context-sensitive error mapping functions.
int MapNSSHandshakeError(PRErrorCode err) {
  switch (err) {
    // If the server closed on us, it is a protocol error.
    // Some TLS-intolerant servers do this when we request TLS.
    case PR_END_OF_FILE_ERROR:
      return ERR_SSL_PROTOCOL_ERROR;
    default:
      return MapNSSError(err);
  }
}

// Extra parameters to attach to the NetLog when we receive an error in response
// to a call to an NSS function.  Used instead of SSLErrorParams with
// events of type TYPE_SSL_NSS_ERROR.  Automatically looks up last PR error.
class SSLFailedNSSFunctionParams : public NetLog::EventParameters {
 public:
  // |param| is ignored if it has a length of 0.
  SSLFailedNSSFunctionParams(const std::string& function,
                             const std::string& param)
      : function_(function), param_(param), ssl_lib_error_(PR_GetError()) {
  }

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("function", function_);
    if (!param_.empty())
      dict->SetString("param", param_);
    dict->SetInteger("ssl_lib_error", ssl_lib_error_);
    return dict;
  }

 private:
  const std::string function_;
  const std::string param_;
  const PRErrorCode ssl_lib_error_;
};

void LogFailedNSSFunction(const BoundNetLog& net_log,
                          const char* function,
                          const char* param) {
  net_log.AddEvent(
      NetLog::TYPE_SSL_NSS_ERROR,
      make_scoped_refptr(new SSLFailedNSSFunctionParams(function, param)));
}

}  // namespace net
