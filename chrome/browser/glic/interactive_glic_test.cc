// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/interactive_glic_test.h"

#include "base/scoped_observation_traits.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/polling_state_observer.h"

namespace glic::test {

namespace internal {

GlicWindowControllerStateObserver::GlicWindowControllerStateObserver(
    const GlicWindowController& controller)
    : PollingStateObserver([&controller]() { return controller.state(); }) {}
GlicWindowControllerStateObserver::~GlicWindowControllerStateObserver() =
    default;

DEFINE_STATE_IDENTIFIER_VALUE(GlicWindowControllerStateObserver,
                              kGlicWindowControllerState);

GlicAppStateObserver::GlicAppStateObserver(GlicWindowController* controler)
    : ObservationStateObserver(controler) {}

GlicAppStateObserver::~GlicAppStateObserver() = default;

void GlicAppStateObserver::WebUiStateChanged(mojom::WebUiState state) {
  OnStateObserverStateChanged(state);
}

DEFINE_STATE_IDENTIFIER_VALUE(GlicAppStateObserver, kGlicAppState);

}  // namespace internal

DEFINE_ELEMENT_IDENTIFIER_VALUE(kGlicHostElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kGlicContentsElementId);

const InteractiveBrowserTestApi::DeepQuery kPathToMockGlicCloseButton = {
    "#closebn"};
const InteractiveBrowserTestApi::DeepQuery kPathToGuestPanel = {
    ".panel#guestPanel"};

}  // namespace glic::test
