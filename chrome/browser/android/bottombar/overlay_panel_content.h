// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOTTOMBAR_OVERLAY_PANEL_CONTENT_H_
#define CHROME_BROWSER_ANDROID_BOTTOMBAR_OVERLAY_PANEL_CONTENT_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/task/cancelable_task_tracker.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_contents_delegate_android {
class WebContentsDelegateAndroid;
}  // namespace web_contents_delegate_android

// Manages the native extraction and request logic for Contextual Search,
// and interacts with the Java OverlayPanelContent for UX.
// Most of the work is done by the associated ContextualSearchDelegate.
class OverlayPanelContent {
 public:
  // Constructs a native manager associated with the Java manager.
  OverlayPanelContent(JNIEnv* env, jobject obj);

  OverlayPanelContent(const OverlayPanelContent&) = delete;
  OverlayPanelContent& operator=(const OverlayPanelContent&) = delete;

  virtual ~OverlayPanelContent();

  // Called by the Java OverlayPanelContent when it is being destroyed.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height);

  // Removes a search URL from history. |search_start_time_ms| represents the
  // time at which |search_url| was committed.
  void RemoveLastHistoryEntry(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& search_url,
      jlong search_start_time_ms);

  // Takes ownership of the WebContents which holds the panel content.
  void SetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      const base::android::JavaParamRef<jobject>& jweb_contents_delegate);

  // Destroys the WebContents.
  void DestroyWebContents(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj);

  // Sets the delegate used to convert navigations to intents.
  void SetInterceptNavigationDelegate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& delegate,
      const base::android::JavaParamRef<jobject>& jweb_contents);

  // Update the browser controls for the held web contents.
  void UpdateBrowserControlsState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean are_controls_hidden);

 private:
  // Our global reference to the Java OverlayPanelContent.
  base::android::ScopedJavaGlobalRef<jobject> java_manager_;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  // The WebContents that holds the panel content.
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<web_contents_delegate_android::WebContentsDelegateAndroid>
      web_contents_delegate_;
};

#endif  // CHROME_BROWSER_ANDROID_BOTTOMBAR_OVERLAY_PANEL_CONTENT_H_
