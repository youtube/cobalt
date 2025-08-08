// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_sdk_impl.h"

#include "base/android/jni_android.h"
#include "third_party/cardboard/src/sdk/include/cardboard.h"

using base::android::AttachCurrentThread;

namespace device {

CardboardSdkImpl::CardboardSdkImpl() = default;
CardboardSdkImpl::~CardboardSdkImpl() = default;

void CardboardSdkImpl::Initialize(jobject context) {
  if (initialized_) {
    return;
  }

  Cardboard_initializeAndroid(base::android::GetVM(), context);
  initialized_ = true;
}

void CardboardSdkImpl::ScanQrCodeAndSaveDeviceParams() {
  CHECK(initialized_);

  // Launches a new QR code scanner activity in order to scan a QR code with
  // the parameters of a new Cardboard viewer. Whether the QR code with
  // Cardboard viewer paramerter is scanned or the scanning is skipped, the
  // scanner activity is finished.
  CardboardQrCode_scanQrCodeAndSaveDeviceParams();
}

}  // namespace device
