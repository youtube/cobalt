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

#include "cobalt/browser/h5vcc_accessibility/h5vcc_accessibility_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace h5vcc_accessibility {

H5vccAccessibilityImpl::H5vccAccessibilityImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccAccessibilityBrowser> receiver)
    : content::DocumentService<mojom::H5vccAccessibilityBrowser>(
          render_frame_host,
          std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
  platform_text_to_speech_helper_ =
      PlatformTextToSpeechHelper::Create(weak_ptr_factory_.GetWeakPtr());
}

H5vccAccessibilityImpl::~H5vccAccessibilityImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void H5vccAccessibilityImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccAccessibilityBrowser> receiver) {
  new H5vccAccessibilityImpl(*render_frame_host, std::move(receiver));
}

void H5vccAccessibilityImpl::IsTextToSpeechEnabledSync(
    IsTextToSpeechEnabledSyncCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(
      platform_text_to_speech_helper_->IsTextToSpeechEnabled());
}

void H5vccAccessibilityImpl::RegisterClient(
    mojo::PendingRemote<mojom::H5vccAccessibilityClient> client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  remote_clients_.Add(std::move(client));
}

void H5vccAccessibilityImpl::NotifyTextToSpeechChange() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& client : remote_clients_) {
    client->NotifyTextToSpeechChange();
  }
}

}  // namespace h5vcc_accessibility
