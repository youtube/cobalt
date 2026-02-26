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

#include "content/public/browser/tvos/simple_begin_frame_observer.h"

#include "base/time/time.h"
#include "ui/compositor/compositor.h"

namespace content {

SimpleBeginFrameObserver::SimpleBeginFrameObserver(WebContents* web_contents,
                                                   ui::Compositor* compositor)
    : WebContentsUserData<SimpleBeginFrameObserver>(*web_contents),
      observed_compositor_(compositor) {}

SimpleBeginFrameObserver::~SimpleBeginFrameObserver() = default;

void SimpleBeginFrameObserver::OnBeginFrame(
    base::TimeTicks frame_begin_time,
    base::TimeDelta frame_interval,
    std::optional<base::TimeTicks> first_coalesced_frame_begin_time) {
  if (!callback_) {
    return;
  }

  double lastDisplayRefreshRate = 1.0 / frame_interval.InSecondsF();
  callback_.Run(lastDisplayRefreshRate);
}

void SimpleBeginFrameObserver::OnBeginFrameSourceShuttingDown() {
  StopObservingBeginFrames();
}

void SimpleBeginFrameObserver::StartObservingBeginFrame(
    OnBeginFrameCallback callback) {
  if (!observed_compositor_) {
    return;
  }
  callback_ = callback;
  observed_compositor_->AddSimpleBeginFrameObserver(this);
}

void SimpleBeginFrameObserver::StopObservingBeginFrames() {
  if (observed_compositor_) {
    observed_compositor_->RemoveSimpleBeginFrameObserver(this);
    observed_compositor_ = nullptr;
  }
  callback_.Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SimpleBeginFrameObserver);

}  // namespace content
