// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/chrome_jni_headers/PageInfoAboutThisSiteController_jni.h"

#include <jni.h>
#include "base/android/jni_array.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_info/about_this_site_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"

static jboolean JNI_PageInfoAboutThisSiteController_IsFeatureEnabled(
    JNIEnv* env) {
  return page_info::IsAboutThisSiteFeatureEnabled(
      g_browser_process->GetApplicationLocale());
}

static jint JNI_PageInfoAboutThisSiteController_GetJavaDrawableIconId(
    JNIEnv* env) {
  return ResourceMapper::MapToJavaDrawableId(
      IDR_ANDROID_ABOUT_THIS_SITE_LOGO_24DP);
}

static base::android::ScopedJavaLocalRef<jbyteArray>
JNI_PageInfoAboutThisSiteController_GetSiteInfo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_browserContext,
    const base::android::JavaParamRef<jobject>& j_url,
    const base::android::JavaParamRef<jobject>& j_webContents) {
  Profile* profile = Profile::FromBrowserContext(
      content::BrowserContextFromJavaHandle(j_browserContext));
  auto* service = AboutThisSiteServiceFactory::GetForProfile(profile);
  if (!service)
    return nullptr;
  auto url = url::GURLAndroid::ToNativeGURL(env, j_url);
  auto source_id = content::WebContents::FromJavaWebContents(j_webContents)
                       ->GetPrimaryMainFrame()
                       ->GetPageUkmSourceId();
  auto info = service->GetAboutThisSiteInfo(*url, source_id);
  if (!info)
    return nullptr;

  // Serialize the proto to pass it to Java. This will copy the whole object
  // but it only contains a few strings and ints and this method is called only
  // when PageInfo is opened.
  int size = info->ByteSize();
  std::vector<uint8_t> data(size);
  info->SerializeToArray(data.data(), size);
  return base::android::ToJavaByteArray(env, data.data(), size);
}

static void JNI_PageInfoAboutThisSiteController_OnAboutThisSiteRowClicked(
    JNIEnv* env,
    jboolean j_withDescription) {
  page_info::AboutThisSiteService::OnAboutThisSiteRowClicked(j_withDescription);
}
