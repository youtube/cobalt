// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SENSOR_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SENSOR_DELEGATE_H_

namespace content {

class RenderFrameHost;

// Delegate interface for the embedder to handle sensor events.
class SensorDelegate {
 public:
  virtual ~SensorDelegate() = default;

  virtual void SetRequestedSensorIsAvailable(RenderFrameHost* render_frame_host,
                                             bool is_available) = 0;
  virtual void OnSensorStarted(RenderFrameHost* render_frame_host) = 0;
  virtual void OnSensorStopped(RenderFrameHost* render_frame_host) = 0;
  virtual void OnSensorBlocked(RenderFrameHost* render_frame_host) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SENSOR_DELEGATE_H_
