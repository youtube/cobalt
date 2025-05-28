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

#include "cobalt/browser/h5vcc_runtime/h5vcc_runtime_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/android/shared/starboard_bridge.h"

using starboard::android::shared::StarboardBridge;
#endif

namespace h5vcc_runtime {

namespace {

std::vector<uint8_t> CompressBitmap(SkBitmap bitmap) {
  constexpr int kCompressionQuality = 97;
  std::vector<uint8_t> data;
  gfx::JPEGCodec::Encode(bitmap, kCompressionQuality, &data);
  return data;
}

}  // namespace

void WebContentsRenderToImageObserver::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  LOG(WARNING) << "##############\n DOMContentLoaded!";
}

void WebContentsRenderToImageObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  LOG(WARNING) << "##############\n DidFinishLoad! " << validated_url;
  if (!callback_) {
    LOG(WARNING) << "##############\n DidFinishLoad! CALLBACK called already"
                 << validated_url;
    return;
  }
  // std::vector<uint8_t> bytes = {10, 20, 30, 40};
  // std::move(callback_).Run(bytes);
  render_frame_host->GetView()->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> response_task_runner,
             H5vccRuntimeImpl::RenderToImageCallback callback,
             const SkBitmap& bitmap) {
            LOG(WARNING) << "##############\n Rendered to bitmap! "
                         << bitmap.width() << "x" << bitmap.height();
            auto encoded = CompressBitmap(bitmap);
            LOG(WARNING) << "size: " << encoded.size();
            response_task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(encoded)));
          },
          response_task_runner_, std::move(callback_)));
}

void WebContentsRenderToImageObserver::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  LOG(WARNING) << "##############\n DidFailLoad! " << validated_url;
}

// TODO (b/395126160): refactor mojom implementation on Android
H5vccRuntimeImpl::H5vccRuntimeImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccRuntime> receiver)
    : content::DocumentService<mojom::H5vccRuntime>(render_frame_host,
                                                    std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

void H5vccRuntimeImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccRuntime> receiver) {
  new H5vccRuntimeImpl(*render_frame_host, std::move(receiver));
}

void H5vccRuntimeImpl::GetAndClearInitialDeepLinkSync(
    GetAndClearInitialDeepLinkSyncCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  std::move(callback).Run(manager->GetAndClearDeepLink());
}

void H5vccRuntimeImpl::GetAndClearInitialDeepLink(
    GetAndClearInitialDeepLinkCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  std::move(callback).Run(manager->GetAndClearDeepLink());
}

void H5vccRuntimeImpl::RenderToImage(const std::string& url_spec,
                                     uint32_t width,
                                     uint32_t height,
                                     RenderToImageCallback callback) {
  content::WebContents::CreateParams create_params(
      render_frame_host().GetBrowserContext());
  create_params.context = render_frame_host().GetNativeView();
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(create_params));
  GURL url(url_spec);
  content::NavigationController::LoadURLParams load_url_params(url);
  web_contents->GetController().LoadURLWithParams(load_url_params);
  web_contents->GetRenderWidgetHostView()->SetBounds(
      gfx::Rect(5000, 5000, width, height));
  web_contents->GetNativeView()->Show();
  web_contents_rendering_to_images_observers_.push_back(
      std::make_unique<WebContentsRenderToImageObserver>(
          web_contents.get(), base::SingleThreadTaskRunner::GetCurrentDefault(),
          std::move(callback)));
  web_contents_rendering_to_images_.push_back(std::move(web_contents));
}

void H5vccRuntimeImpl::AddListener(
    mojo::PendingRemote<mojom::DeepLinkListener> listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojo::Remote<mojom::DeepLinkListener> listener_remote;
  listener_remote.Bind(std::move(listener));

  // Hold the remote mojom connection in DeepLinkManager (singleton), so that it
  // can be accessed anywhere.
  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  manager->AddListener(std::move(listener_remote));
}

}  // namespace h5vcc_runtime
