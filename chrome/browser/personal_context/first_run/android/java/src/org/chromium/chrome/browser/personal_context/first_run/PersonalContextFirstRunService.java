// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.personal_context.first_run;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** Java proxy for the C++ PersonalContextFirstRunService. */
@NullMarked
public class PersonalContextFirstRunService {

    public static boolean shouldShowNotice(Profile profile) {
        return PersonalContextFirstRunServiceJni.get().shouldShowNotice(profile);
    }

    public static void noticeAcknowledged(Profile profile) {
        PersonalContextFirstRunServiceJni.get().noticeAcknowledged(profile);
    }

    @NativeMethods
    public interface Natives {
        boolean shouldShowNotice(Profile profile);

        void noticeAcknowledged(Profile profile);
    }
}
