#!/bin/bash
git restore --source=add-thread-based-memory-allocation-hooks \
  gin/BUILD.gn \
  gin/isolate_holder.cc \
  gin/v8_platform.cc \
  third_party/blink/renderer/bindings/core/v8/callback_invoke_helper.cc \
  third_party/blink/renderer/bindings/core/v8/v8_script_runner.cc \
  v8/BUILD.gn \
  v8/src/baseline/baseline-batch-compiler.cc \
  v8/src/compiler-dispatcher/lazy-compile-dispatcher.cc \
  v8/src/compiler-dispatcher/optimizing-compile-dispatcher.cc \
  v8/src/handles/traced-handles.cc \
  v8/src/heap/array-buffer-sweeper.cc \
  v8/src/heap/concurrent-marking.cc \
  v8/src/heap/cppgc/concurrent-marker.cc \
  v8/src/heap/cppgc/marker.cc \
  v8/src/heap/cppgc/sweeper.cc \
  v8/src/heap/mark-compact.cc \
  v8/src/heap/scavenger.cc \
  v8/src/heap/sweeper.cc \
  v8/src/maglev/maglev-concurrent-dispatcher.cc \
  v8/src/wasm/module-compiler.cc \
  v8/src/wasm/module-decoder.cc \
  v8/src/wasm/wasm-serialization.cc
