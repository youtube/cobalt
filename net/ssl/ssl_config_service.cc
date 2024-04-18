// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "net/base/features.h"

namespace net {

namespace {

// Checks if the config-service managed fields in two SSLContextConfigs are the
// same.
bool SSLContextConfigsAreEqual(const net::SSLContextConfig& config1,
                               const net::SSLContextConfig& config2) {
  return std::tie(config1.version_min, config1.version_max,
                  config1.disabled_cipher_suites, config1.post_quantum_enabled,
                  config1.ech_enabled, config1.insecure_hash_override) ==
         std::tie(config2.version_min, config2.version_max,
                  config2.disabled_cipher_suites, config2.post_quantum_enabled,
                  config2.ech_enabled, config2.insecure_hash_override);
}

}  // namespace

SSLContextConfig::SSLContextConfig() = default;
SSLContextConfig::SSLContextConfig(const SSLContextConfig&) = default;
SSLContextConfig::SSLContextConfig(SSLContextConfig&&) = default;
SSLContextConfig::~SSLContextConfig() = default;
SSLContextConfig& SSLContextConfig::operator=(const SSLContextConfig&) =
    default;
SSLContextConfig& SSLContextConfig::operator=(SSLContextConfig&&) = default;

bool SSLContextConfig::EncryptedClientHelloEnabled() const {
  return ech_enabled &&
         base::FeatureList::IsEnabled(features::kEncryptedClientHello);
}

bool SSLContextConfig::InsecureHashesInTLSHandshakesEnabled() const {
  return insecure_hash_override.value_or(
      base::FeatureList::IsEnabled(features::kSHA1ServerSignature));
}

SSLConfigService::SSLConfigService()
    : observer_list_(base::ObserverListPolicy::EXISTING_ONLY) {}

SSLConfigService::~SSLConfigService() = default;

void SSLConfigService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SSLConfigService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SSLConfigService::NotifySSLContextConfigChange() {
  for (auto& observer : observer_list_)
    observer.OnSSLContextConfigChanged();
}

bool SSLConfigService::SSLContextConfigsAreEqualForTesting(
    const SSLContextConfig& config1,
    const SSLContextConfig& config2) {
  return SSLContextConfigsAreEqual(config1, config2);
}

void SSLConfigService::ProcessConfigUpdate(const SSLContextConfig& old_config,
                                           const SSLContextConfig& new_config,
                                           bool force_notification) {
  // Do nothing if the configuration hasn't changed.
  if (!SSLContextConfigsAreEqual(old_config, new_config) || force_notification)
    NotifySSLContextConfigChange();
}

}  // namespace net
