// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/net_jni_registrar.h"

#include "base/basictypes.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "net/android/network_library.h"
#include "net/android/network_change_notifier_android.h"

namespace net {
namespace android {

static base::android::RegistrationMethod kNetRegisteredMethods[] = {
  { "AndroidNetworkLibrary", net::android::RegisterNetworkLibrary },
  { "NetworkChangeNotifier", net::android::NetworkChangeNotifier::Register },
};

bool RegisterJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kNetRegisteredMethods, arraysize(kNetRegisteredMethods));
}

}  // namespace android
}  // namespace net
