// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard_image_model_factory_impl.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"

ClipboardImageModelFactoryImpl::ClipboardImageModelFactoryImpl(
    Profile* primary_profile)
    : primary_profile_(primary_profile),
      idle_timer_(FROM_HERE,
                  base::Minutes(2),
                  this,
                  &ClipboardImageModelFactoryImpl::OnRequestIdle) {
  DCHECK(primary_profile_);
}

ClipboardImageModelFactoryImpl::~ClipboardImageModelFactoryImpl() = default;

void ClipboardImageModelFactoryImpl::Render(const base::UnguessableToken& id,
                                            const std::string& html_markup,
                                            const gfx::Size& bounding_box_size,
                                            ImageModelCallback callback) {
  DCHECK(!html_markup.empty());
  pending_list_.emplace_front(id, html_markup, bounding_box_size,
                              std::move(callback));
  StartNextRequest();
}

void ClipboardImageModelFactoryImpl::CancelRequest(
    const base::UnguessableToken& id) {
  if (request_ && request_->IsRunningRequest(id)) {
    request_->Stop(
        ClipboardImageModelRequest::RequestStopReason::kRequestCanceled);
    return;
  }

  auto iter = base::ranges::find(pending_list_, id,
                                 &ClipboardImageModelRequest::Params::id);
  if (iter == pending_list_.end())
    return;

  pending_list_.erase(iter);
}

void ClipboardImageModelFactoryImpl::Activate() {
  active_ = true;
  StartNextRequest();
}

void ClipboardImageModelFactoryImpl::Deactivate() {
  active_ = false;

  // Rendering will not stop if |active_until_empty_| has been set true by a
  // call to `RenderCurrentPendingRequests()`.
  if (active_until_empty_)
    return;

  if ((!request_ || !request_->IsModifyingClipboard()))
    return;

  // Stop the currently running request if it is modifying the clipboard.
  // ClipboardImageModelFactory is `Deactivate()`-ed prior to the user pasting
  // and a modified clipboard could interfere with pasting from
  // ClipboardHistory.
  pending_list_.emplace_front(request_->StopAndGetParams());
}

void ClipboardImageModelFactoryImpl::RenderCurrentPendingRequests() {
  active_until_empty_ = true;
  StartNextRequest();
}

void ClipboardImageModelFactoryImpl::OnShutdown() {
  // Reset |request_| to drop its reference to Profile, specifically the
  // RenderProcessHost of its WebContents.
  request_.reset();
}

void ClipboardImageModelFactoryImpl::StartNextRequest() {
  if (pending_list_.empty())
    active_until_empty_ = false;

  if (pending_list_.empty() || (!active_ && !active_until_empty_) ||
      (request_ && request_->IsRunningRequest())) {
    return;
  }

  if (!request_) {
    request_ = std::make_unique<ClipboardImageModelRequest>(
        primary_profile_,
        base::BindRepeating(&ClipboardImageModelFactoryImpl::StartNextRequest,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // Reset the timer that cleans up |request_|. If StartNextRequest() is not
  // called again in 2 minutes, |request_| will be reset.
  idle_timer_.Reset();
  request_->Start(std::move(pending_list_.front()));
  pending_list_.pop_front();
}

void ClipboardImageModelFactoryImpl::OnRequestIdle() {
  if (!request_)
    return;

  DCHECK(!request_->IsRunningRequest())
      << "Running requests should timeout or complete before being cleaned up.";
  request_.reset();
}
