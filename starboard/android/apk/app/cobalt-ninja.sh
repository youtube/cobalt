#! /bin/bash
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

# This wrapper allows us to run ninja without any assumptions about the
# environment having been setup correctly to build Cobalt, since that won't
# happen when started from Android Studio. See:
# https://cobalt.googlesource.com/cobalt/+/master/src/#Building-and-Running-the-Code

# Allow for a developer-specific environment setup from .cobaltrc
# e.g., it may set DEPOT_TOOLS and/or setup some distributed build tools.
local_rc=$(dirname $0)/.cobaltrc
global_rc=${HOME}/.cobaltrc
if [ -r ${local_rc} ]; then
  source ${local_rc}
elif [ -r ${global_rc} ]; then
  source ${global_rc}
fi

# DEPOT_TOOLS may be set in .cobaltrc, otherwise assume it's in $HOME.
[ -x ${DEPOT_TOOLS}/ninja ] || DEPOT_TOOLS=${HOME}/depot_tools

# Use Cobalt's clang if it's not anywhere earlier in the PATH.
SRC_DIR=$(cd $(dirname $0)/../../../..; pwd)
PATH=$PATH:${SRC_DIR}/third_party/llvm-build/Release+Asserts/bin

# Do nothing if -n is the first argument.
if [ "$1" == "-n" ]; then
  shift
  echo "Skipping: ninja $@"
  exit
fi

# When running in CMake, depot_tools isn't in the path, so we have to be
# explicit about which ninja to run. Fail if we didn't find depot_tools.
exec ${DEPOT_TOOLS}/ninja "$@"
