/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_BASE_SHELL_MEDIA_PLATFORM_H_
#define MEDIA_BASE_SHELL_MEDIA_PLATFORM_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/base/limits.h"
#include "media/base/media_export.h"

namespace media {

// This class is meant to be the single point to attach platform specific media
// classes and settings. Each platform should implement its own implementation
// and provide it through ShellMediaPlatform::Instance.
class MEDIA_EXPORT ShellMediaPlatform {
 public:
  ShellMediaPlatform() {}
  virtual ~ShellMediaPlatform() {}

  // Individual platforms should implement the following two functions to
  // ensure that a valid ShellMediaPlatform instance is available during the
  // life time of media stack
  static void Initialize();
  static void Terminate();

  static ShellMediaPlatform* Instance();

  // Media stack buffer allocate/free functions currently only used by
  // ShellBufferFactory.
  virtual void* AllocateBuffer(size_t size) = 0;
  virtual void FreeBuffer(void* ptr) = 0;

  // The maximum audio and video buffer size used by SourceBufferStream.
  // See implementation of SourceBufferStream for more details.
  virtual size_t GetSourceBufferStreamAudioMemoryLimit() const = 0;
  virtual size_t GetSourceBufferStreamVideoMemoryLimit() const = 0;

  // Total number of video frames which are populating in the pipeline when
  // prerolling.
  // You can expect more start delay by increasing this.
  virtual int GetMaxVideoPrerollFrames() const {
    return limits::kMaxVideoFrames;
  }
  // Total number of video frames which are populating in the pipeline.
  // You can expect more memory usage and less jitter by increasing this.
  virtual int GetMaxVideoFrames() const {
    return limits::kMaxVideoFrames;
  }

 private:
  static void SetInstance(ShellMediaPlatform* shell_media_platform);

  // Platform specific media Init and Tear down.
  virtual void InternalInitialize() const = 0;
  virtual void InternalTerminate() const = 0;

  DISALLOW_COPY_AND_ASSIGN(ShellMediaPlatform);
};

}  // namespace media

#define REGISTER_SHELL_MEDIA_PLATFORM(ClassName) \
    namespace media {                            \
    void ShellMediaPlatform::Initialize() {      \
      DCHECK(!Instance());                       \
      SetInstance(new ClassName);                \
      Instance()->InternalInitialize();          \
    }                                            \
    void ShellMediaPlatform::Terminate() {       \
      DCHECK(Instance());                        \
      Instance()->InternalTerminate();           \
      delete Instance();                         \
      SetInstance(NULL);                         \
    }                                            \
  }  // namespace media

#endif  // MEDIA_BASE_SHELL_MEDIA_PLATFORM_H_
