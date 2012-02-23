// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COMPLETION_CALLBACK_H__
#define NET_BASE_COMPLETION_CALLBACK_H__
#pragma once

#include "base/callback.h"
#include "base/cancelable_callback.h"

namespace net {

// A callback specialization that takes a single int parameter. Usually this is
// used to report a byte count or network error code.
typedef base::Callback<void(int)> CompletionCallback;

typedef base::CancelableCallback<void(int)> CancelableCompletionCallback;

}  // namespace net

#endif  // NET_BASE_COMPLETION_CALLBACK_H__
