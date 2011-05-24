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
      tls1_enabled(true), dnssec_enabled(false),
      dns_cert_provenance_checking_enabled(false),
      false_start_enabled(true),
      send_client_cert(false), verify_ev_cert(false), ssl3_fallback(false) {
}

SSLConfig::~SSLConfig() {
}

bool SSLConfig::IsAllowedBadCert(X509Certificate* cert,
                                 int* cert_status) const {
  for (size_t i = 0; i < allowed_bad_certs.size(); ++i) {
    if (cert->Equals(allowed_bad_certs[i].cert)) {
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
  return SSLFalseStartBlacklist::IsMember(hostname.c_str());
}

static bool g_dnssec_enabled = false;
static bool g_false_start_enabled = true;
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

// static
bool SSLConfigService::IsSNIAvailable(SSLConfigService* service) {
  if (!service)
    return false;

  SSLConfig ssl_config;
  service->GetSSLConfig(&ssl_config);
  return ssl_config.tls1_enabled;
}

}  // namespace net
