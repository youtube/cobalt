// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "url/gurl.h"

#ifndef CHROME_COMMON_BOUND_SESSION_REQUEST_THROTTLED_LISTENER_H_
#define CHROME_COMMON_BOUND_SESSION_REQUEST_THROTTLED_LISTENER_H_

class BoundSessionRequestThrottledListener {
 public:
  enum class UnblockAction { kResume, kCancel };
  using ResumeOrCancelThrottledRequestCallback =
      base::OnceCallback<void(UnblockAction)>;

  BoundSessionRequestThrottledListener() = default;
  virtual ~BoundSessionRequestThrottledListener() = default;

  BoundSessionRequestThrottledListener(
      const BoundSessionRequestThrottledListener&) = delete;
  BoundSessionRequestThrottledListener& operator=(
      const BoundSessionRequestThrottledListener&) = delete;

  virtual void OnRequestBlockedOnCookie(
      ResumeOrCancelThrottledRequestCallback callback) = 0;
};
#endif  // CHROME_COMMON_BOUND_SESSION_REQUEST_THROTTLED_LISTENER_H_
