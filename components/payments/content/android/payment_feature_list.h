// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_LIST_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_LIST_H_

#include <jni.h>

#include "base/feature_list.h"

namespace payments {
namespace android {

// Android only payment features in alphabetical order:

BASE_DECLARE_FEATURE(kAndroidAppPaymentUpdateEvents);

// If enabled, then the web merchant origin and web wallet parameters will be
// omitted from the isReadyToPayRequest. See: https://crbug.com/1406655.
BASE_DECLARE_FEATURE(kOmitParametersInReadyToPay);

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_LIST_H_
