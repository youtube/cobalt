// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_weak_ref.h"
#include "base/no_destructor.h"
#include "chrome/android/chrome_jni_headers/DocumentPictureInPictureActivity_jni.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

using content::WebContents;

namespace {

// A helper class that observes the `PictureInPictureWindowManager` and forces
// the Android Activity to close via JNI if the native Picture-in-Picture
// session exits (e.g. if the opener tab is closed or navigated). This resolves
// the asynchronous race condition by ensuring C++ does not orphan the Java
// Activity if teardown occurs during the Intent startup phase.
class DocumentPictureInPictureActivityObserver
    : public PictureInPictureWindowManager::Observer {
 public:
  static DocumentPictureInPictureActivityObserver* GetInstance() {
    static base::NoDestructor<DocumentPictureInPictureActivityObserver>
        instance;
    return instance.get();
  }

  DocumentPictureInPictureActivityObserver() {
    PictureInPictureWindowManager::GetInstance()->AddObserver(this);
  }

  void SetActivity(const jni_zero::JavaRef<jobject>& java_activity) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    java_activity_ = JavaObjectWeakGlobalRef(env, java_activity);
  }

  // PictureInPictureWindowManager::Observer:
  void OnExitPictureInPicture() override {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    jni_zero::ScopedJavaLocalRef<jobject> local_activity =
        java_activity_.get(env);
    if (!local_activity.is_null()) {
      Java_DocumentPictureInPictureActivity_closeActivity(env, local_activity);
    }
    // Clear the weak ref so we don't hold onto it across sessions.
    java_activity_.reset();
  }

 private:
  JavaObjectWeakGlobalRef java_activity_;
};

}  // namespace

static bool JNI_DocumentPictureInPictureActivity_RegisterJavaActivity(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& java_activity,
    const jni_zero::JavaRef<jobject>& j_web_contents) {
  CHECK(!java_activity.is_null());
  auto* window_manager = PictureInPictureWindowManager::GetInstance();
  content::WebContents* active_web_contents =
      window_manager->GetChildWebContents();
  content::WebContents* activity_web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  // Reject if the controller was closed OR if the Activity belongs to an older
  // session.
  if (!active_web_contents || active_web_contents != activity_web_contents) {
    return false;
  }

  DocumentPictureInPictureActivityObserver::GetInstance()->SetActivity(
      java_activity);
  return true;
}

// This JNI call serves as a fallback for test environments that launch the
// Activity directly, bypassing the standard AddNewContents pipeline.
static void
JNI_DocumentPictureInPictureActivity_OnActivityStartForTesting(  // IN-TEST
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& parentWebContent,
    const jni_zero::JavaRef<jobject>& webContent) {
  WebContents* parent_web_contents =
      WebContents::FromJavaWebContents(parentWebContent);
  WebContents* web_content = WebContents::FromJavaWebContents(webContent);
  PictureInPictureWindowManager::GetInstance()->EnterDocumentPictureInPicture(
      parent_web_contents, web_content);
}

static void JNI_DocumentPictureInPictureActivity_OnBackToTab(JNIEnv* env) {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPictureViaWindowUi(
      PictureInPictureWindowManager::UiBehavior::kCloseWindowAndFocusOpener);
}

DEFINE_JNI(DocumentPictureInPictureActivity)
