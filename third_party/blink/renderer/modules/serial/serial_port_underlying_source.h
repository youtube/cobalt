// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_UNDERLYING_SOURCE_H_

#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/device/public/mojom/serial.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/underlying_byte_source_base.h"

namespace blink {

class ExceptionState;
class ScriptPromiseResolver;
class SerialPort;

class SerialPortUnderlyingSource : public UnderlyingByteSourceBase,
                                   ExecutionContextLifecycleObserver {
 public:
  SerialPortUnderlyingSource(ScriptState*,
                             SerialPort*,
                             mojo::ScopedDataPipeConsumerHandle);

  // UnderlyingByteSourceBase
  ScriptPromise Pull(ReadableByteStreamController* controller,
                     ExceptionState&) override;
  ScriptPromise Cancel(ExceptionState&) override;
  ScriptPromise Cancel(v8::Local<v8::Value> reason, ExceptionState&) override;
  ScriptState* GetScriptState() override;

  void ContextDestroyed() override;

  void SignalErrorOnClose(device::mojom::blink::SerialReceiveError);

  void Trace(Visitor*) const override;

 private:
  // Reads data from |data_pipe_|. Arms |watcher_| if it needs to wait.
  void ReadDataOrArmWatcher();

  void OnHandleReady(MojoResult, const mojo::HandleSignalsState&);
  void OnFlush(ScriptPromiseResolver*);
  void PipeClosed();
  void Close();

  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  mojo::SimpleWatcher watcher_;
  const Member<ScriptState> script_state_;
  const Member<SerialPort> serial_port_;
  Member<ReadableByteStreamController> controller_;
  ScriptValue pending_exception_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_UNDERLYING_SOURCE_H_
