// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_image_extractor.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {

namespace {

// A helper wrapper to manage a single GetImageBytes request.
// It is RefCounted because both the Mojo callback (OnResult) and the timeout
// timer (OnTimeout) hold strong references (scoped_refptr) to this object,
// keeping it alive until the request terminates and self-deletes.
class GetImageBytesRequest : public base::RefCounted<GetImageBytesRequest> {
 public:
  GetImageBytesRequest(
      mojo::Remote<blink::mojom::AIPageContentAgent> agent,
      base::OnceCallback<void(blink::mojom::AIPageContentImageBytesResultPtr)>
          callback,
      std::optional<base::TimeDelta> timeout)
      : agent_(std::move(agent)),
        callback_(std::move(callback)),
        timeout_(timeout) {}

  void Start(int32_t dom_node_id) {
    elapsed_timer_ = base::ElapsedTimer();

    if (timeout_.has_value()) {
      timeout_timer_.Start(FROM_HERE, *timeout_,
                           base::BindOnce(&GetImageBytesRequest::OnTimeout,
                                          base::WrapRefCounted(this)));
    }

    agent_->GetImageBytes(dom_node_id,
                          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                              base::BindOnce(&GetImageBytesRequest::OnResult,
                                             base::WrapRefCounted(this)),
                              /*result=*/nullptr));
  }

 private:
  friend class base::RefCounted<GetImageBytesRequest>;
  ~GetImageBytesRequest() = default;

  void OnResult(blink::mojom::AIPageContentImageBytesResultPtr result) {
    if (!callback_) {
      return;
    }

    timeout_timer_.Stop();
    agent_.reset();

    base::UmaHistogramBoolean(
        "OptimizationGuide.AIPageContent.GetImageBytes.Timeout", false);

    if (result) {
      base::UmaHistogramTimes(
          "OptimizationGuide.AIPageContent.GetImageBytes.Latency",
          elapsed_timer_.Elapsed());
      base::UmaHistogramCounts10M(
          "OptimizationGuide.AIPageContent.GetImageBytes.Size",
          base::saturated_cast<int>(result->image_bytes.size()));
    }

    std::move(callback_).Run(std::move(result));
  }

  void OnTimeout() {
    if (!callback_) {
      return;
    }

    // Move the callback first to invalidate `callback_`. This ensures that when
    // `agent_.reset()` triggers the Mojo destruction callback, `OnResult`
    // returns early and avoids logging duplicate metrics or running the user
    // callback.
    auto cb = std::move(callback_);
    agent_.reset();

    base::UmaHistogramBoolean(
        "OptimizationGuide.AIPageContent.GetImageBytes.Timeout", true);

    std::move(cb).Run(/*result=*/nullptr);
  }

  mojo::Remote<blink::mojom::AIPageContentAgent> agent_;
  base::OnceCallback<void(blink::mojom::AIPageContentImageBytesResultPtr)>
      callback_;
  std::optional<base::TimeDelta> timeout_;
  base::ElapsedTimer elapsed_timer_;
  base::OneShotTimer timeout_timer_;
};

}  // namespace

void GetImageBytes(
    content::WebContents* web_contents,
    const std::string& document_identifier,
    int32_t dom_node_id,
    base::OnceCallback<
        void(blink::mojom::AIPageContentImageBytesResultPtr result)> callback) {
  CHECK(web_contents);
  CHECK(callback);

  content::RenderFrameHost* target_rfh =
      GetRenderFrameForDocumentIdentifier(*web_contents, document_identifier);

  if (!target_rfh) {
    std::move(callback).Run(/*result=*/nullptr);
    return;
  }

  const content::RenderWidgetHost* target_rwh =
      target_rfh->GetRenderWidgetHost();
  if (!target_rwh) {
    std::move(callback).Run(/*result=*/nullptr);
    return;
  }

  // The `AIPageContentAgent` Mojo interface is only registered on local root
  // frames in the renderer. We must find the local root `RenderFrameHost` that
  // contains `target_rfh` and bind to its agent. Since DOM node IDs are
  // process-wide unique, the local root agent can successfully look up and
  // extract bytes for any node belonging to its subframes.
  content::RenderFrameHost* local_root = target_rfh;
  while (true) {
    content::RenderFrameHost* parent =
        local_root->GetParentOrOuterDocumentOrEmbedder();
    if (!parent || parent->GetRenderWidgetHost() != target_rwh) {
      break;
    }
    local_root = parent;
  }

  mojo::Remote<blink::mojom::AIPageContentAgent> agent;
  local_root->GetRemoteInterfaces()->GetInterface(
      agent.BindNewPipeAndPassReceiver());

  auto request = base::MakeRefCounted<GetImageBytesRequest>(
      std::move(agent), std::move(callback),
      features::GetAIPageContentGetImageBytesTimeout());
  request->Start(dom_node_id);
}

}  // namespace optimization_guide
