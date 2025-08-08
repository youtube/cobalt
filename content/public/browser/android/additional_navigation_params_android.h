// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"

namespace content {

CONTENT_EXPORT base::android::ScopedJavaLocalRef<jobject>
CreateJavaAdditionalNavigationParams(
    JNIEnv* env,
    base::UnguessableToken initiator_frame_token,
    int initiator_process_id,
    absl::optional<base::UnguessableToken> attribution_src_token,
    absl::optional<network::AttributionReportingRuntimeFeatures>
        runtime_features);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_ADDITIONAL_NAVIGATION_PARAMS_ANDROID_H_
