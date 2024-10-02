// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * This class handles dialog fragments for casting, such as a {@link
 * MediaRouteChooserDialogFragment} or a {@link MediaRouteControllerDialogFragment}.
 *
 * TODO(rayankans): Expose MediaRouteDialog to the client side.
 */
class MediaRouteDialogFragmentEventHandler extends RemoteFragmentEventHandler {
    MediaRouteDialogFragmentEventHandler() {
        super(null /* args */);
    }
}
