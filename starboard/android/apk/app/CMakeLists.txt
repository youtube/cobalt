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

# The Gradle build already puts libcoat.so into the APK, but we have to
# declare it here as an imported library so that the debugger recognizes it.
#
# Since we don't build from source here, in order to debug in Android Studio
# you have to manually set the debugger type so that LLDB will be started:
# Run -> Edit Configurations -> "app" -> Debugger -> Debug Type = Dual

cmake_minimum_required(VERSION 3.4.1)
add_library(coat SHARED IMPORTED)
set_target_properties(coat PROPERTIES IMPORTED_LOCATION libcoat.so)
