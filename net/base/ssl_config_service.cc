// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service.h"
#include "net/base/ssl_false_start_blacklist.h"

#if defined(OS_WIN)
#include "net/base/ssl_config_service_win.h"
#elif defined(OS_MACOSX)
#include "net/base/ssl_config_service_mac.h"
#else
#include "net/base/ssl_config_service_defaults.h"
#endif

namespace net {

SSLConfig::CertAndStatus::CertAndStatus() : cert_status(0) {}

SSLConfig::CertAndStatus::~CertAndStatus() {}

SSLConfig::SSLConfig()
    : rev_checking_enabled(true), ssl3_enabled(true),
      tls1_enabled(true), dnssec_enabled(false), snap_start_enabled(false),
      dns_cert_provenance_checking_enabled(false),
      session_resume_disabled(false), mitm_proxies_allowed(false),
      false_start_enabled(true), send_client_cert(false),
      verify_ev_cert(false), ssl3_fallback(false) {
}

SSLConfig::~SSLConfig() {
}

bool SSLConfig::IsAllowedBadCert(X509Certificate* cert) const {
  for (size_t i = 0; i < allowed_bad_certs.size(); ++i) {
    if (cert->Equals(allowed_bad_certs[i].cert))
      return true;
  }
  return false;
}

SSLConfigService::SSLConfigService()
    : observer_list_(ObserverList<Observer>::NOTIFY_EXISTING_ONLY) {
}

// static
SSLConfigService* SSLConfigService::CreateSystemSSLConfigService() {
#if defined(OS_WIN)
  return new SSLConfigServiceWin;
#elif defined(OS_MACOSX)
  return new SSLConfigServiceMac;
#else
  return new SSLConfigServiceDefaults;
#endif
}

// static
bool SSLConfigService::IsKnownStrictTLSServer(const std::string& hostname) {
  // If you wish to add an entry to this list, please contact agl AT chromium
  // DOT org.
  //
  // If this list starts growing, it'll need to be something more efficient
  // than a linear list.
  static const char kStrictServers[][22] = {
      "www.google.com",
      "mail.google.com",
      "www.gmail.com",
      "docs.google.com",
      "clients1.google.com",
      "sunshinepress.org",
      "www.sunshinepress.org",

      // Removed until we update the XMPP servers with the renegotiation
      // extension.
      // "gmail.com",
  };

  for (size_t i = 0; i < arraysize(kStrictServers); i++) {
    // Note that the hostname is normalised to lower-case by this point.
    if (strcmp(hostname.c_str(), kStrictServers[i]) == 0)
      return true;
  }

  return false;
}

// static
bool SSLConfigService::IsKnownFalseStartIncompatibleServer(
    const std::string& hostname) {
  return SSLFalseStartBlacklist::IsMember(hostname.c_str());
}

static bool g_dnssec_enabled = false;
static bool g_false_start_enabled = true;
static bool g_mitm_proxies_allowed = false;
static bool g_snap_start_enabled = false;
static bool g_dns_cert_provenance_checking = false;

// static
void SSLConfigService::EnableDNSSEC() {
  g_dnssec_enabled = true;
}

// static
bool SSLConfigService::dnssec_enabled() {
  return g_dnssec_enabled;
}

// static
void SSLConfigService::EnableSnapStart() {
  g_snap_start_enabled = true;
}

// static
bool SSLConfigService::snap_start_enabled() {
  return g_snap_start_enabled;
}

// static
void SSLConfigService::AllowMITMProxies() {
  g_mitm_proxies_allowed = true;
}

// static
bool SSLConfigService::mitm_proxies_allowed() {
  return g_mitm_proxies_allowed;
}

// static
void SSLConfigService::DisableFalseStart() {
  g_false_start_enabled = false;
}

// static
bool SSLConfigService::false_start_enabled() {
  return g_false_start_enabled;
}

// static
void SSLConfigService::EnableDNSCertProvenanceChecking() {
  g_dns_cert_provenance_checking = true;
}

// static
bool SSLConfigService::dns_cert_provenance_checking_enabled() {
  return g_dns_cert_provenance_checking;
}

void SSLConfigService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SSLConfigService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

SSLConfigService::~SSLConfigService() {
}

// static
void SSLConfigService::SetSSLConfigFlags(SSLConfig* ssl_config) {
  ssl_config->dnssec_enabled = g_dnssec_enabled;
  ssl_config->false_start_enabled = g_false_start_enabled;
  ssl_config->mitm_proxies_allowed = g_mitm_proxies_allowed;
  ssl_config->snap_start_enabled = g_snap_start_enabled;
  ssl_config->dns_cert_provenance_checking_enabled =
      g_dns_cert_provenance_checking;
}

void SSLConfigService::ProcessConfigUpdate(const SSLConfig& orig_config,
                                           const SSLConfig& new_config) {
  if (orig_config.rev_checking_enabled != new_config.rev_checking_enabled ||
      orig_config.ssl3_enabled != new_config.ssl3_enabled ||
      orig_config.tls1_enabled != new_config.tls1_enabled) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSSLConfigChanged());
  }
}

}  // namespace net
