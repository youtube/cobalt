// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_SHORTCUT_PUBLISHER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_SHORTCUT_PUBLISHER_H_

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/publishers/compressed_icon_getter.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {
enum ResourceScaleFactor : int;
}

namespace apps {

// ShortcutPublisher parent class (in the App Service sense) for all shortcut
// publishers. See components/services/app_service/README.md.
class ShortcutPublisher : public CompressedIconGetter {
 public:
  explicit ShortcutPublisher(AppServiceProxy* proxy);
  ShortcutPublisher(const ShortcutPublisher&) = delete;
  ShortcutPublisher& operator=(const ShortcutPublisher&) = delete;
  virtual ~ShortcutPublisher();

  // Registers this ShortcutPublisher to AppServiceProxy, allowing it to receive
  // App Service API calls. This function must be called after the object's
  // creation, and can't be called in the constructor function to avoid
  // receiving API calls before being fully constructed and ready. This should
  // be called immediately before the first call to ShortcutPublisher::Publish
  // that sends the initial list of apps to the App Service.
  void RegisterShortcutPublisher(AppType app_type);

  // Launches a shortcut identified by `local_shortcut_id` in the app identified
  // by 'host_app_id`. `display_id` contains the id of the display from which
  // the shortcut will be launched. display::kInvalidDisplayId means that the
  // default display for new windows will be used. See `display::Screen` for
  // details.
  virtual void LaunchShortcut(const std::string& host_app_id,
                              const std::string& local_shortcut_id,
                              int64_t display_id) = 0;

  // Removes the shortcut identified by `local_shortcut_id` in the app
  // identified by 'host_app_id`. This request will be sent to shortcut
  // publisher to remove shortcut from the platform published it.
  virtual void RemoveShortcut(const std::string& host_app_id,
                              const std::string& local_shortcut_id,
                              UninstallSource uninstall_source) = 0;

  // CompressedIconGetter override.
  void GetCompressedIconData(const std::string& shortcut_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             LoadIconCallback callback) override;

 protected:
  // Publish one `delta` to AppServiceProxy. Should be called whenever the
  // shortcut represented by `delta` undergoes some state change to inform
  // AppServiceProxy of the change. Ensure that RegisterShortcutPublisher() has
  // been called before the first call to this method.
  void PublishShortcut(ShortcutPtr delta);

  // Calls when shortcut represented by shortcut id `id` has been removed from
  // the shortcut publisher, and needs to be removed from ShortcutRegistryCache.
  void ShortcutRemoved(const ShortcutId& id);

  AppServiceProxy* proxy() { return proxy_; }

 private:
  const raw_ptr<AppServiceProxy> proxy_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_SHORTCUT_PUBLISHER_H_
