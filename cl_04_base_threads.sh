#!/bin/bash
git restore --source=add-thread-based-memory-allocation-hooks \
  content/renderer/in_process_renderer_thread.cc \
  content/renderer/render_thread_impl.cc \
  cc/raster/categorized_worker_pool.cc \
  cc/raster/single_thread_task_graph_runner.cc \
  starboard/common/thread.h \
  starboard/common/thread.cc \
  starboard/common/thread_options.h \
  starboard/shared/starboard/player/job_thread.cc
