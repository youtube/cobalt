// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PASSWORDS_MANUAL_FILLING_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PASSWORDS_MANUAL_FILLING_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"

namespace gfx {
class Image;
}

namespace content {
class WebContents;
}  // namespace content

class ManualFillingController;

// This Android-specific implementation of the |ManualFillingViewInterface|
// is the native counterpart of the |PasswordAccessoryViewBridge| java class.
// It's owned by a ManualFillingController which is bound to an activity.
class ManualFillingViewAndroid : public ManualFillingViewInterface {
 public:
  // Builds the UI for the |controller|.
  ManualFillingViewAndroid(ManualFillingController* controller,
                           content::WebContents* web_contents);

  ManualFillingViewAndroid(const ManualFillingViewAndroid&) = delete;
  ManualFillingViewAndroid& operator=(const ManualFillingViewAndroid&) = delete;

  ~ManualFillingViewAndroid() override;

  // ManualFillingViewInterface:
  void OnItemsAvailable(autofill::AccessorySheetData data) override;
  void OnAutomaticGenerationStatusChanged(bool available) override;
  void CloseAccessorySheet() override;
  void SwapSheetWithKeyboard() override;
  void Show(WaitForKeyboard wait_for_keyboard) override;
  void Hide() override;
  void ShowAccessorySheetTab(
      const autofill::AccessoryTabType& tab_type) override;

  // Called from Java via JNI:
  void OnFaviconRequested(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_origin,
      jint desired_size_in_px,
      const base::android::JavaParamRef<jobject>& j_callback);
  void OnFillingTriggered(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint tab_type,
      const base::android::JavaParamRef<jobject>& j_user_info_field);
  void OnOptionSelected(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint selected_action);
  void OnToggleChanged(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint selected_action,
                       jboolean enabled);
  void RequestAccessorySheet(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jint tab_type);
  void OnViewDestroyed(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

 private:
  void OnImageFetched(base::android::ScopedJavaGlobalRef<jstring> j_origin,
                      base::android::ScopedJavaGlobalRef<jobject> j_callback,
                      const gfx::Image& image);

  base::android::ScopedJavaGlobalRef<jobject> GetOrCreateJavaObject();

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT});

  // The controller provides data for this view and owns it.
  raw_ptr<ManualFillingController> controller_;

  // WebContents object that the controller and the bridge correspond to.
  raw_ptr<content::WebContents> web_contents_;

  // The corresponding java object. Use `GetOrCreateJavaObject()` to access.
  base::android::ScopedJavaGlobalRef<jobject> java_object_internal_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_PASSWORDS_MANUAL_FILLING_VIEW_ANDROID_H_
