// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_ERROR_ASSISTANT_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_ERROR_ASSISTANT_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "components/security_interstitials/content/ssl_error_assistant.pb.h"
#include "net/ssl/ssl_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace net {
class SSLInfo;
}

// Struct which stores data about a known MITM software pulled from the
// SSLErrorAssistant proto.
struct MITMSoftwareType {
  MITMSoftwareType(const std::string& name,
                   const std::string& issuer_common_name_regex,
                   const std::string& issuer_organization_regex);

  const std::string name;
  const std::string issuer_common_name_regex;
  const std::string issuer_organization_regex;
};

// Struct which stores data about a dynamic interstitial pulled from the
// SSLErrorAssistant proto.
struct DynamicInterstitialInfo {
  DynamicInterstitialInfo(
      const std::unordered_set<std::string>& spki_hashes,
      const std::string& issuer_common_name_regex,
      const std::string& issuer_organization_regex,
      const std::string& mitm_software_name,
      chrome_browser_ssl::DynamicInterstitial::InterstitialPageType
          interstitial_type,
      int cert_error,
      const GURL& support_url,
      bool show_only_for_nonoverridable_errors);

  DynamicInterstitialInfo(const DynamicInterstitialInfo& other);

  ~DynamicInterstitialInfo();

  const std::unordered_set<std::string> spki_hashes;
  const std::string issuer_common_name_regex;
  const std::string issuer_organization_regex;
  const std::string mitm_software_name;
  const chrome_browser_ssl::DynamicInterstitial::InterstitialPageType
      interstitial_type;
  const int cert_error;
  const GURL support_url;
  bool show_only_for_nonoverridable_errors;
};

// Helper class for SSLErrorHandler. This class is responsible for reading in
// the ssl_error_assistant protobuf and parsing through the data.
class SSLErrorAssistant {
 public:
  SSLErrorAssistant();

  SSLErrorAssistant(const SSLErrorAssistant&) = delete;
  SSLErrorAssistant& operator=(const SSLErrorAssistant&) = delete;

  ~SSLErrorAssistant();

  // Returns true if any of the SHA256 hashes in |ssl_info| is of a captive
  // portal certificate. The set of captive portal hashes is loaded on first
  // use.
  bool IsKnownCaptivePortalCertificate(const net::SSLInfo& ssl_info);

  // Returns the name of a known MITM software provider that matches the
  // certificate passed in as the |cert| parameter. Returns empty string if
  // there is no match.
  const std::string MatchKnownMITMSoftware(
      const scoped_refptr<net::X509Certificate>& cert);

  // Returns a DynamicInterstitialInfo from |dynamic_interstitial_list_| that
  // matches with |ssl_info|. If there is no match, returns null. Loads
  // |dynamic_interstitial_list_| on the first use.
  absl::optional<DynamicInterstitialInfo> MatchDynamicInterstitial(
      const net::SSLInfo& ssl_info,
      bool is_overridable = false);

  void SetErrorAssistantProto(
      std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> proto);

  // Returns the SSL error assistant config stored in the resource bundle. This
  // function is thread-safe and can safely be called multiple times.
  static std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
  GetErrorAssistantProtoFromResourceBundle();

  // Testing methods:
  void ResetForTesting();
  int GetErrorAssistantProtoVersionIdForTesting() const;

 private:
  void EnsureInitialized();

  // SPKI hashes belonging to certs treated as captive portals. Null until the
  // first time ShouldDisplayCaptiveProtalInterstitial() or
  // SetErrorAssistantProto() is called.
  std::unique_ptr<std::unordered_set<std::string>> captive_portal_spki_hashes_;

  // Data about a known MITM software pulled from the SSLErrorAssistant proto.
  // Null until MatchKnownMITMSoftware() is called.
  std::unique_ptr<std::vector<MITMSoftwareType>> mitm_software_list_;

  // Dynamic interstitial data pulled from the SSLErrorAssistant proto. Null
  // until MatchDynamicInterstitial() is called.
  std::unique_ptr<std::vector<DynamicInterstitialInfo>>
      dynamic_interstitial_list_;

  // Error assistant configuration.
  std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
      error_assistant_proto_;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_ERROR_ASSISTANT_H_
