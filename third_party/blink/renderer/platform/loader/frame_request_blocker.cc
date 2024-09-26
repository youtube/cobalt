// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/frame_request_blocker.h"

#include "net/base/net_errors.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace blink {

class RequestBlockerThrottle : public URLLoaderThrottle,
                               public FrameRequestBlocker::Client {
 public:
  explicit RequestBlockerThrottle(
      scoped_refptr<FrameRequestBlocker> frame_request_blocker)
      : frame_request_blocker_(std::move(frame_request_blocker)) {}

  ~RequestBlockerThrottle() override {
    if (frame_request_blocker_)
      frame_request_blocker_->RemoveObserver(this);
  }

  // URLLoaderThrottle implementation:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    // Wait until this method to add as a client for FrameRequestBlocker because
    // this throttle could have moved sequences in the case of sync XHR.
    if (!frame_request_blocker_->RegisterClientIfRequestsBlocked(this)) {
      frame_request_blocker_ = nullptr;
      return;
    }

    *defer = true;
  }

  const char* NameForLoggingWillStartRequest() override {
    return "FrameRequestBlockerThrottle";
  }

  // FrameRequestBlocker::Client implementation:
  void Resume() override {
    frame_request_blocker_->RemoveObserver(this);
    frame_request_blocker_ = nullptr;
    delegate_->Resume();
  }

 private:
  scoped_refptr<FrameRequestBlocker> frame_request_blocker_;
};

FrameRequestBlocker::FrameRequestBlocker()
    : clients_(base::MakeRefCounted<base::ObserverListThreadSafe<Client>>()) {}

void FrameRequestBlocker::Block() {
  DCHECK(blocked_.IsZero());
  blocked_.Increment();
}

void FrameRequestBlocker::Resume() {
  // In normal operation it's valid to get a Resume without a Block.
  if (blocked_.IsZero())
    return;

  blocked_.Decrement();
  clients_->Notify(FROM_HERE, &Client::Resume);
}

std::unique_ptr<URLLoaderThrottle>
FrameRequestBlocker::GetThrottleIfRequestsBlocked() {
  if (blocked_.IsZero())
    return nullptr;

  return std::make_unique<RequestBlockerThrottle>(this);
}

void FrameRequestBlocker::RemoveObserver(Client* client) {
  clients_->RemoveObserver(client);
}

FrameRequestBlocker::~FrameRequestBlocker() = default;

bool FrameRequestBlocker::RegisterClientIfRequestsBlocked(Client* client) {
  if (blocked_.IsZero())
    return false;

  clients_->AddObserver(client);
  return true;
}

// static
scoped_refptr<WebFrameRequestBlocker> WebFrameRequestBlocker::Create() {
  return base::MakeRefCounted<FrameRequestBlocker>();
}

}  // namespace blink
