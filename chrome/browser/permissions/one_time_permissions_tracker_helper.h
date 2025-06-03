// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_HELPER_H_
#define CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_HELPER_H_

#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// This class informs OneTimePermissionsTracker of pages being loaded, navigated
// or destroyed in each tab. This information is then used by the
// OneTimePermissionProvider to revoke permissions.
class OneTimePermissionsTrackerHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OneTimePermissionsTrackerHelper>,
      public resource_coordinator::TabLifecycleObserver,
      public MediaStreamCaptureIndicator::Observer {
 public:
  ~OneTimePermissionsTrackerHelper() override;

  OneTimePermissionsTrackerHelper(const OneTimePermissionsTrackerHelper&) =
      delete;
  OneTimePermissionsTrackerHelper& operator=(
      const OneTimePermissionsTrackerHelper&) = delete;

  // content::WebContentObserver
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // resource_coordinator::TabLifecycleObserver
  void OnDiscardedStateChange(content::WebContents* contents,
                              LifecycleUnitDiscardReason reason,
                              bool is_discarded) override;

  // MediaStreamCaptureIndicator::Observer
  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                 bool is_capturing_audio) override;

 private:
  explicit OneTimePermissionsTrackerHelper(content::WebContents* webContents);
  friend class content::WebContentsUserData<OneTimePermissionsTrackerHelper>;
  absl::optional<url::Origin> last_committed_origin_;
  absl::optional<content::Visibility> last_visibility_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PERMISSIONS_ONE_TIME_PERMISSIONS_TRACKER_HELPER_H_
