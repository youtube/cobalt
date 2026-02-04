// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SERVICE_PLATFORM_WINDOW_PROVIDER_SERVICE_H_
#define COBALT_MEDIA_SERVICE_PLATFORM_WINDOW_PROVIDER_SERVICE_H_

#include "base/functional/callback.h"
#include "cobalt/media/service/mojom/platform_window_provider.mojom.h"

namespace cobalt {
namespace media {

// This service is used by the renderer to obtain the SbWindow handle from the
// browser process. It uses a callback to lazily evaluate the window handle,
// ensuring the latest value is returned even if the window is created after
// the service is bound.
class PlatformWindowProviderService final
    : public mojom::PlatformWindowProvider {
 public:
  using GetWindowHandleCallback = base::RepeatingCallback<uint64_t()>;

  explicit PlatformWindowProviderService(
      GetWindowHandleCallback get_window_handle_cb);

  PlatformWindowProviderService(const PlatformWindowProviderService&) = delete;
  PlatformWindowProviderService& operator=(
      const PlatformWindowProviderService&) = delete;

  ~PlatformWindowProviderService() override;

 private:
  // mojom::PlatformWindowProvider implementation.
  void GetSbWindow(GetSbWindowCallback callback) override;

  GetWindowHandleCallback get_window_handle_cb_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SERVICE_PLATFORM_WINDOW_PROVIDER_SERVICE_H_
