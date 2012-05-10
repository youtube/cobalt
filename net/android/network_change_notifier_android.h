// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
#define NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
#pragma once

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/network_change_notifier.h"

namespace net {
namespace android {

class NetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  NetworkChangeNotifier();
  virtual ~NetworkChangeNotifier();

  void NotifyObservers(JNIEnv* env, jobject obj);

  static bool Register(JNIEnv* env);

 private:
  void CreateJavaObject(JNIEnv* env);

  // NetworkChangeNotifier:
  virtual bool IsCurrentlyOffline() const OVERRIDE;

  base::android::ScopedJavaGlobalRef<jobject> java_network_change_notifier_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifier);
};

}  // namespace android
}  // namespace net

#endif  // NET_ANDROID_NETWORK_CHANGE_NOTIFIER_ANDROID_H_
