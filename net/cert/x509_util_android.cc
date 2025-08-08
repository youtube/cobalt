// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_database.h"
#include "net/net_jni_headers/X509Util_jni.h"

using base::android::JavaParamRef;

namespace net {

void JNI_X509Util_NotifyTrustStoreChanged(JNIEnv* env) {
  CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
}

void JNI_X509Util_NotifyClientCertStoreChanged(JNIEnv* env) {
  CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
}

}  // namespace net
