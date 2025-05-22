// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/android/shared/media_drm_bridge.h"

#include <jni.h>

#include <cstring>
#include <vector>

#include "starboard/android/shared/drm_system.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/MediaDrmBridge_jni.h"

namespace starboard {
namespace android {
namespace shared {
namespace {
using starboard::android::shared::DrmSystem;

const char kNoUrl[] = "";

// Using all capital names to be consistent with other Android media statuses.
// They are defined in the same order as in their Java counterparts.  Their
// values should be kept in consistent with their Java counterparts defined in
// android.media.MediaDrm.KeyStatus.
const jint MEDIA_DRM_KEY_STATUS_EXPIRED = 1;
const jint MEDIA_DRM_KEY_STATUS_INTERNAL_ERROR = 4;
const jint MEDIA_DRM_KEY_STATUS_OUTPUT_NOT_ALLOWED = 2;
const jint MEDIA_DRM_KEY_STATUS_PENDING = 3;
const jint MEDIA_DRM_KEY_STATUS_USABLE = 0;

// They must have the same values as defined in MediaDrm.KeyRequest.
const jint REQUEST_TYPE_INITIAL = 0;
const jint REQUEST_TYPE_RENEWAL = 1;
const jint REQUEST_TYPE_RELEASE = 2;

SbDrmSessionRequestType SbDrmSessionRequestTypeFromMediaDrmKeyRequestType(
    jint request_type) {
  if (request_type == REQUEST_TYPE_INITIAL) {
    return kSbDrmSessionRequestTypeLicenseRequest;
  }
  if (request_type == REQUEST_TYPE_RENEWAL) {
    return kSbDrmSessionRequestTypeLicenseRenewal;
  }
  if (request_type == REQUEST_TYPE_RELEASE) {
    return kSbDrmSessionRequestTypeLicenseRelease;
  }
  SB_NOTREACHED();
  return kSbDrmSessionRequestTypeLicenseRequest;
}

}  // namespace

extern "C" SB_EXPORT_PLATFORM void JNI_MediaDrmBridge_OnMediaDrmSessionMessage(
    JNIEnv* env,
    jlong mediaDrmBridge,
    jint ticket,
    const base::android::JavaParamRef<jbyteArray>& sessionId,
    jint requestType,
    const base::android::JavaParamRef<jbyteArray>& message) {
  jbyte* session_id_elements = env->GetByteArrayElements(sessionId, NULL);
  jsize session_id_size = env->GetArrayLength(sessionId);

  jbyte* message_elements = env->GetByteArrayElements(message, NULL);
  jsize message_size = env->GetArrayLength(message);

  SB_DCHECK(session_id_elements);
  SB_DCHECK(message_elements);

  DrmSystem* drm_system = reinterpret_cast<DrmSystem*>(mediaDrmBridge);
  SB_DCHECK(drm_system);
  drm_system->CallUpdateRequestCallback(
      ticket, SbDrmSessionRequestTypeFromMediaDrmKeyRequestType(requestType),
      session_id_elements, session_id_size, message_elements, message_size,
      kNoUrl);
  env->ReleaseByteArrayElements(sessionId, session_id_elements, JNI_ABORT);
  env->ReleaseByteArrayElements(message, message_elements, JNI_ABORT);
}

extern "C" SB_EXPORT_PLATFORM void JNI_MediaDrmBridge_OnMediaDrmKeyStatusChange(
    JNIEnv* env,
    jlong mediaDrmBridge,
    const base::android::JavaParamRef<jbyteArray>& sessionId,
    const base::android::JavaParamRef<jobjectArray>& keyInformation) {
  jbyte* session_id_elements = env->GetByteArrayElements(sessionId, NULL);
  jsize session_id_size = env->GetArrayLength(sessionId);

  SB_DCHECK(session_id_elements);

  // NULL array indicates key status isn't supported (i.e. Android API < 23)
  jsize length =
      (keyInformation == NULL) ? 0 : env->GetArrayLength(keyInformation);
  std::vector<SbDrmKeyId> drm_key_ids(length);
  std::vector<SbDrmKeyStatus> drm_key_statuses(length);

  for (jsize i = 0; i < length; ++i) {
    jobject j_key_status = env->GetObjectArrayElement(keyInformation, i);
    jclass mediaDrmKeyStatusClass =
        env->FindClass("android/media/MediaDrm$KeyStatus");
    jmethodID getKeyIdMethod =
        env->GetMethodID(mediaDrmKeyStatusClass, "getKeyId", "()[B");
    jbyteArray j_key_id = static_cast<jbyteArray>(
        env->CallObjectMethod(j_key_status, getKeyIdMethod));

    jbyte* key_id_elements = env->GetByteArrayElements(j_key_id, NULL);
    jsize key_id_size = env->GetArrayLength(j_key_id);
    SB_DCHECK(key_id_elements);

    SB_DCHECK(key_id_size <= sizeof(drm_key_ids[i].identifier));
    memcpy(drm_key_ids[i].identifier, key_id_elements, key_id_size);
    env->ReleaseByteArrayElements(j_key_id, key_id_elements, JNI_ABORT);
    drm_key_ids[i].identifier_size = key_id_size;

    jmethodID getStatusCodeMethod =
        env->GetMethodID(mediaDrmKeyStatusClass, "getStatusCode", "()I");
    jint j_status_code = env->CallIntMethod(j_key_status, getStatusCodeMethod);
    if (j_status_code == MEDIA_DRM_KEY_STATUS_EXPIRED) {
      drm_key_statuses[i] = kSbDrmKeyStatusExpired;
    } else if (j_status_code == MEDIA_DRM_KEY_STATUS_INTERNAL_ERROR) {
      drm_key_statuses[i] = kSbDrmKeyStatusError;
    } else if (j_status_code == MEDIA_DRM_KEY_STATUS_OUTPUT_NOT_ALLOWED) {
      drm_key_statuses[i] = kSbDrmKeyStatusRestricted;
    } else if (j_status_code == MEDIA_DRM_KEY_STATUS_PENDING) {
      drm_key_statuses[i] = kSbDrmKeyStatusPending;
    } else if (j_status_code == MEDIA_DRM_KEY_STATUS_USABLE) {
      drm_key_statuses[i] = kSbDrmKeyStatusUsable;
    } else {
      SB_NOTREACHED();
      drm_key_statuses[i] = kSbDrmKeyStatusError;
    }
  }

  DrmSystem* drm_system = reinterpret_cast<DrmSystem*>(mediaDrmBridge);
  SB_DCHECK(drm_system);
  drm_system->CallDrmSessionKeyStatusesChangedCallback(
      session_id_elements, session_id_size, drm_key_ids, drm_key_statuses);

  env->ReleaseByteArrayElements(sessionId, session_id_elements, JNI_ABORT);
}

// static
jboolean MediaDrmBridge::IsSuccess(JNIEnv* env,
                                   const base::android::JavaRef<jobject>& obj) {
  return Java_UpdateSessionResult_isSuccess(env, obj);
}

// static
ScopedJavaLocalRef<jstring> MediaDrmBridge::GetErrorMessage(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_UpdateSessionResult_getErrorMessage(env, obj);
}

// static
ScopedJavaLocalRef<jobject> MediaDrmBridge::CreateJavaMediaDrmBridge(
    JNIEnv* env,
    const JavaRef<jstring>& keySystem,
    jlong nativeMediaDrmBridge) {
  return Java_MediaDrmBridge_create(env, keySystem, nativeMediaDrmBridge);
}

// static
bool MediaDrmBridge::IsWidevineSupported(JNIEnv* env) {
  return Java_MediaDrmBridge_isWidevineCryptoSchemeSupported(env) == JNI_TRUE;
}

// static
bool MediaDrmBridge::IsCbcsSupported(JNIEnv* env) {
  return Java_MediaDrmBridge_isCbcsSchemeSupported(env) == JNI_TRUE;
}

// static
void MediaDrmBridge::Destroy(JNIEnv* env,
                             const base::android::JavaRef<jobject>& obj) {
  Java_MediaDrmBridge_destroy(env, obj);
}

// static
void MediaDrmBridge::CreateSession(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    JniIntWrapper ticket,
    const base::android::JavaRef<jbyteArray>& initData,
    const base::android::JavaRef<jstring>& mime) {
  Java_MediaDrmBridge_createSession(env, obj, ticket, initData, mime);
}

// static
ScopedJavaLocalRef<jobject> MediaDrmBridge::UpdateSession(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    JniIntWrapper ticket,
    const JavaRef<jbyteArray>& sessionId,
    const JavaRef<jbyteArray>& response) {
  return Java_MediaDrmBridge_updateSession(env, obj, ticket, sessionId,
                                           response);
}

// static
void MediaDrmBridge::CloseSession(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jbyteArray>& sessionId) {
  Java_MediaDrmBridge_closeSession(env, obj, sessionId);
}

// static
base::android::ScopedJavaLocalRef<jbyteArray>
MediaDrmBridge::GetMetricsInBase64(JNIEnv* env,
                                   const base::android::JavaRef<jobject>& obj) {
  return Java_MediaDrmBridge_getMetricsInBase64(env, obj);
}

// static
base::android::ScopedJavaLocalRef<jobject> MediaDrmBridge::GetMediaCrypto(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_MediaDrmBridge_getMediaCrypto(env, obj);
}

// static
bool MediaDrmBridge::CreateMediaCryptoSession(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_MediaDrmBridge_createMediaCryptoSession(env, obj);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
