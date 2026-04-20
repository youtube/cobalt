# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

# Since libcobalt.so is an "IMPORTED" library in CMake, in order to debug in
# Android Studio you have to manually set the debugger type so that LLDB will
# be started:
# Run -> Edit Configurations -> "app" -> Debugger -> Debug Type = Dual

cmake_minimum_required(VERSION 3.10.2)

# Map Android ABI to Cobalt architecture
if(ANDROID_ABI STREQUAL x86)
  set(COBALT_ARCH x86)
elseif(ANDROID_ABI STREQUAL armeabi-v7a)
  set(COBALT_ARCH arm)
elseif(ANDROID_ABI STREQUAL arm64-v8a)
  set(COBALT_ARCH arm64)
else()
  message(SEND_ERROR "Unsupported Android ABI: ${ANDROID_ABI}.")
endif()

# If COBALT_PRODUCT_DIR isn't set use the relative path up to the appropriate
# 'out' directory.
if(NOT COBALT_PRODUCT_DIR)
  set(COBALT_PRODUCT_DIR
      ${CMAKE_CURRENT_LIST_DIR}/../../../../out/android-${COBALT_ARCH}_${COBALT_CONFIG}
  )
endif()

# Abort gracefully if the config has not been generated. This should only happen
# when building from Android Studio which builds all supported ABIs by default.
if(NOT EXISTS "${COBALT_PRODUCT_DIR}")
  message(WARNING "android-${COBALT_ARCH}_${COBALT_CONFIG} not configured. Please run GN. Skipping config.")
  return()
endif()

# If COBALT_CONTENT_DIR isn't set for a particular deploy target use the
# 'content/data' directory.
if(NOT COBALT_CONTENT_DIR)
  set(COBALT_CONTENT_DIR
      ${COBALT_PRODUCT_DIR}/content/data
  )
endif()

# If COBALT_LIBRARY_DIR isn't set for a particular deploy target use the
# product root.
if(NOT COBALT_LIBRARY_DIR)
  set(COBALT_LIBRARY_DIR ${COBALT_PRODUCT_DIR})
endif()

# For platform deploy builds, use the -n parameter to skip Cobalt ninja and
# just copy the .so that was already built and waiting in COBALT_PRODUCT_DIR.
if(COBALT_PLATFORM_DEPLOY)
  set(skip_ninja_arg -n)
endif()

# Run Cobalt ninja, and copy the result as our "IMPORTED" libcobalt.so.
# ("coat_lib" never gets created, so this runs every time.)
add_custom_command(OUTPUT coat_lib
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/cobalt-ninja.sh
            ${skip_ninja_arg} -C ${COBALT_PRODUCT_DIR} ${COBALT_TARGET}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${COBALT_LIBRARY_DIR}/lib${COBALT_TARGET}.so
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libcobalt.so
)

if(EVERGREEN_COMPATIBLE)
  # Follow the pattern used to import libcobalt.so, above.
  add_custom_command(OUTPUT crashpad_handler_lib
      COMMAND ${CMAKE_CURRENT_LIST_DIR}/cobalt-ninja.sh
              ${skip_ninja_arg} -C ${COBALT_PRODUCT_DIR} crashpad_handler
      COMMAND ${CMAKE_COMMAND} -E copy
              ${COBALT_LIBRARY_DIR}/libcrashpad_handler.so
              ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libcrashpad_handler.so
  )
endif()

set (lib_deps coat_lib)
if(EVERGREEN_COMPATIBLE)
  list(APPEND lib_deps crashpad_handler_lib)
endif()

# Make a symlink to the cobalt static content. We put it in a parent directory
# of the library output, which corresponds to what the Gradle build sets for
# android.sourceSets.<build-type>.assets.srcDir. This ends up overwriting the
# symlink for each ABI built, but that's okay since the Cobalt static content
# for a given build config is the same for all architectures.
# ("cobalt_content" never gets created, so this runs every time.)
add_custom_command(OUTPUT cobalt_content
    DEPENDS ${lib_deps}
    COMMAND ${CMAKE_COMMAND} -E make_directory
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../../../../${COBALT_CONFIG}
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${COBALT_CONTENT_DIR}
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../../../../${COBALT_CONFIG}/cobalt_content
)

# We need a target (not a file) for the phony native dependency below.
add_custom_target(external_cobalt_build DEPENDS coat_lib cobalt_content)

# Declare libcobalt.so as a shared library that needs to be included in the APK.
# However, Android Studio will build it as an "IMPORTED" library.
add_library(coat SHARED IMPORTED)
set_target_properties(coat PROPERTIES
    IMPORTED_LOCATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libcobalt.so
)

if(EVERGREEN_COMPATIBLE)
  # Follow the pattern used to include libcobalt.so, above.
  add_library(crashpadhandler SHARED IMPORTED)
  set_target_properties(crashpadhandler PROPERTIES
      IMPORTED_LOCATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libcrashpad_handler.so
  )
endif()

# Make a phony "native" library, so Android Studio has something to build.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/phony.cpp "void __phony() {}")
add_library(native SHARED  ${CMAKE_CURRENT_BINARY_DIR}/phony.cpp)

# Add a dependency to run the external cobalt build as a side effect of
# building the phony native library.
add_dependencies(native external_cobalt_build)
