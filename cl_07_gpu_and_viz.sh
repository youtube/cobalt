#!/bin/bash
git restore --source=add-thread-based-memory-allocation-hooks \
  content/gpu/BUILD.gn \
  content/gpu/gpu_main.cc \
  content/gpu/in_process_gpu_thread.cc \
  components/viz/service/BUILD.gn \
  components/viz/service/display_embedder/compositor_gpu_thread.cc \
  components/viz/service/main/viz_compositor_thread_runner_impl.cc \
  gpu/command_buffer/service/scheduler.cc
