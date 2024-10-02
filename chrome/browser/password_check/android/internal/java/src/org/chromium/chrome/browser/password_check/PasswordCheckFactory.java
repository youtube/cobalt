// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * Use {@link #getOrCreate()} to instantiate a {@link PasswordCheckImpl}
 * and {@link #destroy()} when the instance is no longer needed.
 */
public class PasswordCheckFactory {
    private static PasswordCheck sPasswordCheck;
    private PasswordCheckFactory() {}

    /**
     * Creates a {@link PasswordCheckImpl} if none exists. Otherwise it returns the existing
     * instance.
     * @param settingsLauncher The {@link SettingsLauncher} to open the check page.
     * @return A {@link PasswordCheckImpl} or null if the feature is disabled.
     */
    public static @Nullable PasswordCheck getOrCreate(SettingsLauncher settingsLauncher) {
        if (sPasswordCheck == null) {
            sPasswordCheck = new PasswordCheckImpl(settingsLauncher);
        }
        return sPasswordCheck;
    }

    /**
     * Destroys the C++ layer to free up memory. In order to not leave a partially initialized
     * feature component alive, it sets the {@link PasswordCheckImpl} to null.
     *
     * Should be called by the last object alive who needs the feature. This is, in general,
     * the outermost settings screen. Note that this is not always {@link MainSettings}.
     */
    public static void destroy() {
        if (sPasswordCheck == null) return;
        sPasswordCheck.destroy();
        sPasswordCheck = null;
    }

    @VisibleForTesting
    public static void setPasswordCheckForTesting(PasswordCheck passwordCheck) {
        sPasswordCheck = passwordCheck;
    }

    /**
     * Returns the underlying instance.
     * Should only be used when there's a need to avoid creating a new instance.
     * @return A {@link PasswordCheeck} instance as stored here.
     */
    public static PasswordCheck getPasswordCheckInstance() {
        return sPasswordCheck;
    }
}
