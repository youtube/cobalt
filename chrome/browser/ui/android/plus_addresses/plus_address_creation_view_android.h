// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_ANDROID_H_

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "content/public/browser/web_contents.h"

#include <jni.h>

namespace plus_addresses {

// A class intended as a thin wrapper around a Java object, which calls out to
// the `PlusAddressCreationController`. This shields the controller from JNI
// complications, allowing a consistent interface for clients (e.g., autofill).
// Note that it is likely that either the controller will morph to do what this
// class does now, or a similar wrapper will be created for desktop, with a
// single controller implementation.
class PlusAddressCreationViewAndroid {
 public:
  PlusAddressCreationViewAndroid(
      base::WeakPtr<PlusAddressCreationController> controller,
      content::WebContents* web_contents);
  ~PlusAddressCreationViewAndroid();

  void Show(const std::string& primary_email_address,
            const std::string& plus_address);
  void OnConfirmed(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);
  void OnCanceled(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void PromptDismissed(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::WeakPtr<PlusAddressCreationController> controller_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_ANDROID_H_
