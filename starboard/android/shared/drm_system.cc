// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/android/shared/drm_system.h"

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"

namespace {

const char kNoUrl[] = "";

}  // namespace

extern "C" SB_EXPORT_PLATFORM void
Java_foo_cobalt_media_MediaDrmBridge_nativeOnSessionMessage(
    JNIEnv* env,
    jobject unused_this,
    jlong native_media_drm_bridge,
    jbyteArray j_session_id,
    jint request_type,
    jbyteArray j_message) {
  using starboard::android::shared::DrmSystem;

  jbyte* session_id_elements = env->GetByteArrayElements(j_session_id, NULL);
  jsize session_id_size = env->GetArrayLength(j_session_id);

  jbyte* message_elements = env->GetByteArrayElements(j_message, NULL);
  jsize message_size = env->GetArrayLength(j_message);

  SB_DCHECK(session_id_elements);
  SB_DCHECK(message_elements);

  DrmSystem* drm_system = reinterpret_cast<DrmSystem*>(native_media_drm_bridge);
  SB_DCHECK(drm_system);
  drm_system->CallUpdateRequestCallback(session_id_elements, session_id_size,
                                        message_elements, message_size, kNoUrl);
}

namespace starboard {
namespace android {
namespace shared {

namespace {

jbyteArray ByteArrayFromRaw(const void* data, int size) {
  return JniEnvExt::Get()->NewByteArrayFromRaw(static_cast<const jbyte*>(data),
                                               size);
}

}  // namespace

DrmSystem::DrmSystem(void* context,
                     SbDrmSessionUpdateRequestFunc update_request_callback,
                     SbDrmSessionUpdatedFunc session_updated_callback)
    : context_(context),
      update_request_callback_(update_request_callback),
      session_updated_callback_(session_updated_callback),
      j_media_drm_bridge_(NULL),
      j_media_crypto_(NULL) {
  JniEnvExt* env = JniEnvExt::Get();
  j_media_drm_bridge_ = env->CallStaticObjectMethod(
      "foo/cobalt/media/MediaDrmBridge", "create",
      "(J)Lfoo/cobalt/media/MediaDrmBridge;", reinterpret_cast<jlong>(this));
  if (!j_media_drm_bridge_) {
    SB_LOG(ERROR) << "Failed to create MediaDrmBridge.";
    return;
  }
  j_media_drm_bridge_ = env->ConvertLocalRefToGlobalRef(j_media_drm_bridge_);
  j_media_crypto_ = env->CallObjectMethod(j_media_drm_bridge_, "getMediaCrypto",
                                          "()Landroid/media/MediaCrypto;");
  if (!j_media_crypto_) {
    SB_LOG(ERROR) << "Failed to create MediaCrypto.";
    return;
  }
  j_media_crypto_ = env->ConvertLocalRefToGlobalRef(j_media_crypto_);
}

DrmSystem::~DrmSystem() {
  JniEnvExt* env = JniEnvExt::Get();
  if (j_media_crypto_) {
    env->DeleteGlobalRef(j_media_crypto_);
    j_media_crypto_ = NULL;
  }
  if (j_media_drm_bridge_) {
    env->CallVoidMethod(j_media_drm_bridge_, "destroy", "()V");
    env->DeleteGlobalRef(j_media_drm_bridge_);
    j_media_drm_bridge_ = NULL;
  }
}

void DrmSystem::GenerateSessionUpdateRequest(
    int /*ticket*/,  // TODO: Implement ticket passing.
    const char* type,
    const void* initialization_data,
    int initialization_data_size) {
  ScopedLocalJavaRef j_init_data(
      ByteArrayFromRaw(initialization_data, initialization_data_size));
  JniEnvExt* env = JniEnvExt::Get();
  jstring j_mime = env->NewStringUTF(type);
  env->CallVoidMethod(j_media_drm_bridge_, "createSession",
                      "([BLjava/lang/String;)V", j_init_data.Get(), j_mime);
  // |update_request_callback_| will be called by Java calling into
  // |onSessionMessage|.
}

void DrmSystem::UpdateSession(const void* key,
                              int key_size,
                              const void* session_id,
                              int session_id_size) {
  ScopedLocalJavaRef j_session_id(
      ByteArrayFromRaw(session_id, session_id_size));
  ScopedLocalJavaRef j_response(ByteArrayFromRaw(key, key_size));

  jboolean status = JniEnvExt::Get()->CallBooleanMethod(
      j_media_drm_bridge_, "updateSession", "([B[B)Z", j_session_id.Get(),
      j_response.Get());
  session_updated_callback_(this, context_, session_id, session_id_size,
                            status == JNI_TRUE);
}

void DrmSystem::CloseSession(const void* session_id, int session_id_size) {
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef j_session_id(
      ByteArrayFromRaw(session_id, session_id_size));
  env->CallVoidMethod(j_media_drm_bridge_, "closeSession", "([B)V",
                      j_session_id.Get());
}

DrmSystem::DecryptStatus DrmSystem::Decrypt(InputBuffer* buffer) {
  SB_DCHECK(buffer);
  SB_DCHECK(buffer->drm_info());
  SB_DCHECK(j_media_crypto_);
  // The actual decryption will take place by calling |queueSecureInputBuffer|
  // in the decoders.  Our existence implies that there is enough information
  // to perform the decryption.
  return kSuccess;
}

void DrmSystem::CallUpdateRequestCallback(const void* session_id,
                                          int session_id_size,
                                          const void* content,
                                          int content_size,
                                          const char* url) {
  update_request_callback_(this, context_,
                           0,  // TODO: Implement ticket passing.
                           session_id, session_id_size, content, content_size,
                           url);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback) {
  using starboard::android::shared::DrmSystem;
  using starboard::android::shared::IsWidevine;

  if (!IsWidevine(key_system)) {
    return kSbDrmSystemInvalid;
  }

  DrmSystem* drm_system =
      new DrmSystem(context, update_request_callback, session_updated_callback);
  if (!drm_system->is_valid()) {
    delete drm_system;
    return kSbDrmSystemInvalid;
  }
  return drm_system;
}
