#!/bin/bash
git restore --source=add-thread-based-memory-allocation-hooks-rebased-pr-7 \
  cobalt/memory/cobalt_memory_attribution_manager.h \
  cobalt/memory/cobalt_memory_attribution_manager.cc \
  cobalt/memory/BUILD.gn \
  cobalt/app/cobalt_main_delegate.cc \
  cobalt/shell/app/shell_main_delegate.cc \
  cobalt/browser/cobalt_browser_main_parts.cc \
  cobalt/browser/features.h \
  cobalt/browser/features.cc \
  cobalt/browser/BUILD.gn \
  cobalt/BUILD.gn \
  cobalt/build/configs/cobalt.gni \
  cobalt/android/apk/app/src/main/java/dev/cobalt/coat/CommandLineOverrideHelper.java \
  cobalt/android/cobalt_library_loader.cc
