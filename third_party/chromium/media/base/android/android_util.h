// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_ANDROID_ANDROID_UTIL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_ANDROID_ANDROID_UTIL_H_

#include <memory>

#include "base/android/scoped_java_ref.h"

namespace media_m96 {

// TODO(crbug.com/765862): Remove the type. Use ScopedJavaGlobalRef directly.
using JavaObjectPtr =
    std::unique_ptr<base::android::ScopedJavaGlobalRef<jobject>>;

// A helper method to create a JavaObjectPtr.
JavaObjectPtr CreateJavaObjectPtr(jobject object);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_ANDROID_ANDROID_UTIL_H_
