// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SENSOR_CHROME_SENSOR_DELEGATE_H_
#define CHROME_BROWSER_SENSOR_CHROME_SENSOR_DELEGATE_H_

#include "content/public/browser/sensor_delegate.h"

namespace content {
class RenderFrameHost;
}

class ChromeSensorDelegate : public content::SensorDelegate {
 public:
  ChromeSensorDelegate();
  ChromeSensorDelegate(const ChromeSensorDelegate&) = delete;
  ChromeSensorDelegate& operator=(const ChromeSensorDelegate&) = delete;
  ~ChromeSensorDelegate() override;

  // content::SensorDelegate overrides:
  void SetRequestedSensorIsAvailable(
      content::RenderFrameHost* render_frame_host,
      bool is_available) override;
  void OnSensorStarted(content::RenderFrameHost* render_frame_host) override;
  void OnSensorStopped(content::RenderFrameHost* render_frame_host) override;
  void OnSensorBlocked(content::RenderFrameHost* render_frame_host) override;
};

#endif  // CHROME_BROWSER_SENSOR_CHROME_SENSOR_DELEGATE_H_
