// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/display_cutout/display_cutout_host_impl.h"

#include "content/browser/display_cutout/display_cutout_constants.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {

DisplayCutoutHostImpl::DisplayCutoutHostImpl(WebContentsImpl* web_contents)
    : receivers_(web_contents, this), web_contents_impl_(web_contents) {}

DisplayCutoutHostImpl::~DisplayCutoutHostImpl() = default;

void DisplayCutoutHostImpl::BindReceiver(
    mojo::PendingAssociatedReceiver<blink::mojom::DisplayCutoutHost> receiver,
    RenderFrameHost* rfh) {
  receivers_.Bind(rfh, std::move(receiver));
}

void DisplayCutoutHostImpl::NotifyViewportFitChanged(
    blink::mojom::ViewportFit value) {
  ViewportFitChangedForFrame(receivers_.GetCurrentTargetFrame(), value);
}

void DisplayCutoutHostImpl::ViewportFitChangedForFrame(
    RenderFrameHost* rfh,
    blink::mojom::ViewportFit value) {
  if (GetValueOrDefault(rfh) == value)
    return;

  values_[rfh] = value;

  // If we are the current |RenderFrameHost| frame then notify
  // WebContentsObservers about the new value.
  if (current_rfh_.get() == rfh)
    web_contents_impl_->NotifyViewportFitChanged(value);
}

void DisplayCutoutHostImpl::DidAcquireFullscreen(RenderFrameHost* rfh) {
  SetCurrentRenderFrameHost(rfh);
}

void DisplayCutoutHostImpl::DidExitFullscreen() {
  SetCurrentRenderFrameHost(nullptr);
}

void DisplayCutoutHostImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If the navigation is not in the main frame or if we are a same document
  // navigation then we should stop now.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // If we finish a main frame navigation and the |WebDisplayMode| is
  // fullscreen then we should make the main frame the current
  // |RenderFrameHost|.  Note that this is probably not correct; we do not check
  // that the navigation completed successfully, nor do we check if the main
  // frame is still IsRenderFrameLive().
  blink::mojom::DisplayMode mode = web_contents_impl_->GetDisplayMode();
  if (mode == blink::mojom::DisplayMode::kFullscreen)
    SetCurrentRenderFrameHost(web_contents_impl_->GetPrimaryMainFrame());
}

void DisplayCutoutHostImpl::RenderFrameDeleted(RenderFrameHost* rfh) {
  values_.erase(rfh);

  // If we were the current |RenderFrameHost| then we should clear that.
  if (current_rfh_.get() == rfh)
    SetCurrentRenderFrameHost(nullptr);
}

void DisplayCutoutHostImpl::RenderFrameCreated(RenderFrameHost* rfh) {
  ViewportFitChangedForFrame(rfh, blink::mojom::ViewportFit::kAuto);
}

void DisplayCutoutHostImpl::SetDisplayCutoutSafeArea(gfx::Insets insets) {
  insets_ = insets;

  if (current_rfh_)
    SendSafeAreaToFrame(current_rfh_.get(), insets);
}

void DisplayCutoutHostImpl::SetCurrentRenderFrameHost(RenderFrameHost* rfh) {
  if (current_rfh_.get() == rfh)
    return;

  // If we had a previous frame then we should clear the insets on that frame.
  if (current_rfh_)
    SendSafeAreaToFrame(current_rfh_.get(), gfx::Insets());

  // If the new RenderFrameHost is nullptr we should stop here and notify
  // observers that the new viewport fit is kAuto (the default).
  if (!rfh) {
    current_rfh_ = nullptr;
    web_contents_impl_->NotifyViewportFitChanged(
        blink::mojom::ViewportFit::kAuto);
    return;
  }

  // Update the |current_rfh_| with the new frame.
  current_rfh_ = static_cast<RenderFrameHostImpl*>(rfh)->GetWeakPtr();

  // Send the current safe area to the new frame.
  SendSafeAreaToFrame(rfh, insets_);

  // Notify the WebContentsObservers that the viewport fit value has changed.
  web_contents_impl_->NotifyViewportFitChanged(GetValueOrDefault(rfh));
}

void DisplayCutoutHostImpl::SendSafeAreaToFrame(RenderFrameHost* rfh,
                                                gfx::Insets insets) {
  blink::AssociatedInterfaceProvider* provider =
      rfh->GetRemoteAssociatedInterfaces();
  if (!provider)
    return;

  mojo::AssociatedRemote<blink::mojom::DisplayCutoutClient> client;
  provider->GetInterface(client.BindNewEndpointAndPassReceiver());
  client->SetSafeArea(insets);
}

blink::mojom::ViewportFit DisplayCutoutHostImpl::GetValueOrDefault(
    RenderFrameHost* rfh) const {
  auto value = values_.find(rfh);
  if (value != values_.end())
    return value->second;
  return blink::mojom::ViewportFit::kAuto;
}

}  // namespace content
