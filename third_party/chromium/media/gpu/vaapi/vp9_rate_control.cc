// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_rate_control.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

namespace media {
namespace {
class LibvpxVP9RateControl : public VP9RateControl {
 public:
  explicit LibvpxVP9RateControl(std::unique_ptr<libvpx::VP9RateControlRTC> impl)
      : impl_(std::move(impl)) {}

  ~LibvpxVP9RateControl() override = default;
  LibvpxVP9RateControl(const LibvpxVP9RateControl&) = delete;
  LibvpxVP9RateControl& operator=(const LibvpxVP9RateControl&) = delete;

  void UpdateRateControl(
      const libvpx::VP9RateControlRtcConfig& rate_control_config) override {
    impl_->UpdateRateControl(rate_control_config);
  }
  int GetQP() const override { return impl_->GetQP(); }
  int GetLoopfilterLevel() const override {
    return impl_->GetLoopfilterLevel();
  }
  void ComputeQP(const libvpx::VP9FrameParamsQpRTC& frame_params) override {
    impl_->ComputeQP(frame_params);
  }
  void PostEncodeUpdate(uint64_t encoded_frame_size) override {
    impl_->PostEncodeUpdate(encoded_frame_size);
  }

 private:
  const std::unique_ptr<libvpx::VP9RateControlRTC> impl_;
};

}  // namespace

// static
std::unique_ptr<VP9RateControl> VP9RateControl::Create(
    const libvpx::VP9RateControlRtcConfig& config) {
  auto impl = libvpx::VP9RateControlRTC::Create(config);
  if (!impl) {
    DLOG(ERROR) << "Failed creating libvpx::VP9RateControlRTC";
    return nullptr;
  }
  return std::make_unique<LibvpxVP9RateControl>(std::move(impl));
}
}  // namespace media
