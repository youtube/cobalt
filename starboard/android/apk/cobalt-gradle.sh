#!/bin/bash
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

# Helper to set ANDROID_HOME and ANDROID_NDK_HOME from command-line args
# before running gradlew, as specified by leading --sdk and --ndk args.
#
# Also resets hung gradle builds when specified by a leading --reset arg.

GRADLE_ARGS=()
while [ "$1" ]; do
  case "$1" in
    --sdk) shift; ANDROID_HOME="$1" ;;
    --cache) shift; mkdir -p "$1";
             GRADLE_ARGS+=("--project-cache-dir" $(cd "$1"; pwd)) ;;
    --reset) RESET_GRADLE=1 ;;
    *) break ;;
  esac
  shift
done
GRADLE_ARGS+=("$@")

# Cleanup Gradle from previous builds. Used as part of the GYP step.
if [[ "${RESET_GRADLE}" ]]; then
  echo "Cleaning Gradle daemons and locks."
  # If there are any lock files, kill any hung processes still waiting on them.
  if compgen -G '/var/lock/cobalt-gradle.lock.*'; then
    lsof -t /var/lock/cobalt-gradle.lock.* | xargs -rt kill
  fi
  # Stop the Gradle daemon (if still running).
  $(dirname "$0")/gradlew --stop
  # Remove Gradle caches (including its lock files).
  rm -rf ${HOME}/.gradle/caches
  # Show the gradle version, which will cause it to download if needed.
  $(dirname "$0")/gradlew -v
  # After resetting, exit without running any gradle tasks.
  exit
fi

export ANDROID_HOME
echo "ANDROID_HOME=${ANDROID_HOME}"

# Allow parallel gradle builds, as defined by a COBALT_GRADLE_BUILD_COUNT envvar
# or default to 1 if that's not set (so buildbot only runs 1 gradle at a time).
BUCKETS=${COBALT_GRADLE_BUILD_COUNT:-1}
if [ "$BUCKETS" -eq 1 ]; then
  echo "Gradle daemon and parallel gradle disabled for Cobalt build"
  GRADLE_ARGS+=(
    "-Dorg.gradle.parallel=false"
    "-Dorg.gradle.workers.max=1"
    "-Dorg.gradle.daemon=false"
  )
fi

MD5=$(echo "${GRADLE_ARGS[@]}" | md5sum)
LOCKNUM=$(( ${BUCKETS} * 0x${MD5:0:6} / 0x1000000 ))

echo "TASK: ${GRADLE_ARGS[-1]}"
flock /var/lock/cobalt-gradle.lock.${LOCKNUM} $(dirname "$0")/gradlew "${GRADLE_ARGS[@]}"
