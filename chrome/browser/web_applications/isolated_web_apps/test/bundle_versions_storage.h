// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_BUNDLE_VERSIONS_MAP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_BUNDLE_VERSIONS_MAP_H_

#include "base/containers/flat_map.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace web_app::test {

// Stores the bundles & their corresponding update manifests in the following
// format:
//  * <base_url>/<web_bundle_id>/update_manifest.json
//  * <base_url>/<web_bundle_id>/<version>.swbn
class BundleVersionsStorage {
 public:
  BundleVersionsStorage();
  ~BundleVersionsStorage();

  // Returns the full URL to the update manifest for `web_bundle_id` relative to
  // `base_url`.
  static GURL GetUpdateManifestUrl(
      const GURL& base_url,
      const web_package::SignedWebBundleId& web_bundle_id);

  // Must be called once at startup.
  void SetBaseUrl(const GURL& base_url);

  // Adds a bundle to the storage and starts tracking it in the corresponding
  // update manifest. Returns the full URL to this bundle relative to
  // `base_url`.
  GURL AddBundle(
      std::unique_ptr<BundledIsolatedWebApp> bundle,
      std::optional<std::vector<UpdateChannel>> update_channels = std::nullopt);

  // Removes the bundle with `version` for `web_bundle_id` and stops tracking it
  // in the corresponding update manifest. Will CHECK if this bundle is not
  // currently served.
  void RemoveBundle(const web_package::SignedWebBundleId& web_bundle_id,
                    const base::Version& version);

  // Returns the full URL to the update manifest for `web_bundle_id`.
  GURL GetUpdateManifestUrl(
      const web_package::SignedWebBundleId& web_bundle_id) const;

  // Returns the update manifest for `web_bundle_id`. Will CHECK if there are no
  // bundles served for this `web_bundle_id`.
  base::Value::Dict GetUpdateManifest(
      const web_package::SignedWebBundleId& web_bundle_id) const;

  using BundleOrUpdateManifest =
      absl::variant<BundledIsolatedWebApp*, base::Value::Dict>;
  // Handles the following routes:
  //  * /<web_bundle_id>/update_manifest.json
  //  * /<web_bundle_id>/<version>.swbn
  std::optional<BundleOrUpdateManifest> GetResource(const std::string& route);

 private:
  struct BundleInfo;

  std::optional<GURL> base_url_;
  base::flat_map<web_package::SignedWebBundleId,
                 base::flat_map<base::Version, std::unique_ptr<BundleInfo>>>
      bundle_versions_per_id_;
};

}  // namespace web_app::test

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_BUNDLE_VERSIONS_MAP_H_
