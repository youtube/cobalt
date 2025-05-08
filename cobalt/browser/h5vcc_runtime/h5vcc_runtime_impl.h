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

#ifndef COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_IMPL_H_
#define COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_IMPL_H_

#include <string>

#include "base/task/single_thread_task_runner.h"
#include "cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_runtime {

class WebContentsRenderToImageObserver : public content::WebContentsObserver {
 public:
  WebContentsRenderToImageObserver(
      content::WebContents* web_contents,
      scoped_refptr<base::SingleThreadTaskRunner> response_task_runner,
      mojom::H5vccRuntime::RenderToImageCallback callback)
      : content::WebContentsObserver(web_contents),
        response_task_runner_(response_task_runner),
        callback_(std::move(callback)) {}

  ~WebContentsRenderToImageObserver() override {}

  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

  // This method is invoked when the load is done, i.e. the spinner of the tab
  // will stop spinning, and the onload event was dispatched.
  //
  // If the WebContents is displaying replacement content, e.g. network error
  // pages, DidFinishLoad is invoked for frames that were not sending
  // navigational events before. It is safe to ignore these events.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // This method is like DidFinishLoad, but when the load failed or was
  // cancelled, e.g. window.stop() is invoked.
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> response_task_runner_;
  mojom::H5vccRuntime::RenderToImageCallback callback_;
};

// Implements the H5vccRuntime Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccRuntimeImpl : public content::DocumentService<mojom::H5vccRuntime> {
 public:
  // Creates a H5vccRuntimeImpl. The H5vccRuntimeImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::H5vccRuntime> receiver);

  H5vccRuntimeImpl(const H5vccRuntimeImpl&) = delete;
  H5vccRuntimeImpl& operator=(const H5vccRuntimeImpl&) = delete;

  void GetAndClearInitialDeepLinkSync(
      GetAndClearInitialDeepLinkSyncCallback) override;
  void GetAndClearInitialDeepLink(GetAndClearInitialDeepLinkCallback) override;
  void RenderToImage(const std::string& url,
                     uint32_t width,
                     uint32_t height,
                     RenderToImageCallback callback) override;

  void AddListener(mojo::PendingRemote<mojom::DeepLinkListener> listener);

 private:
  H5vccRuntimeImpl(content::RenderFrameHost& render_frame_host,
                   mojo::PendingReceiver<mojom::H5vccRuntime> receiver);

  std::vector<std::unique_ptr<content::WebContents>>
      web_contents_rendering_to_images_;
  std::vector<std::unique_ptr<WebContentsRenderToImageObserver>>
      web_contents_rendering_to_images_observers_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace h5vcc_runtime

#endif  // COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_IMPL_H_
