// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

/**
 * Lists base::Features that can be accessed through {@link PermissionsAndroidFeatureMap}.
 *
 * Should be kept in sync with |kFeaturesExposedToJava| in
 * components/permissions/android/permissions_android_feature_map.cc.
 */
public abstract class PermissionsAndroidFeatureList {
    public static final String ANDROID_APPROXIMATE_LOCATION_PERMISSION_SUPPORT =
            "AndroidApproximateLocationPermissionSupport";
    public static final String BLOCK_MIDI_BY_DEFAULT = "BlockMidiByDefault";

    public static final String PERMISSION_STORAGE_ACCESS = "PermissionStorageAccessAPI";
}
