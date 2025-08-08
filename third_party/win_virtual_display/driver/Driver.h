// Copyright (c) Microsoft Corporation

#ifndef THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_DRIVER_H_
#define THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_DRIVER_H_

// Make sure we don't get min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include "Direct3DDevice.h"
#include "IndirectMonitor.h"
#include "SwapChainProcessor.h"
#include "Trace.h"

namespace display::test {

// Contains data and handles related to a single device (WDFDEVICE) object.
class IndirectDeviceContext {
 public:
  IndirectDeviceContext(_In_ WDFDEVICE WdfDevice);
  virtual ~IndirectDeviceContext();

  void InitAdapter();
  void FinishInit(UINT ConnectorIndex);

  std::vector<IndirectMonitor> monitors;

 protected:
  WDFDEVICE m_WdfDevice;
  IDDCX_ADAPTER m_Adapter;
};

// Contains data and handles related to a single monitor (IDDCX_MONITOR) object.
class IndirectMonitorContext {
 public:
  IndirectMonitorContext(_In_ IDDCX_MONITOR Monitor);
  virtual ~IndirectMonitorContext();

  void AssignSwapChain(IDDCX_SWAPCHAIN SwapChain,
                       LUID RenderAdapter,
                       HANDLE NewFrameEvent);
  void UnassignSwapChain();

  // Default modes reported for edid-less monitors. The first mode is set as
  // preferred
  std::vector<DriverProperties::MonitorMode> default_mode_list;

 private:
  IDDCX_MONITOR m_Monitor;
  std::unique_ptr<SwapChainProcessor> m_ProcessingThread;
};
}  // namespace display::test

#endif  // THIRD_PARTY_WIN_VIRTUAL_DISPLAY_DRIVER_DRIVER_H_
