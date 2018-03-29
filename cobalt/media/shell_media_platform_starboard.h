// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SHELL_MEDIA_PLATFORM_STARBOARD_H_
#define COBALT_MEDIA_SHELL_MEDIA_PLATFORM_STARBOARD_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "cobalt/media/base/shell_media_platform.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace media {

class ShellMediaPlatformStarboard : public ShellMediaPlatform {
 public:
  explicit ShellMediaPlatformStarboard(
      cobalt::render_tree::ResourceProvider* resource_provider)
      : resource_provider_(resource_provider) {
    SetInstance(this);
  }

  ~ShellMediaPlatformStarboard() {
    DCHECK_EQ(Instance(), this);
    SetInstance(NULL);
  }

  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() override {
#if SB_HAS(GRAPHICS)
    return resource_provider_->GetSbDecodeTargetGraphicsContextProvider();
#else   // SB_HAS(GRAPHICS)
    return NULL;
#endif  // SB_HAS(GRAPHICS)
  }

  void Suspend() override { resource_provider_ = NULL; }
  void Resume(render_tree::ResourceProvider* resource_provider) override {
    resource_provider_ = resource_provider;
  }

 private:
  void* AllocateBuffer(size_t size) override {
    UNREFERENCED_PARAMETER(size);
    NOTREACHED();
    return NULL;
  }
  void FreeBuffer(void* ptr) override {
    UNREFERENCED_PARAMETER(ptr);
    NOTREACHED();
  }
  size_t GetSourceBufferStreamAudioMemoryLimit() const override {
    NOTREACHED();
    return 0;
  }
  size_t GetSourceBufferStreamVideoMemoryLimit() const override {
    NOTREACHED();
    return 0;
  }

  cobalt::render_tree::ResourceProvider* resource_provider_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SHELL_MEDIA_PLATFORM_STARBOARD_H_
