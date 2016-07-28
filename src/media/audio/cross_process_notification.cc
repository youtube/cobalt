// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cross_process_notification.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

CrossProcessNotification::CrossProcessNotification() {}

CrossProcessNotification::WaitForMultiple::WaitForMultiple(
    const Notifications* notifications) {
  Reset(notifications);
}

int CrossProcessNotification::WaitForMultiple::Wait() {
  DCHECK(CalledOnValidThread());
  int ret = WaitMultiple(*notifications_, wait_offset_);
  wait_offset_ = (ret + 1) % notifications_->size();
  return ret;
}

void CrossProcessNotification::WaitForMultiple::Reset(
    const Notifications* notifications) {
  DCHECK(CalledOnValidThread());
  wait_offset_ = 0;
  notifications_ = notifications;
  DCHECK(!notifications_->empty());
}
