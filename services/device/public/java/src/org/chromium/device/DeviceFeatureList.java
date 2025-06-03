// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device;

import org.jni_zero.JNINamespace;

/**
 * Lists //services/device features that can be accessed through {@link DeviceFeatureMap}.
 *
 * Note: Features must be added to the array |kFeaturesExposedToJava| in
 * //services/device/public/cpp/device_feature_map.cc.
 */
@JNINamespace("features")
public abstract class DeviceFeatureList {
    public static final String GENERIC_SENSOR_EXTRA_CLASSES = "GenericSensorExtraClasses";
    public static final String WEBAUTHN_ANDROID_CRED_MAN = "WebAuthenticationAndroidCredMan";
    public static final String WEBAUTHN_ANDROID_CRED_MAN_FOR_HYBRID =
            "WebAuthenticationAndroidCredManForHybrid";
    public static final String WEBAUTHN_ANDROID_HYBRID_CLIENT_UI =
            "WebAuthenticationAndroidHybridClientUi";
    public static final String WEBAUTHN_ANDROID_INCOGNITO_CONFIRMATION =
            "WebAuthenticationAndroidIncognitoConfirmation";
    public static final String WEBAUTHN_CABLE_VIA_CREDMAN = "WebAuthenticationCableViaCredMan";
    public static final String WEBAUTHN_DONT_PRELINK_IN_PROFILES =
            "WebAuthenticationDontPrelinkInProfiles";
    public static final String WEBAUTHN_HYBRID_LINK_WITHOUT_NOTIFICATIONS =
            "WebAuthenticationHybridLinkWithoutNotifications";
}
