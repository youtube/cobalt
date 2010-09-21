// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_BODY_DRAINER_H_
#define NET_HTTP_HTTP_RESPONSE_BODY_DRAINER_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "net/base/completion_callback.h"

namespace net {

class HttpStream;
class IOBuffer;

class HttpResponseBodyDrainer {
 public:
  // The size in bytes of the buffer we use to drain the response body that
  // we want to throw away.  The response body is typically a small page just a
  // few hundred bytes long.  We set a limit to prevent it from taking too long,
  // since we may as well just create a new socket then.
  enum { kDrainBodyBufferSize = 16384 };
  enum { kTimeoutInSeconds = 5 };

  explicit HttpResponseBodyDrainer(HttpStream* stream);

  // Starts reading the body until completion, or we hit the buffer limit, or we
  // timeout.  After Start(), |this| will eventually delete itself.
  void Start();

 private:
  enum State {
    STATE_DRAIN_RESPONSE_BODY,
    STATE_DRAIN_RESPONSE_BODY_COMPLETE,
    STATE_NONE,
  };

  ~HttpResponseBodyDrainer();

  int DoLoop(int result);

  int DoDrainResponseBody();
  int DoDrainResponseBodyComplete(int result);

  void OnIOComplete(int result);
  void OnTimerFired();
  void Finish(int result);

  const scoped_ptr<HttpStream> stream_;
  State next_state_;
  scoped_refptr<IOBuffer> read_buf_;
  int total_read_;
  CompletionCallbackImpl<HttpResponseBodyDrainer> io_callback_;
  CompletionCallback* user_callback_;
  base::OneShotTimer<HttpResponseBodyDrainer> timer_;

  DISALLOW_COPY_AND_ASSIGN(HttpResponseBodyDrainer);
};

}  // namespace net

#endif // NET_HTTP_HTTP_RESPONSE_BODY_DRAINER_H_
