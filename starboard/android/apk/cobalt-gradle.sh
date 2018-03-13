#!/bin/bash
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

# Helper to set ANDROID_HOME and ANDROID_NDK_HOME from command-line args
# before running gradlew.

while [ "$1" ]; do
  case "$1" in
    --sdk) shift; ANDROID_HOME="$1" ;;
    --ndk) shift; ANDROID_NDK_HOME="$1" ;;
    *) break ;;
  esac
  shift
done

export ANDROID_HOME
export ANDROID_NDK_HOME
echo "ANDROID_HOME=${ANDROID_HOME}"
echo "ANDROID_NDK_HOME=${ANDROID_NDK_HOME}"
echo "TASK: ${@: -1}"

# Allow up to 4 parallel gradle builds, distributing among lock files based
# on a hash of the command and its arguments. Except on buildbot, only run 1
# gradle build at a time.
BUCKETS=4
if [ "${BUILDBOT_BUILDER_NAME}" ]; then
  BUCKETS=1
fi
MD5=$(echo "$@" | md5sum)
LOCKNUM=$(( ${BUCKETS} * 0x${MD5:0:6} / 0x1000000 ))

flock /var/lock/cobalt-gradle.lock.${LOCKNUM} $(dirname "$0")/gradlew "$@"
