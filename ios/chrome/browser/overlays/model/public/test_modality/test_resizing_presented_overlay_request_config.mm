// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/test_modality/test_resizing_presented_overlay_request_config.h"

OVERLAY_USER_DATA_SETUP_IMPL(TestResizingPresentedOverlay);

TestResizingPresentedOverlay::TestResizingPresentedOverlay(const CGRect& frame)
    : frame_(frame) {}

TestResizingPresentedOverlay::~TestResizingPresentedOverlay() = default;
