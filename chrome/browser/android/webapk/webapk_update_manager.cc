// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/android/chrome_jni_headers/WebApkUpdateManager_jni.h"
#include "chrome/browser/android/webapk/webapk_features.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/android/color_utils_android.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

namespace {

// Called after the update either succeeds or fails.
void OnUpdated(const JavaRef<jobject>& java_callback,
               webapps::WebApkInstallResult result,
               std::unique_ptr<std::string> serialized_proto,
               bool relax_updates,
               const std::string& webapk_package) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkUpdateCallback_onResultFromNative(
      env, java_callback, static_cast<int>(result), relax_updates);
}

}  // anonymous namespace

// static JNI method.
static jint JNI_WebApkUpdateManager_GetWebApkTargetShellVersion(JNIEnv* env) {
  return base::GetFieldTrialParamByFeatureAsInt(
      kWebApkShellUpdate, kWebApkTargetShellVersion.name,
      kWebApkTargetShellVersion.default_value);
}

// static JNI method.
static void JNI_WebApkUpdateManager_StoreWebApkUpdateRequestToFile(
    JNIEnv* env,
    const JavaParamRef<jstring>& java_update_request_path,
    const JavaParamRef<jstring>& java_start_url,
    const JavaParamRef<jstring>& java_scope,
    const JavaParamRef<jstring>& java_name,
    const JavaParamRef<jstring>& java_short_name,
    const JavaParamRef<jstring>& java_manifest_id,
    const JavaParamRef<jstring>& java_app_key,
    const JavaParamRef<jstring>& java_primary_icon_url,
    const JavaParamRef<jstring>& java_primary_icon_data,
    jboolean java_is_primary_icon_maskable,
    const JavaParamRef<jstring>& java_splash_icon_url,
    const JavaParamRef<jstring>& java_splash_icon_data,
    jboolean java_is_splash_icon_maskable,
    const JavaParamRef<jobjectArray>& java_icon_urls,
    const JavaParamRef<jobjectArray>& java_icon_hashes,
    jint java_display_mode,
    jint java_orientation,
    jlong java_theme_color,
    jlong java_background_color,
    const JavaParamRef<jstring>& java_share_target_action,
    const JavaParamRef<jstring>& java_share_target_param_title,
    const JavaParamRef<jstring>& java_share_target_param_text,
    const jboolean java_share_target_param_is_method_post,
    const jboolean java_share_target_param_is_enctype_multipart,
    const JavaParamRef<jobjectArray>& java_share_target_param_file_names,
    const JavaParamRef<jobjectArray>& java_share_target_param_accepts,
    const JavaParamRef<jobjectArray>& java_shortcuts,
    const JavaParamRef<jobjectArray>& java_shortcut_icon_data,
    const JavaParamRef<jstring>& java_web_manifest_url,
    const JavaParamRef<jstring>& java_webapk_package,
    jint java_webapk_version,
    jboolean java_is_manifest_stale,
    jboolean java_is_app_identity_update_supported,
    const JavaParamRef<jintArray>& java_update_reasons,
    const JavaParamRef<jobject>& java_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string update_request_path =
      ConvertJavaStringToUTF8(env, java_update_request_path);

  webapps::ShortcutInfo info(
      GURL(ConvertJavaStringToUTF8(env, java_start_url)));
  info.scope = GURL(ConvertJavaStringToUTF8(env, java_scope));
  info.name = ConvertJavaStringToUTF16(env, java_name);
  info.short_name = ConvertJavaStringToUTF16(env, java_short_name);
  info.user_title = info.short_name;
  info.display = static_cast<blink::mojom::DisplayMode>(java_display_mode);
  info.orientation =
      static_cast<device::mojom::ScreenOrientationLockType>(java_orientation);
  info.theme_color = ui::JavaColorToOptionalSkColor(java_theme_color);
  info.background_color = ui::JavaColorToOptionalSkColor(java_background_color);
  info.best_primary_icon_url =
      GURL(ConvertJavaStringToUTF8(env, java_primary_icon_url));
  info.splash_image_url =
      GURL(ConvertJavaStringToUTF8(env, java_splash_icon_url));
  info.is_splash_image_maskable = java_is_splash_icon_maskable;
  info.manifest_url = GURL(ConvertJavaStringToUTF8(env, java_web_manifest_url));
  info.manifest_id = GURL(ConvertJavaStringToUTF8(env, java_manifest_id));
  GURL app_key(ConvertJavaStringToUTF8(env, java_app_key));

  GURL share_target_action =
      GURL(ConvertJavaStringToUTF8(env, java_share_target_action));
  if (!share_target_action.is_empty()) {
    info.share_target = webapps::ShareTarget();
    info.share_target->action = share_target_action;
    info.share_target->params.title =
        ConvertJavaStringToUTF16(java_share_target_param_title);
    info.share_target->params.text =
        ConvertJavaStringToUTF16(java_share_target_param_text);
    info.share_target->method =
        java_share_target_param_is_method_post == JNI_TRUE
            ? blink::mojom::ManifestShareTarget_Method::kPost
            : blink::mojom::ManifestShareTarget_Method::kGet;

    info.share_target->enctype =
        java_share_target_param_is_enctype_multipart == JNI_TRUE
            ? blink::mojom::ManifestShareTarget_Enctype::kMultipartFormData
            : blink::mojom::ManifestShareTarget_Enctype::kFormUrlEncoded;

    std::vector<std::u16string> fileNames;
    base::android::AppendJavaStringArrayToStringVector(
        env, java_share_target_param_file_names, &fileNames);

    std::vector<std::vector<std::u16string>> accepts;
    base::android::Java2dStringArrayTo2dStringVector(
        env, java_share_target_param_accepts, &accepts);

    // The length of fileNames and accepts should always be the same, but here
    // we just want to be safe.
    for (size_t i = 0; i < std::min(fileNames.size(), accepts.size()); ++i) {
      webapps::ShareTargetParamsFile file;
      file.name = fileNames[i];
      file.accept.swap(accepts[i]);
      info.share_target->params.files.push_back(file);
    }
  }

  base::android::AppendJavaStringArrayToStringVector(env, java_icon_urls,
                                                     &info.icon_urls);

  std::vector<std::string> icon_hashes;
  base::android::AppendJavaStringArrayToStringVector(env, java_icon_hashes,
                                                     &icon_hashes);

  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  for (size_t i = 0; i < info.icon_urls.size(); ++i) {
    icon_url_to_murmur2_hash[info.icon_urls[i]] =
        webapps::WebApkIconHasher::Icon{/* data= */ "", icon_hashes[i]};
  }

  std::string primary_icon_data =
      ConvertJavaStringToUTF8(env, java_primary_icon_data);
  std::string splash_icon_data =
      ConvertJavaStringToUTF8(env, java_splash_icon_data);

  std::string webapk_package;
  ConvertJavaStringToUTF8(env, java_webapk_package, &webapk_package);

  std::vector<std::vector<std::u16string>> shortcuts;
  std::vector<std::string> shortcut_icon_data;
  base::android::Java2dStringArrayTo2dStringVector(env, java_shortcuts,
                                                   &shortcuts);
  base::android::JavaArrayOfByteArrayToStringVector(
      env, java_shortcut_icon_data, &shortcut_icon_data);

  DCHECK_EQ(shortcuts.size(), shortcut_icon_data.size());

  for (size_t i = 0; i < shortcuts.size(); i++) {
    const auto& shortcut_data = shortcuts[i];
    DCHECK_EQ(shortcut_data.size(), 5u);

    blink::Manifest::ShortcutItem shortcut_item;
    shortcut_item.name = shortcut_data[0];
    shortcut_item.short_name = shortcut_data[1];
    shortcut_item.url = GURL(base::UTF16ToUTF8(shortcut_data[2]));

    blink::Manifest::ImageResource icon;
    GURL icon_src(base::UTF16ToUTF8(shortcut_data[3]));
    icon.src = icon_src;
    icon.purpose.push_back(blink::mojom::ManifestImageResource_Purpose::ANY);
    shortcut_item.icons.push_back(std::move(icon));

    if (icon_src.is_valid()) {
      icon_url_to_murmur2_hash[icon_src.spec()] =
          webapps::WebApkIconHasher::Icon{
              /* data= */ std::move(shortcut_icon_data[i]),
              /* hash= */ base::UTF16ToUTF8(shortcut_data[4])};
    }
    info.best_shortcut_icon_urls.push_back(std::move(icon_src));
    info.shortcut_items.push_back(std::move(shortcut_item));
  }

  std::vector<int> int_update_reasons;
  base::android::JavaIntArrayToIntVector(env, java_update_reasons,
                                         &int_update_reasons);
  std::vector<webapps::WebApkUpdateReason> update_reasons;
  for (int update_reason : int_update_reasons)
    update_reasons.push_back(
        static_cast<webapps::WebApkUpdateReason>(update_reason));

  WebApkInstaller::StoreUpdateRequestToFile(
      base::FilePath(update_request_path), info, app_key, primary_icon_data,
      java_is_primary_icon_maskable, splash_icon_data, webapk_package,
      base::NumberToString(java_webapk_version),
      std::move(icon_url_to_murmur2_hash), java_is_manifest_stale,
      java_is_app_identity_update_supported, std::move(update_reasons),
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(java_callback)));
}

// static JNI method.
static void JNI_WebApkUpdateManager_UpdateWebApkFromFile(
    JNIEnv* env,
    const JavaParamRef<jstring>& java_update_request_path,
    const JavaParamRef<jobject>& java_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScopedJavaGlobalRef<jobject> callback_ref(java_callback);

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&OnUpdated, callback_ref,
                       webapps::WebApkInstallResult::FAILURE,
                       nullptr /* serialized_proto */,
                       false /* relax_updates */, "" /* webapk_package */));
    return;
  }

  std::string update_request_path =
      ConvertJavaStringToUTF8(env, java_update_request_path);
  WebApkInstallService::Get(profile)->UpdateAsync(
      base::FilePath(update_request_path),
      base::BindOnce(&OnUpdated, callback_ref));
}
