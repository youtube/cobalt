// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TRUSTED_CDN_H_
#define CHROME_BROWSER_ANDROID_TRUSTED_CDN_H_

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class Page;
}

// Native part of Trusted CDN publisher URL provider. Managed by Java layer.
class TrustedCdn : public content::WebContentsObserver {
 public:
  TrustedCdn(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  ~TrustedCdn() override;

  void SetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents);
  void ResetWebContents(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void OnDestroyed(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobj_;
};

#endif  // CHROME_BROWSER_ANDROID_TRUSTED_CDN_H_
