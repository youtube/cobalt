// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/presentation_service_delegate_observers.h"

#include "base/containers/contains.h"

namespace media_router {

PresentationServiceDelegateObservers::PresentationServiceDelegateObservers() {}

PresentationServiceDelegateObservers::~PresentationServiceDelegateObservers() {
  for (auto& observer_pair : observers_)
    observer_pair.second->OnDelegateDestroyed();
}

void PresentationServiceDelegateObservers::AddObserver(
    int render_process_id,
    int render_frame_id,
    content::PresentationServiceDelegate::Observer* observer) {
  DCHECK(observer);

  content::GlobalRenderFrameHostId rfh_id(render_process_id, render_frame_id);
  DCHECK(!base::Contains(observers_, rfh_id));
  observers_[rfh_id] = observer;
}

void PresentationServiceDelegateObservers::RemoveObserver(int render_process_id,
                                                          int render_frame_id) {
  observers_.erase(
      content::GlobalRenderFrameHostId(render_process_id, render_frame_id));
}

}  // namespace media_router
