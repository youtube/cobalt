// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_
#define CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_

#include <memory>
#include <vector>

#include "base/strings/string_piece.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/doh_provider_entry.h"

class PrefRegistrySimple;
class PrefService;

namespace chrome_browser_net::secure_dns {

// Returns the subsequence of `providers` that are marked for use in the
// specified country.
net::DohProviderEntry::List ProvidersForCountry(
    const net::DohProviderEntry::List& providers,
    int country_id);

// Returns the subsequence of `providers` that are enabled, according to their
// `net::DohProviderEntry::feature` members.
net::DohProviderEntry::List SelectEnabledProviders(
    const net::DohProviderEntry::List& providers);

// When the selected DoH provider changes, call this function to update the
// Selected, Unselected, and Ignored histograms for all the included providers,
// and also for the custom provider option.  If the old or new selection is the
// custom provider option, pass an empty string as the config.
void UpdateDropdownHistograms(const net::DohProviderEntry::List& providers,
                              base::StringPiece old_config,
                              base::StringPiece new_config);
void UpdateValidationHistogram(bool valid);
void UpdateProbeHistogram(bool success);

// Returns a DNS prober configured for testing DoH servers
std::unique_ptr<DnsProbeRunner> MakeProbeRunner(
    net::DnsOverHttpsConfig doh_config,
    const DnsProbeRunner::NetworkContextGetter& network_context_getter);

// Registers the backup preference required for the DNS probes setting reset.
// TODO(crbug.com/1062698): Remove this once the privacy settings redesign
// is fully launched.
void RegisterProbesSettingBackupPref(PrefRegistrySimple* registry);

// Backs up the unneeded preference controlling DNS and captive portal probes
// once the privacy settings redesign is enabled, or restores the backup
// in case the feature is rolled back.
// TODO(crbug.com/1062698): Remove this once the privacy settings redesign
// is fully launched.
void MigrateProbesSettingToOrFromBackup(PrefService* prefs);

}  // namespace chrome_browser_net::secure_dns

#endif  // CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_
