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

# This file is included in the direct_dependent_settings from
# glimp.gyp and in glimp_common.gypi.
{
  'include_dirs': [
    '<(DEPTH)/glimp/include',
  ],
  'defines': [
    # There doesn't appear to be any way to use the C preprocessor to do
    # string concatenation with the / character. This prevents us from using
    # the preprocessor to assemble an include file path, so we have to do
    # the concatenation here in GYP.
    # http://stackoverflow.com/questions/29601786/c-preprocessor-building-a-path-string
    'GLIMP_EGLPLATFORM_INCLUDE="../../<(sb_target_platform)/eglplatform_public.h"',
    'GLIMP_KHRPLATFORM_INCLUDE="../../<(sb_target_platform)/khrplatform_public.h"',
    # Uncomment the define below to enable and use tracing inside glimp.
    # 'ENABLE_GLIMP_TRACING',
],
}
