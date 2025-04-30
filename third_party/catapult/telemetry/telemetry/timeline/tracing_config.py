# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
from telemetry.timeline import atrace_config
from telemetry.timeline import chrome_trace_config
from telemetry.timeline import system_trace_config


class TracingConfig():
  """Tracing config is the configuration for tracing in Telemetry.

  TracingConfig configures tracing in Telemetry. It contains tracing options
  that control which core tracing system should be enabled. If a tracing
  system requires additional configuration, e.g., what to trace, then it is
  typically configured in its own config class. TracingConfig provides
  interfaces to access the configuration for those tracing systems.

  Options:
      enable_atrace_trace: a boolean that specifies whether to enable
          atrace tracing.
      enable_cpu_trace: a boolean that specifies whether to enable cpu tracing.
      enable_chrome_trace: a boolean that specifies whether to enable
          chrome tracing.
      enable_platform_display_trace: a boolean that specifies whether to
          platform display tracing.
      enable_android_graphics_memtrack: a boolean that specifies whether
          to enable the memtrack_helper daemon to track graphics memory on
          Android (see goo.gl/4Y30p9). Doesn't have any effects on other OSs.

  Detailed configurations:
      atrace_config: Stores configuration options specific to Atrace.
      chrome_trace_config: Stores configuration options specific to
          Chrome trace.
  """

  def __init__(self):
    self._enable_atrace_trace = False
    self._enable_platform_display_trace = False
    self._enable_android_graphics_memtrack = False
    self._enable_cpu_trace = False
    self._enable_chrome_trace = False
    self._enable_experimental_system_tracing = False
    self._force_sideload_perfetto = False

    self._atrace_config = atrace_config.AtraceConfig()
    self._chrome_trace_config = chrome_trace_config.ChromeTraceConfig()
    self._system_trace_config = system_trace_config.SystemTraceConfig()

  @property
  def enable_atrace_trace(self):
    return self._enable_atrace_trace

  @enable_atrace_trace.setter
  def enable_atrace_trace(self, value):
    self._enable_atrace_trace = value

  @property
  def enable_cpu_trace(self):
    return self._enable_cpu_trace

  @enable_cpu_trace.setter
  def enable_cpu_trace(self, value):
    self._enable_cpu_trace = value

  @property
  def enable_platform_display_trace(self):
    return self._enable_platform_display_trace

  @enable_platform_display_trace.setter
  def enable_platform_display_trace(self, value):
    self._enable_platform_display_trace = value

  @property
  def enable_android_graphics_memtrack(self):
    return self._enable_android_graphics_memtrack

  @enable_android_graphics_memtrack.setter
  def enable_android_graphics_memtrack(self, value):
    self._enable_android_graphics_memtrack = value

  @property
  def enable_chrome_trace(self):
    return self._enable_chrome_trace

  @enable_chrome_trace.setter
  def enable_chrome_trace(self, value):
    self._enable_chrome_trace = value

  @property
  def enable_experimental_system_tracing(self):
    return self._enable_experimental_system_tracing

  @enable_experimental_system_tracing.setter
  def enable_experimental_system_tracing(self, value):
    self._enable_experimental_system_tracing = value

  @property
  def force_sideload_perfetto(self):
    return self._force_sideload_perfetto

  @force_sideload_perfetto.setter
  def force_sideload_perfetto(self, value):
    self._force_sideload_perfetto = value

  @property
  def atrace_config(self):
    return self._atrace_config

  @property
  def chrome_trace_config(self):
    return self._chrome_trace_config

  @property
  def system_trace_config(self):
    return self._system_trace_config
