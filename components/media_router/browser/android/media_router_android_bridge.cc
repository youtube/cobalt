// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/android/media_router_android_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/media_router/browser/android/flinging_controller_bridge.h"
#include "components/media_router/browser/android/jni_headers/BrowserMediaRouter_jni.h"
#include "components/media_router/browser/android/media_router_android.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_controller.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace media_router {

MediaRouterAndroidBridge::MediaRouterAndroidBridge(MediaRouterAndroid* router)
    : native_media_router_(router) {
  JNIEnv* env = AttachCurrentThread();
  java_media_router_.Reset(
      Java_BrowserMediaRouter_create(env, reinterpret_cast<jlong>(this)));
}

MediaRouterAndroidBridge::~MediaRouterAndroidBridge() {
  JNIEnv* env = AttachCurrentThread();
  // When |this| is destroyed, there might still pending runnables on the Java
  // side, that are keeping the Java object alive. These runnables might try to
  // call back to the native side when executed. We need to signal to the Java
  // counterpart that it can't call back into native anymore.
  Java_BrowserMediaRouter_teardown(env, java_media_router_);
}

void MediaRouterAndroidBridge::CreateRoute(const MediaSource::Id& source_id,
                                           const MediaSink::Id& sink_id,
                                           const std::string& presentation_id,
                                           const url::Origin& origin,
                                           content::WebContents* web_contents,
                                           int route_request_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource_id =
      ConvertUTF8ToJavaString(env, source_id);
  ScopedJavaLocalRef<jstring> jsink_id = ConvertUTF8ToJavaString(env, sink_id);
  ScopedJavaLocalRef<jstring> jpresentation_id =
      ConvertUTF8ToJavaString(env, presentation_id);
  ScopedJavaLocalRef<jstring> jorigin =
      ConvertUTF8ToJavaString(env, origin.GetURL().spec());
  base::android::ScopedJavaLocalRef<jobject> java_web_contents;
  if (web_contents)
    java_web_contents = web_contents->GetJavaWebContents();

  Java_BrowserMediaRouter_createRoute(env, java_media_router_, jsource_id,
                                      jsink_id, jpresentation_id, jorigin,
                                      java_web_contents, route_request_id);
}

void MediaRouterAndroidBridge::JoinRoute(const MediaSource::Id& source_id,
                                         const std::string& presentation_id,
                                         const url::Origin& origin,
                                         content::WebContents* web_contents,
                                         int route_request_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource_id =
      ConvertUTF8ToJavaString(env, source_id);
  ScopedJavaLocalRef<jstring> jpresentation_id =
      ConvertUTF8ToJavaString(env, presentation_id);
  ScopedJavaLocalRef<jstring> jorigin =
      ConvertUTF8ToJavaString(env, origin.GetURL().spec());
  base::android::ScopedJavaLocalRef<jobject> java_web_contents;
  if (web_contents)
    java_web_contents = web_contents->GetJavaWebContents();

  Java_BrowserMediaRouter_joinRoute(env, java_media_router_, jsource_id,
                                    jpresentation_id, jorigin,
                                    java_web_contents, route_request_id);
}

void MediaRouterAndroidBridge::TerminateRoute(const MediaRoute::Id& route_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jroute_id =
      ConvertUTF8ToJavaString(env, route_id);
  Java_BrowserMediaRouter_closeRoute(env, java_media_router_, jroute_id);
}

void MediaRouterAndroidBridge::SendRouteMessage(const MediaRoute::Id& route_id,
                                                const std::string& message) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jroute_id =
      ConvertUTF8ToJavaString(env, route_id);
  ScopedJavaLocalRef<jstring> jmessage = ConvertUTF8ToJavaString(env, message);
  Java_BrowserMediaRouter_sendStringMessage(env, java_media_router_, jroute_id,
                                            jmessage);
}

void MediaRouterAndroidBridge::DetachRoute(const MediaRoute::Id& route_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jroute_id =
      ConvertUTF8ToJavaString(env, route_id);
  Java_BrowserMediaRouter_detachRoute(env, java_media_router_, jroute_id);
}

bool MediaRouterAndroidBridge::StartObservingMediaSinks(
    const MediaSource::Id& source_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource_id =
      ConvertUTF8ToJavaString(env, source_id);
  return Java_BrowserMediaRouter_startObservingMediaSinks(
      env, java_media_router_, jsource_id);
}

void MediaRouterAndroidBridge::StopObservingMediaSinks(
    const MediaSource::Id& source_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jsource_id =
      ConvertUTF8ToJavaString(env, source_id);
  Java_BrowserMediaRouter_stopObservingMediaSinks(env, java_media_router_,
                                                  jsource_id);
}

std::unique_ptr<media::FlingingController>
MediaRouterAndroidBridge::GetFlingingController(
    const MediaRoute::Id& route_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jroute_id =
      ConvertUTF8ToJavaString(env, route_id);

  ScopedJavaGlobalRef<jobject> flinging_controller;

  flinging_controller.Reset(Java_BrowserMediaRouter_getFlingingControllerBridge(
      env, java_media_router_, jroute_id));

  if (flinging_controller.is_null())
    return nullptr;

  return std::make_unique<FlingingControllerBridge>(flinging_controller);
}

void MediaRouterAndroidBridge::OnSinksReceived(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& jsource_urn,
    jint jcount) {
  std::vector<MediaSink> sinks_converted;
  sinks_converted.reserve(jcount);
  for (int i = 0; i < jcount; ++i) {
    ScopedJavaLocalRef<jstring> jsink_urn = Java_BrowserMediaRouter_getSinkUrn(
        env, java_media_router_, jsource_urn, i);
    ScopedJavaLocalRef<jstring> jsink_name =
        Java_BrowserMediaRouter_getSinkName(env, java_media_router_,
                                            jsource_urn, i);
    sinks_converted.push_back(MediaSink(
        ConvertJavaStringToUTF8(env, jsink_urn.obj()),
        ConvertJavaStringToUTF8(env, jsink_name.obj()), SinkIconType::GENERIC,
        mojom::MediaRouteProviderId::ANDROID_CAF));
  }
  native_media_router_->OnSinksReceived(
      ConvertJavaStringToUTF8(env, jsource_urn), sinks_converted);
}

void MediaRouterAndroidBridge::OnRouteCreated(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& jmedia_route_id,
    const JavaRef<jstring>& jsink_id,
    jint jroute_request_id,
    jboolean jis_local) {
  native_media_router_->OnRouteCreated(
      ConvertJavaStringToUTF8(env, jmedia_route_id),
      ConvertJavaStringToUTF8(env, jsink_id), jroute_request_id, jis_local);
}

void MediaRouterAndroidBridge::OnRouteMediaSourceUpdated(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jstring>& jmedia_route_id,
    const base::android::JavaRef<jstring>& jmedia_source_id) {
  native_media_router_->OnRouteMediaSourceUpdated(
      ConvertJavaStringToUTF8(env, jmedia_route_id),
      ConvertJavaStringToUTF8(env, jmedia_source_id));
}

void MediaRouterAndroidBridge::OnCreateRouteRequestError(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& jerror_text,
    jint jroute_request_id) {
  native_media_router_->OnCreateRouteRequestError(
      ConvertJavaStringToUTF8(jerror_text), jroute_request_id);
}

void MediaRouterAndroidBridge::OnJoinRouteRequestError(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& jerror_text,
    jint jroute_request_id) {
  native_media_router_->OnJoinRouteRequestError(
      ConvertJavaStringToUTF8(jerror_text), jroute_request_id);
}

void MediaRouterAndroidBridge::OnRouteTerminated(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& jmedia_route_id) {
  native_media_router_->OnRouteTerminated(
      ConvertJavaStringToUTF8(env, jmedia_route_id));
}

void MediaRouterAndroidBridge::OnRouteClosed(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& jmedia_route_id,
    const JavaRef<jstring>& jerror) {
  native_media_router_->OnRouteClosed(
      ConvertJavaStringToUTF8(env, jmedia_route_id),
      jerror.is_null()
          ? absl::nullopt
          : absl::make_optional(ConvertJavaStringToUTF8(env, jerror)));
}

void MediaRouterAndroidBridge::OnMessage(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& jmedia_route_id,
    const JavaRef<jstring>& jmessage) {
  native_media_router_->OnMessage(ConvertJavaStringToUTF8(env, jmedia_route_id),
                                  ConvertJavaStringToUTF8(env, jmessage));
}

}  // namespace media_router
