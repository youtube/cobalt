// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CONFIG_SERVICE_H_
#define NET_BASE_SSL_CONFIG_SERVICE_H_
#pragma once

#include <vector>

#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "net/base/x509_certificate.h"

namespace net {

// A collection of SSL-related configuration settings.
struct SSLConfig {
  // Default to revocation checking.
  // Default to SSL 2.0 off, SSL 3.0 on, and TLS 1.0 on.
  SSLConfig()
      : rev_checking_enabled(true),  ssl2_enabled(false), ssl3_enabled(true),
        tls1_enabled(true), dnssec_enabled(false), mitm_proxies_allowed(false),
        false_start_enabled(true), send_client_cert(false),
        verify_ev_cert(false), ssl3_fallback(false) {
  }

  bool rev_checking_enabled;  // True if server certificate revocation
                              // checking is enabled.
  bool ssl2_enabled;  // True if SSL 2.0 is enabled.
  bool ssl3_enabled;  // True if SSL 3.0 is enabled.
  bool tls1_enabled;  // True if TLS 1.0 is enabled.
  bool dnssec_enabled;  // True if we'll accept DNSSEC chains in certificates.

  // True if we allow this connection to be MITM attacked. This sounds a little
  // worse than it is: large networks sometimes MITM attack all SSL connections
  // on egress. We want to know this because we might not have the end-to-end
  // connection that we believe that we have based on the hostname. Therefore,
  // certain certificate checks can't be performed and we can't use outside
  // knowledge about whether the server has the renegotiation extension.
  bool mitm_proxies_allowed;

  bool false_start_enabled;  // True if we'll use TLS False Start.

  // TODO(wtc): move the following members to a new SSLParams structure.  They
  // are not SSL configuration settings.

  struct CertAndStatus {
    scoped_refptr<X509Certificate> cert;
    int cert_status;
  };

  // Returns true if |cert| is one of the certs in |allowed_bad_certs|.
  // TODO(wtc): Move this to a .cc file.  ssl_config_service.cc is Windows
  // only right now, so I can't move it there.
  bool IsAllowedBadCert(X509Certificate* cert) const {
    for (size_t i = 0; i < allowed_bad_certs.size(); ++i) {
      if (cert == allowed_bad_certs[i].cert)
        return true;
    }
    return false;
  }

  // Add any known-bad SSL certificate (with its cert status) to
  // |allowed_bad_certs| that should not trigger an ERR_CERT_* error when
  // calling SSLClientSocket::Connect.  This would normally be done in
  // response to the user explicitly accepting the bad certificate.
  std::vector<CertAndStatus> allowed_bad_certs;

  // True if we should send client_cert to the server.
  bool send_client_cert;

  bool verify_ev_cert;  // True if we should verify the certificate for EV.

  bool ssl3_fallback;  // True if we are falling back to SSL 3.0 (one still
                       // needs to clear tls1_enabled).

  // The list of application level protocols supported. If set, this will
  // enable Next Protocol Negotiation (if supported). This is a list of 8-bit
  // length prefixed strings. The order of the protocols doesn't matter expect
  // for one case: if the server supports Next Protocol Negotiation, but there
  // is no overlap between the server's and client's protocol sets, then the
  // first protocol in this list will be requested by the client.
  std::string next_protos;

  scoped_refptr<X509Certificate> client_cert;
};

// The interface for retrieving the SSL configuration.  This interface
// does not cover setting the SSL configuration, as on some systems, the
// SSLConfigService objects may not have direct access to the configuration, or
// live longer than the configuration preferences.
class SSLConfigService : public base::RefCountedThreadSafe<SSLConfigService> {
 public:
  // Observer is notified when SSL config settings have changed.
  class Observer {
   public:
    // Notify observers if SSL settings have changed.  We don't check all of the
    // data in SSLConfig, just those that qualify as a user config change.
    // The following settings are considered user changes:
    //     rev_checking_enabled
    //     ssl2_enabled
    //     ssl3_enabled
    //     tls1_enabled
    virtual void OnSSLConfigChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  SSLConfigService()
      : observer_list_(ObserverList<Observer>::NOTIFY_EXISTING_ONLY) {}

  // Create an instance of SSLConfigService which retrieves the configuration
  // from the system SSL configuration, or an instance of
  // SSLConfigServiceDefaults if the current system does not have a system SSL
  // configuration.  Note: this does not handle SSLConfigService implementations
  // that are not native to their platform, such as preference-backed ones.
  static SSLConfigService* CreateSystemSSLConfigService();

  // May not be thread-safe, should only be called on the IO thread.
  virtual void GetSSLConfig(SSLConfig* config) = 0;

  // Returns true if the given hostname is known to be 'strict'. This means
  // that we will require the renegotiation extension and will always use TLS
  // (no SSLv3 fallback).
  //
  // If you wish to add an element to this list, file a bug at
  // http://crbug.com and email the link to agl AT chromium DOT org.
  static bool IsKnownStrictTLSServer(const std::string& hostname);

  // Returns true if the given hostname is known to be incompatible with TLS
  // False Start.
  static bool IsKnownFalseStartIncompatibleServer(const std::string& hostname);

  // Enables the acceptance of self-signed certificates which contain an
  // embedded DNSSEC chain proving their validity.
  static void EnableDNSSEC();
  static bool dnssec_enabled();

  // Sets a global flag which allows SSL connections to be MITM attacked. See
  // the comment about this flag in |SSLConfig|.
  static void AllowMITMProxies();
  static bool mitm_proxies_allowed();

  // Disables False Start in SSL connections.
  static void DisableFalseStart();
  // True if we use False Start for SSL and TLS.
  static bool false_start_enabled();

  // Add an observer of this service.
  void AddObserver(Observer* observer);

  // Remove an observer of this service.
  void RemoveObserver(Observer* observer);

 protected:
  friend class base::RefCountedThreadSafe<SSLConfigService>;

  virtual ~SSLConfigService() {}

  // SetFlags sets the values of several flags based on global configuration.
  static void SetSSLConfigFlags(SSLConfig*);

  // Process before/after config update.
  void ProcessConfigUpdate(const SSLConfig& orig_config,
                           const SSLConfig& new_config);

 private:
  ObserverList<Observer> observer_list_;
};

}  // namespace net

#endif  // NET_BASE_SSL_CONFIG_SERVICE_H_
