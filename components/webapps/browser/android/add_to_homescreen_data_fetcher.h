// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_DATA_FETCHER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_DATA_FETCHER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/common/web_page_metadata_agent.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace favicon_base {
struct FaviconRawBitmapResult;
}

namespace webapps {

class InstallableManager;
struct InstallableData;

// Aysnchronously fetches and processes data needed to create a shortcut for an
// Android Home screen launcher.
class AddToHomescreenDataFetcher {
 public:
  class Observer {
   public:
    // Called when the homescreen icon title (and possibly information from the
    // web manifest) is available.
    virtual void OnUserTitleAvailable(const std::u16string& title,
                                      const GURL& url,
                                      bool is_webapk_compatible) = 0;

    // Called when all the data needed to prompt the user to add to home screen
    // is available.
    virtual void OnDataAvailable(
        const ShortcutInfo& info,
        const SkBitmap& primary_icon,
        const InstallableStatusCode installable_status) = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Initialize the fetcher by requesting the information about the page from
  // the renderer process. The initialization is asynchronous and
  // OnDidGetWebAppInstallInfo is expected to be called when finished.
  // |observer| must outlive AddToHomescreenDataFetcher.
  AddToHomescreenDataFetcher(content::WebContents* web_contents,
                             int data_timeout_ms,
                             Observer* observer);
  AddToHomescreenDataFetcher(const AddToHomescreenDataFetcher&) = delete;
  AddToHomescreenDataFetcher& operator=(const AddToHomescreenDataFetcher&) =
      delete;

  ~AddToHomescreenDataFetcher();

  // IPC message received when the initialization is finished.
  void OnDidGetWebPageMetadata(
      mojo::AssociatedRemote<mojom::WebPageMetadataAgent> webapps_render_frame,
      mojom::WebPageMetadataPtr web_page_metadata);

  // Accessors, etc.
  content::WebContents* web_contents() { return web_contents_.get(); }
  const SkBitmap& primary_icon() const { return primary_icon_; }
  ShortcutInfo& shortcut_info() { return shortcut_info_; }
  bool has_maskable_primary_icon() const { return has_maskable_primary_icon_; }

 private:
  // Called to stop the timeout timer.
  void StopTimer();

  // Called if either InstallableManager or the favicon fetch takes too long.
  void OnDataTimedout();

  // Called when InstallableManager finishes looking for a manifest and icon.
  void OnDidGetManifestAndIcons(const InstallableData& data);

  // Called when InstallableManager finishes checking for installability.
  void OnDidPerformInstallableCheck(const InstallableData& data);

  // Grabs the favicon for the current URL.
  void FetchFavicon();
  void OnFaviconFetched(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Creates an icon to display to the user to confirm the add to home screen
  // from the given |base_icon|. If |use_for_launcher| is true, the created icon
  // will also be used as the launcher icon.
  void CreateIconForView(const SkBitmap& base_icon, bool use_for_launcher);

  // Notifies the observer that the shortcut data is all available.
  void OnIconCreated(bool use_for_launcher,
                     const SkBitmap& icon_for_view,
                     bool is_icon_generated);

  base::WeakPtr<content::WebContents> web_contents_;

  raw_ptr<InstallableManager, DanglingUntriaged> installable_manager_;
  raw_ptr<Observer> observer_;

  InstallableStatusCode installable_status_code_ =
      InstallableStatusCode::NO_ERROR_DETECTED;

  // The icons must only be set on the UI thread for thread safety.
  SkBitmap raw_primary_icon_;
  SkBitmap primary_icon_;
  ShortcutInfo shortcut_info_;
  bool has_maskable_primary_icon_;

  base::CancelableTaskTracker favicon_task_tracker_;
  base::OneShotTimer data_timeout_timer_;
  base::TimeTicks start_time_;

  const base::TimeDelta data_timeout_ms_;

  bool is_waiting_for_manifest_;

  base::WeakPtrFactory<AddToHomescreenDataFetcher> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_ADD_TO_HOMESCREEN_DATA_FETCHER_H_
