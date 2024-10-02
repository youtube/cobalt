// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

// Technically this interface is not needed until we add a method to WebSettings with an
// android.webkit parameter or android.webkit return value. But for forwards compatibility all
// app-facing classes should have a boundary-interface that the WebView glue layer can build
// against.

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/**
 * Boundary interface for WebSettingsCompat.
 */
public interface WebSettingsBoundaryInterface {
    void setOffscreenPreRaster(boolean enabled);
    boolean getOffscreenPreRaster();

    void setSafeBrowsingEnabled(boolean enabled);
    boolean getSafeBrowsingEnabled();

    void setDisabledActionModeMenuItems(int menuItems);
    int getDisabledActionModeMenuItems();

    void setWillSuppressErrorPage(boolean suppressed);
    boolean getWillSuppressErrorPage();

    void setForceDark(int forceDarkMode);
    int getForceDark();

    void setAlgorithmicDarkeningAllowed(boolean allow);
    boolean isAlgorithmicDarkeningAllowed();

    @Retention(RetentionPolicy.SOURCE)
    @interface ForceDarkBehavior {
        int FORCE_DARK_ONLY = 0;
        int MEDIA_QUERY_ONLY = 1;
        int PREFER_MEDIA_QUERY_OVER_FORCE_DARK = 2;
    }

    void setForceDarkBehavior(@ForceDarkBehavior int forceDarkBehavior);
    @ForceDarkBehavior
    int getForceDarkBehavior();

    @Retention(RetentionPolicy.SOURCE)
    @interface WebAuthnSupport {
        int NONE = 0;
        int APP = 1;
        int BROWSER = 2;
    }

    void setWebAuthnSupport(@WebAuthnSupport int support);
    @WebAuthnSupport
    int getWebAuthnSupport();

    void setRequestedWithHeaderOriginAllowList(Set<String> allowedOriginRules);
    Set<String> getRequestedWithHeaderOriginAllowList();

    void setEnterpriseAuthenticationAppLinkPolicyEnabled(boolean enabled);
    boolean getEnterpriseAuthenticationAppLinkPolicyEnabled();

    void enableRestrictSensitiveWebContent();
}
