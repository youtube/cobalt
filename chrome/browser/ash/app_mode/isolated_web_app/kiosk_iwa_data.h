// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_DATA_H_
#define CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_DATA_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

class KioskAppDataDelegate;

class KioskIwaData : public KioskAppDataBase {
 public:
  // Size of a kiosk IWA icon in pixels.
  static constexpr int kIconSize = 128;

  static std::unique_ptr<KioskIwaData> Create(const std::string& user_id,
                                              const std::string& web_bundle_id,
                                              const GURL& update_manifest_url,
                                              KioskAppDataDelegate& delegate);

  KioskIwaData(const std::string& user_id,
               const web_app::IsolatedWebAppUrlInfo& iwa_info,
               const GURL& update_manifest_url,
               KioskAppDataDelegate& delegate);
  KioskIwaData(const KioskIwaData&) = delete;
  KioskIwaData& operator=(const KioskIwaData&) = delete;
  ~KioskIwaData() override;

  [[nodiscard]] const url::Origin& origin() const { return iwa_info_.origin(); }

  [[nodiscard]] const webapps::AppId& app_id() const {
    return iwa_info_.app_id();
  }

  [[nodiscard]] const web_package::SignedWebBundleId& web_bundle_id() const {
    return iwa_info_.web_bundle_id();
  }

  [[nodiscard]] const GURL& update_manifest_url() const {
    return update_manifest_url_;
  }

  void Update(const std::string& title,
              const web_app::IconBitmaps& icon_bitmaps);

  // Loads the locally cached data. Returns true on success.
  bool LoadFromCache();

  void OnIconLoadDone(std::optional<gfx::ImageSkia> icon);

 private:
  const web_app::IsolatedWebAppUrlInfo iwa_info_;
  const GURL update_manifest_url_;
  const raw_ref<KioskAppDataDelegate> delegate_;

  base::WeakPtrFactory<KioskIwaData> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_DATA_H_
