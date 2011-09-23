// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service.h"

#include "net/base/ssl_config_service_defaults.h"
#include "net/base/ssl_false_start_blacklist.h"

namespace net {

SSLConfig::CertAndStatus::CertAndStatus() : cert_status(0) {}

SSLConfig::CertAndStatus::~CertAndStatus() {}

SSLConfig::SSLConfig()
    : rev_checking_enabled(true), ssl3_enabled(true),
      tls1_enabled(true),
      dns_cert_provenance_checking_enabled(false), cached_info_enabled(false),
      origin_bound_certs_enabled(false),
      false_start_enabled(true),
      send_client_cert(false), verify_ev_cert(false), ssl3_fallback(false) {
}

SSLConfig::~SSLConfig() {
}

bool SSLConfig::IsAllowedBadCert(X509Certificate* cert,
                                 CertStatus* cert_status) const {
  std::string der_cert;
  if (!cert->GetDEREncoded(&der_cert))
    return false;
  return IsAllowedBadCert(der_cert, cert_status);
}

bool SSLConfig::IsAllowedBadCert(const base::StringPiece& der_cert,
                                 CertStatus* cert_status) const {
  for (size_t i = 0; i < allowed_bad_certs.size(); ++i) {
    if (der_cert == allowed_bad_certs[i].der_cert) {
      if (cert_status)
        *cert_status = allowed_bad_certs[i].cert_status;
      return true;
    }
  }
  return false;
}

SSLConfigService::SSLConfigService()
    : observer_list_(ObserverList<Observer>::NOTIFY_EXISTING_ONLY) {
}

// static
bool SSLConfigService::IsKnownFalseStartIncompatibleServer(
    const std::string& hostname) {
  return SSLFalseStartBlacklist::IsMember(hostname);
}

static bool g_cached_info_enabled = false;
static bool g_origin_bound_certs_enabled = false;
static bool g_false_start_enabled = true;
static bool g_dns_cert_provenance_checking = false;

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

// static
void SSLConfigService::EnableCachedInfo() {
  g_cached_info_enabled = true;
}

// static
bool SSLConfigService::cached_info_enabled() {
  return g_cached_info_enabled;
}

// static
void SSLConfigService::EnableOriginBoundCerts() {
  g_origin_bound_certs_enabled = true;
}

// static
bool SSLConfigService::origin_bound_certs_enabled() {
  return g_origin_bound_certs_enabled;
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
  ssl_config->false_start_enabled = g_false_start_enabled;
  ssl_config->dns_cert_provenance_checking_enabled =
      g_dns_cert_provenance_checking;
  ssl_config->cached_info_enabled = g_cached_info_enabled;
  ssl_config->origin_bound_certs_enabled = g_origin_bound_certs_enabled;
}

void SSLConfigService::ProcessConfigUpdate(const SSLConfig& orig_config,
                                           const SSLConfig& new_config) {
  bool config_changed =
      (orig_config.rev_checking_enabled != new_config.rev_checking_enabled) ||
      (orig_config.ssl3_enabled != new_config.ssl3_enabled) ||
      (orig_config.tls1_enabled != new_config.tls1_enabled) ||
      (orig_config.disabled_cipher_suites.size() !=
       new_config.disabled_cipher_suites.size());
  if (!config_changed) {
    config_changed = (orig_config.disabled_cipher_suites !=
        new_config.disabled_cipher_suites);
  }

  if (config_changed)
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSSLConfigChanged());
}

// static
bool SSLConfigService::IsSNIAvailable(SSLConfigService* service) {
  if (!service)
    return false;

  SSLConfig ssl_config;
  service->GetSSLConfig(&ssl_config);
  return ssl_config.tls1_enabled;
}

}  // namespace net
