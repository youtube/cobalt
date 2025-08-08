// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_FRAME_TEST_UTIL_H_
#define FUCHSIA_WEB_COMMON_TEST_FRAME_TEST_UTIL_H_

#include <fuchsia/mem/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Uses |navigation_controller| to load |url| with |load_url_params|. Returns
// after the load is completed. Returns true if the load was successful, false
// otherwise.
bool LoadUrlAndExpectResponse(
    fuchsia::web::NavigationController* navigation_controller,
    fuchsia::web::LoadUrlParams load_url_params,
    base::StringPiece url);
bool LoadUrlAndExpectResponse(
    const fuchsia::web::NavigationControllerPtr& navigation_controller,
    fuchsia::web::LoadUrlParams load_url_params,
    base::StringPiece url);

// Executes |script| in the context of |frame|'s top-level document.
// Returns an un-set |absl::optional<>| on failure.
absl::optional<base::Value> ExecuteJavaScript(fuchsia::web::Frame* frame,
                                              base::StringPiece script);

// Creates and returns a LoadUrlParams with was_user_activated set to true.
// This allows user actions to propagate to the frame, allowing features such as
// autoplay to be used, which is used by many media tests.
fuchsia::web::LoadUrlParams CreateLoadUrlParamsWithUserActivation();

// Creates a WebMessage with one outgoing transferable set to
// |message_port_request| and data set to |buffer|.
fuchsia::web::WebMessage CreateWebMessageWithMessagePortRequest(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
    fuchsia::mem::Buffer buffer);

#endif  // FUCHSIA_WEB_COMMON_TEST_FRAME_TEST_UTIL_H_
