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

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

// We can't include content/browser/media/captured_surface_controller.h because
// it's a private header and causes gn check errors.
// So we provide the implementation for the class here.

namespace content {

class CapturedSurfaceController {
 public:
  using CapturedSurfaceControlResult =
      ::blink::mojom::CapturedSurfaceControlResult;

  CapturedSurfaceController(
      GlobalRenderFrameHostId capturer_rfh_id,
      WebContentsMediaCaptureId /*captured_wc_id*/,
      base::RepeatingCallback<void(int)> /*on_zoom_level_change_callback*/);

  virtual ~CapturedSurfaceController();

  virtual void UpdateCaptureTarget(
      WebContentsMediaCaptureId /*captured_wc_id*/);

  virtual void SendWheel(blink::mojom::CapturedWheelActionPtr /*action*/,
                         base::OnceCallback<void(
                             CapturedSurfaceControlResult)> /*reply_callback*/);

  virtual void UpdateZoomLevel(
      blink::mojom::ZoomLevelAction /*action*/,
      base::OnceCallback<
          void(CapturedSurfaceControlResult)> /*reply_callback*/);

  virtual void RequestPermission(
      base::OnceCallback<
          void(CapturedSurfaceControlResult)> /*reply_callback*/);

  struct CapturedSurfaceInfo final {
    CapturedSurfaceInfo(
        base::WeakPtr<WebContents> captured_wc,
        std::unique_ptr<base::CallbackListSubscription,
                        BrowserThread::DeleteOnUIThread> subscription,
        int subscription_version,
        int initial_zoom_level);
    CapturedSurfaceInfo(CapturedSurfaceInfo&& other);
    CapturedSurfaceInfo& operator=(CapturedSurfaceInfo&& other);
    ~CapturedSurfaceInfo();

    base::WeakPtr<WebContents> captured_wc;
    std::unique_ptr<base::CallbackListSubscription,
                    BrowserThread::DeleteOnUIThread>
        subscription;
    int subscription_version;
    int initial_zoom_level;
  };

 private:
  const GlobalRenderFrameHostId capturer_rfh_id_;
};

CapturedSurfaceController::CapturedSurfaceController(
    GlobalRenderFrameHostId capturer_rfh_id,
    WebContentsMediaCaptureId /*captured_wc_id*/,
    base::RepeatingCallback<void(int)> /*on_zoom_level_change_callback*/)
    : capturer_rfh_id_(capturer_rfh_id) {
  NOTREACHED();
}

CapturedSurfaceController::~CapturedSurfaceController() = default;

void CapturedSurfaceController::UpdateCaptureTarget(
    WebContentsMediaCaptureId /*captured_wc_id*/) {
  NOTREACHED();
}

void CapturedSurfaceController::SendWheel(
    blink::mojom::CapturedWheelActionPtr /*action*/,
    base::OnceCallback<void(CapturedSurfaceControlResult)> /*reply_callback*/) {
  NOTREACHED();
}

void CapturedSurfaceController::UpdateZoomLevel(
    blink::mojom::ZoomLevelAction /*action*/,
    base::OnceCallback<void(CapturedSurfaceControlResult)> /*reply_callback*/) {
  NOTREACHED();
}

void CapturedSurfaceController::RequestPermission(
    base::OnceCallback<void(CapturedSurfaceControlResult)> /*reply_callback*/) {
  NOTREACHED();
}

CapturedSurfaceController::CapturedSurfaceInfo::CapturedSurfaceInfo(
    base::WeakPtr<WebContents> captured_wc,
    std::unique_ptr<base::CallbackListSubscription,
                    BrowserThread::DeleteOnUIThread> subscription,
    int subscription_version,
    int initial_zoom_level)
    : captured_wc(std::move(captured_wc)),
      subscription(std::move(subscription)),
      subscription_version(subscription_version),
      initial_zoom_level(initial_zoom_level) {}

CapturedSurfaceController::CapturedSurfaceInfo::CapturedSurfaceInfo(
    CapturedSurfaceInfo&& other) = default;

CapturedSurfaceController::CapturedSurfaceInfo&
CapturedSurfaceController::CapturedSurfaceInfo::operator=(
    CapturedSurfaceInfo&& other) = default;

CapturedSurfaceController::CapturedSurfaceInfo::~CapturedSurfaceInfo() =
    default;

}  // namespace content
