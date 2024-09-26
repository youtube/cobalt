// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COOKIE_SETTINGS_H_
#define SERVICES_NETWORK_COOKIE_SETTINGS_H_

#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "net/base/features.h"
#include "net/base/network_delegate.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "services/network/public/cpp/session_cookie_delete_predicate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace net {
class SiteForCookies;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace network {

// Handles cookie access and deletion logic for the network service.
class COMPONENT_EXPORT(NETWORK_SERVICE) CookieSettings
    : public content_settings::CookieSettingsBase {
 public:
  CookieSettings();

  CookieSettings(const CookieSettings&) = delete;
  CookieSettings& operator=(const CookieSettings&) = delete;

  ~CookieSettings() override;

  void set_content_settings(const ContentSettingsForOneType& content_settings) {
    content_settings_ = content_settings;
  }

  void set_block_third_party_cookies(bool block_third_party_cookies) {
    block_third_party_cookies_ = block_third_party_cookies;
  }

  bool are_third_party_cookies_blocked() const {
    return block_third_party_cookies_;
  }

  void set_secure_origin_cookies_allowed_schemes(
      const std::vector<std::string>& secure_origin_cookies_allowed_schemes) {
    secure_origin_cookies_allowed_schemes_.clear();
    secure_origin_cookies_allowed_schemes_.insert(
        secure_origin_cookies_allowed_schemes.begin(),
        secure_origin_cookies_allowed_schemes.end());
  }

  void set_matching_scheme_cookies_allowed_schemes(
      const std::vector<std::string>& matching_scheme_cookies_allowed_schemes) {
    matching_scheme_cookies_allowed_schemes_.clear();
    matching_scheme_cookies_allowed_schemes_.insert(
        matching_scheme_cookies_allowed_schemes.begin(),
        matching_scheme_cookies_allowed_schemes.end());
  }

  void set_third_party_cookies_allowed_schemes(
      const std::vector<std::string>& third_party_cookies_allowed_schemes) {
    third_party_cookies_allowed_schemes_.clear();
    third_party_cookies_allowed_schemes_.insert(
        third_party_cookies_allowed_schemes.begin(),
        third_party_cookies_allowed_schemes.end());
  }

  void set_content_settings_for_legacy_cookie_access(
      const ContentSettingsForOneType& settings) {
    settings_for_legacy_cookie_access_ = settings;
  }

  void set_storage_access_grants(const ContentSettingsForOneType& settings) {
    storage_access_grants_ = settings;
  }

  void set_top_level_storage_access_grants(
      const ContentSettingsForOneType& settings) {
    top_level_storage_access_grants_ = settings;
  }

  // Returns a predicate that takes the domain of a cookie and a bool whether
  // the cookie is secure and returns true if the cookie should be deleted on
  // exit.
  DeleteCookiePredicate CreateDeleteCookieOnExitPredicate() const;

  // content_settings::CookieSettingsBase:
  ContentSetting GetSettingForLegacyCookieAccess(
      const std::string& cookie_domain) const override;
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const override;

  // Returns kStateDisallowed iff the given |url| has to be requested over
  // connection that is not tracked by the server. Usually is kStateAllowed,
  // unless user privacy settings block cookies from being get or set.
  // It may be set to kPartitionedStateAllowedOnly if the request allows
  // partitioned state to be sent over the connection, but unpartitioned
  // state should be blocked.
  net::NetworkDelegate::PrivacySetting IsPrivacyModeEnabled(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin,
      net::CookieSettingOverrides overrides) const;

  // Returns true if the given cookie is accessible according to user
  // cookie-blocking settings. Assumes that the cookie is otherwise accessible
  // (i.e. that the cookie is otherwise valid with no other exclusion reasons).
  bool IsCookieAccessible(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const net::SiteForCookies& site_for_cookies,
                          const absl::optional<url::Origin>& top_frame_origin,
                          net::CookieSettingOverrides overrides) const;

  // Annotates `maybe_included_cookies` and `excluded_cookies` with
  // ExclusionReasons if needed, per user's cookie blocking settings, and
  // ensures that all excluded cookies from `maybe_included_cookies` are moved
  // to `excluded_cookies`.  Returns false if the CookieSettings blocks access
  // to all cookies; true otherwise. Does not change the relative ordering of
  // the cookies in `maybe_included_cookies`, since this order is important when
  // building the cookie line.
  bool AnnotateAndMoveUserBlockedCookies(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin* top_frame_origin,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieSettingOverrides overrides,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies) const;

 private:
  // Returns whether third-party cookie blocking should be bypassed (i.e. always
  // allow the cookie regardless of cookie content settings and third-party
  // cookie blocking settings.
  // This just checks the scheme of the |url| and |site_for_cookies|:
  //  - Allow cookies if the |site_for_cookies| is a chrome:// scheme URL, and
  //    the |url| has a secure scheme.
  //  - Allow cookies if the |site_for_cookies| and the |url| match in scheme
  //    and both have the Chrome extensions scheme.
  bool ShouldAlwaysAllowCookies(const GURL& url,
                                const GURL& first_party_url) const;

  // content_settings::CookieSettingsBase:
  ContentSetting GetCookieSettingInternal(
      const GURL& url,
      const GURL& first_party_url,
      bool is_third_party_request,
      net::CookieSettingOverrides overrides,
      content_settings::SettingSource* source) const override;

  // An enum that represents the scope of cookies to which the user's
  // third-party-cookie-blocking setting applies, in a given context.
  enum class ThirdPartyBlockingScope {
    // Access to all cookies (partitioned or unpartitioned) is blocked in this
    // context.
    kUnpartitionedAndPartitioned,
    // Access to unpartitioned cookies is blocked in this context, but access to
    // partitioned cookies is allowed.
    kUnpartitionedOnly,
  };

  class CookieSettingWithMetadata {
   public:
    CookieSettingWithMetadata(
        ContentSetting cookie_setting,
        absl::optional<ThirdPartyBlockingScope> third_party_blocking_scope);

    // Returns true iff the setting is "block" due to the user's
    // third-party-cookie-blocking setting.
    bool BlockedByThirdPartyCookieBlocking() const;

    // Returns whether the given cookie should be allowed to be sent, according
    // to the user's settings. Assumes that the `cookie.access_result` has been
    // correctly filled in by the cookie store. Note that the cookie may be
    // "excluded" for other reasons, even if this method returns true.
    bool IsCookieAllowed(const net::CanonicalCookie& cookie) const;

    // Computes the PrivacySetting that should be used in this context.
    net::NetworkDelegate::PrivacySetting PrivacySetting() const;

    ContentSetting cookie_setting() const { return cookie_setting_; }

   private:
    // Returns true iff a Partitioned cookie could be allowed to be sent in this
    // context.
    bool IsPartitionedStateAllowed() const;

    // The setting itself.
    ContentSetting cookie_setting_;

    // The scope of cookies blocked by third-party-cookie-blocking.  The scope
    // must only be nullopt if `cookie_setting_` is not "allow", and if the
    // reason for blocking cookies is the third-party cookie blocking setting
    // (rather than a site-specific setting).
    absl::optional<ThirdPartyBlockingScope> third_party_blocking_scope_;
  };

  // Determines the scope of third-party-cookie-blocking, i.e. whether it
  // applies to all cookies or just unpartitioned cookies. Assumes that
  // checks have already determined to block third-party cookies.
  ThirdPartyBlockingScope GetThirdPartyBlockingScope(
      const GURL& first_party_url) const;

  // Returns the cookie setting for the given request, along with metadata
  // associated with the lookup. Namely, whether the setting is due to
  // third-party cookie blocking settings or not.
  CookieSettingWithMetadata GetCookieSettingWithMetadata(
      const GURL& url,
      const GURL& first_party_url,
      bool is_third_party_request,
      net::CookieSettingOverrides overrides) const;

  // An overload of the above, which determines `first_party_url` and
  // `is_third_party_request` appropriately.
  CookieSettingWithMetadata GetCookieSettingWithMetadata(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin* top_frame_origin,
      net::CookieSettingOverrides overrides) const;

  // Returns true if at least one content settings is session only.
  bool HasSessionOnlyOrigins() const;

  // Returns true if there's a matching Storage Access grant that allows access
  // in this context.
  bool IsAllowedByStorageAccessGrant(const GURL& url,
                                     const GURL& first_party_url) const;

  // Returns true if there's a matching top-level Storage Access grant that
  // allows access in this context.
  bool IsAllowedByTopLevelStorageAccessGrant(const GURL& url,
                                             const GURL& first_party_url) const;

  ContentSettingsForOneType content_settings_;
  bool block_third_party_cookies_ = false;
  std::set<std::string> secure_origin_cookies_allowed_schemes_;
  std::set<std::string> matching_scheme_cookies_allowed_schemes_;
  std::set<std::string> third_party_cookies_allowed_schemes_;
  ContentSettingsForOneType settings_for_legacy_cookie_access_;
  // Used to represent storage access grants provided by the StorageAccessAPI.
  // Will only be populated when the StorageAccessAPI feature is enabled
  // https://crbug.com/989663.
  ContentSettingsForOneType storage_access_grants_;
  // Used similarly to `storage_access_grants_`, but applicable at page-level.
  // The two permissions are in the process of being split.
  ContentSettingsForOneType top_level_storage_access_grants_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_COOKIE_SETTINGS_H_
