// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_CHROMEOS_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_CHROMEOS_H_

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/extensions/chromeos/extensions_chromeos_export.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/hid/cros_hid.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/managed_device_health_services/telemetry/cros_telemetry.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class EXTENSIONS_CHROMEOS_EXPORT ChromeOS : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ChromeOS();
  CrosWindowManagement* windowManagement(ExecutionContext*);
  CrosHID* hid(ExecutionContext*);
  CrosTelemetry* telemetry(ExecutionContext*);

  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_CHROMEOS_H_
