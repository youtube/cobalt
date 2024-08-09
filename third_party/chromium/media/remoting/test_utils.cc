// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/remoting/test_utils.h"
#include "third_party/chromium/media/remoting/receiver_controller.h"

namespace media_m96 {
namespace remoting {

void ResetForTesting(ReceiverController* controller) {
  controller->receiver_.reset();
  controller->media_remotee_.reset();
}

}  // namespace remoting
}  // namespace media_m96
