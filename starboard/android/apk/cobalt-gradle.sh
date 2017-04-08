#!/bin/bash

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
echo "ANDROID_HOME: ${ANDROID_HOME}"
echo "ANDROID_NDK_HOME: ${ANDROID_NDK_HOME}"

exec $(dirname $0)/gradlew "$@"
