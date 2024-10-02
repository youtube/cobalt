// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.usb;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatService;

/** See {@link UsbNotificationServiceImpl}. */
public class UsbNotificationService extends SplitCompatService {
    @IdentifierNameString
    private static String sImplClassName =
            "org.chromium.chrome.browser.app.usb.UsbNotificationServiceImpl";

    public UsbNotificationService() {
        super(sImplClassName);
    }
}
