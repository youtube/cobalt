// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_output.h"

#include <wayland-server-protocol.h>

#include <optional>

#include "base/check_op.h"
#include "ui/display/types/display_constants.h"

namespace wl {

namespace {
// TODO(tluk): Update this to the latest supported wl_output version.
constexpr uint32_t kOutputVersion = 2;
}

TestOutput::TestOutput() : TestOutput(TestOutputMetrics()) {}

TestOutput::TestOutput(TestOutputMetrics metrics)
    : GlobalObject(&wl_output_interface, nullptr, kOutputVersion),
      metrics_(std::move(metrics)) {}

TestOutput::~TestOutput() = default;

// static
TestOutput* TestOutput::FromResource(wl_resource* resource) {
  return GetUserDataAs<TestOutput>(resource);
}

uint64_t TestOutput::GetOutputName(wl_client* client) const {
  return wl_global_get_name(global(), client);
}

void TestOutput::SetPhysicalAndLogicalBounds(const gfx::Rect& bounds) {
  metrics_.wl_physical_size = bounds.size();
  metrics_.wl_origin = bounds.origin();
  metrics_.xdg_logical_size = bounds.size();
  metrics_.xdg_logical_origin = bounds.origin();
}

void TestOutput::ApplyLogicalTranspose() {
  metrics_.xdg_logical_size.Transpose();
}

void TestOutput::SetOrigin(const gfx::Point& wl_origin) {
  metrics_.wl_origin = wl_origin;
}

void TestOutput::SetScale(int32_t wl_scale) {
  metrics_.wl_scale = wl_scale;
}

void TestOutput::SetLogicalSize(const gfx::Size& xdg_logical_size) {
  metrics_.xdg_logical_size = xdg_logical_size;
}

void TestOutput::SetLogicalOrigin(const gfx::Point& xdg_logical_origin) {
  metrics_.xdg_logical_origin = xdg_logical_origin;
}

void TestOutput::SetPanelTransform(wl_output_transform wl_panel_transform) {
  metrics_.wl_panel_transform = wl_panel_transform;
}

const gfx::Size& TestOutput::GetPhysicalSize() const {
  return metrics_.wl_physical_size;
}

const gfx::Point& TestOutput::GetOrigin() const {
  return metrics_.wl_origin;
}

int32_t TestOutput::GetScale() const {
  return metrics_.wl_scale;
}

void TestOutput::Flush() {
  constexpr char kUnknownMake[] = "unknown_make";
  constexpr char kUnknownModel[] = "unknown_model";

  wl_output_send_geometry(
      resource(), metrics_.wl_origin.x(), metrics_.wl_origin.y(),
      0 /* physical_width */, 0 /* physical_height */, 0 /* subpixel */,
      kUnknownMake, kUnknownModel, metrics_.wl_panel_transform);
  wl_output_send_mode(resource(), WL_OUTPUT_MODE_CURRENT,
                      metrics_.wl_physical_size.width(),
                      metrics_.wl_physical_size.height(), 0);
  wl_output_send_scale(resource(), metrics_.wl_scale);

  if (xdg_output_) {
    xdg_output_->Flush(metrics_);
  }
  wl_output_send_done(resource());
}

// Notifies clients about the changes in the output configuration via server
// events as soon as the output is bound (unless suppressed). Sending metrics at
// bind time is the most common behavior among Wayland compositors and is the
// case for the exo compositor.
void TestOutput::OnBind() {
  if (!suppress_implicit_flush_) {
    Flush();
  }
}

void TestOutput::SetXdgOutput(TestZXdgOutput* xdg_output) {
  xdg_output_ = xdg_output;
  // Make sure to send the necessary information for a client that
  // relies on the xdg information.
  if (!suppress_implicit_flush_) {
    Flush();
  }
}

}  // namespace wl
