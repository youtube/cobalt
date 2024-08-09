// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_REMOTING_TEST_UTILS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_REMOTING_TEST_UTILS_H_

namespace media_m96 {
namespace remoting {

class ReceiverController;

// Friend function for resetting the mojo binding in ReceiverController.
void ResetForTesting(ReceiverController* controller);

}  // namespace remoting
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_REMOTING_TEST_UTILS_H_
