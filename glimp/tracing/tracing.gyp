# Copyright 2016 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  # When ENABLE_GLIMP_TRACING is defined in glimp_settings.gypi, the many
  # implementation functions that are annotated with GLIMP_TRACE_EVENT0 calls
  # will activate and allow profiling and flow visualization within glimp.
  #
  # With no configuration by the client, nothing will happen, however if a
  # client that is aware it is using glimp calls
  # glimp::SetTraceEventImplementation(), it can customize the behavior that
  # will occur at the start and end of each trace call.  For example, if
  # Chromium's base trace_event library is available to that client, they may
  # wish to call TRACE_EVENT_BEGIN0() and TRACE_EVENT_END0() at the beginning
  # and end of traces.
  'targets': [
    {

      'target_name': 'tracing',
      'type': 'static_library',

      'sources': [
        'tracing.h',
        'tracing.cc',
      ],
      'includes': [
        '../glimp_settings.gypi',
      ],
    },
  ],
}
