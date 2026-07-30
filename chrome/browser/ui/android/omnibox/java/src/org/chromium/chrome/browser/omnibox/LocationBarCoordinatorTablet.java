// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.build.annotations.NullMarked;

/** A supplement to {@link LocationBarCoordinator} with methods specific to larger devices. */
@NullMarked
public class LocationBarCoordinatorTablet implements LocationBarCoordinator.SubCoordinator {

    public LocationBarCoordinatorTablet(LocationBarTablet tabletLayout) {}

    @Override
    public void destroy() {}
}
