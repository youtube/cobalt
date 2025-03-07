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

#include "starboard/android/shared/exoplayer/exoplayer_bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "starboard/common/log.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

ExoPlayerBridge::ExoPlayerBridge() {
  non_delayed_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  SB_CHECK(non_delayed_fd_ != -1);
  looper_ = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
  SB_DCHECK(looper_);
  ALooper_acquire(looper_);
  ALooper_addFd(looper_, non_delayed_fd_, 0, ALOOPER_EVENT_INPUT, NULL, this);
  JniEnvExt* env = JniEnvExt::Get();
  SB_LOG(INFO) << "About to create ExoPlayer";
  ScopedLocalJavaRef<jobject> j_exoplayer_bridge(
      env->CallStarboardObjectMethodOrAbort(
          "getExoPlayerBridge", "()Ldev/cobalt/media/ExoPlayerBridge;"));
  env->CallVoidMethodOrAbort(j_exoplayer_bridge.Get(), "createExoPlayer",
                             "(Landroid/os/Looper;)V", looper_);
}

ExoPlayerBridge::~ExoPlayerBridge() {
  SB_DCHECK(ALooper_forThread() == looper_);
  JniEnvExt* env = JniEnvExt::Get();
  SB_LOG(INFO) << "About to destroy ExoPlayer";
  ScopedLocalJavaRef<jobject> j_exoplayer_bridge(
      env->CallStarboardObjectMethodOrAbort(
          "getExoPlayerBridge", "()Ldev/cobalt/media/ExoPlayerBridge;"));
  env->CallVoidMethodOrAbort(j_exoplayer_bridge.Get(), "destroyExoPlayer",
                             "()V");
  ALooper_removeFd(looper_, non_delayed_fd_);
  ALooper_release(looper_);
  looper_ = nullptr;

  close(non_delayed_fd_);
}
