// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.content_public.common.ContentSwitches;

import java.util.List;
import java.util.Locale;

/** Data class for the JavalessRenderers feature. */
@NullMarked
public class JavalessRenderersFeatureList {
    public static final String JAVALESS_RENDERER_EXPERIMENT_ON = "JavalessRendererExperimentOn";

    public static final CachedFlag sJavalessRendererExperimentOn =
            new CachedFlag(
                    ContentFeatureMap.getInstance(),
                    JAVALESS_RENDERER_EXPERIMENT_ON,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);

    public static final List<CachedFlag> sCachedFlags = List.of(sJavalessRendererExperimentOn);

    private static @Nullable Boolean sEnabled;

    public static boolean isEnabled() {
        if (sEnabled == null) {
            decideEnabledState();
        }
        return sEnabled;
    }

    @EnsuresNonNull("sEnabled")
    private static void decideEnabledState() {
        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine.hasSwitch(ContentSwitches.JAVALESS_RENDERERS)) {
            String value = commandLine.getSwitchValue(ContentSwitches.JAVALESS_RENDERERS);
            if (!TextUtils.isEmpty(value)) {
                if (value.toLowerCase(Locale.ENGLISH).startsWith("enable")) {
                    sEnabled = true;
                    return;
                } else if (value.toLowerCase(Locale.ENGLISH).startsWith("disable")) {
                    sEnabled = false;
                    return;
                }
            }
            assert false : "--javaless-renderers switch requires value of 'enabled' or 'disabled'";
            sEnabled = false;
            return;
        }
        sEnabled = sJavalessRendererExperimentOn.isEnabled();
    }
}
