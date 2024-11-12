// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_HOST_IPC_MOUSE_CURSOR_MONITOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

class DesktopSessionProxy;

// Routes webrtc::MouseCursorMonitor calls through the IPC channel to the
// desktop session agent running in the desktop integration process.
class IpcMouseCursorMonitor : public webrtc::MouseCursorMonitor {
 public:
  explicit IpcMouseCursorMonitor(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);

  IpcMouseCursorMonitor(const IpcMouseCursorMonitor&) = delete;
  IpcMouseCursorMonitor& operator=(const IpcMouseCursorMonitor&) = delete;

  ~IpcMouseCursorMonitor() override;

  // webrtc::MouseCursorMonitor interface.
  void Init(Callback* callback, Mode mode) override;
  void Capture() override;

  // Called when the cursor shape has changed.
  void OnMouseCursor(std::unique_ptr<webrtc::MouseCursor> cursor);

 private:
  // The callback passed to |webrtc::MouseCursorMonitor::Init()|.
  raw_ptr<webrtc::MouseCursorMonitor::Callback> callback_;

  // Wraps the IPC channel to the desktop session agent.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  // Used to cancel tasks pending on the capturer when it is stopped.
  base::WeakPtrFactory<IpcMouseCursorMonitor> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_MOUSE_CURSOR_MONITOR_H_
