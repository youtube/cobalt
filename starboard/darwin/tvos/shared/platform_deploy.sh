#!/bin/bash
set -x
# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

BUNDLE_PATH="$1"
DATA_PATH="$2"
EXECUTABLE_PATH="$3"
ENTITLEMENTS_PATH="$4"

SIGNING_IDENTITY="Apple Development"

# Check if bundle directory is created and an Info.plist is generated under it before copying bundle data and codesigning the bundle
if [[ ! -d $BUNDLE_PATH || ! -f "${BUNDLE_PATH}/Info.plist" ]]; then
  echo "FAILED: Info.plist has to be generated before entering this script."
  exit 1
fi

# Copy any data files required by the executable.
if [ -d "$DATA_PATH" ]; then
  mkdir -p "${BUNDLE_PATH}/content" || exit 1
  cp -r "${DATA_PATH}"/* "${BUNDLE_PATH}/content" || exit 1
fi

# Framework copy and sign
# mkdir -p "${BUNDLE_PATH}/Frameworks" || exit 1
# cp -r "./gen/intents_framework/YtIntent.framework" "${BUNDLE_PATH}/Frameworks" || exit 1
# cp -r "${BUNDLE_PATH}/Frameworks/YtIntent.framework/Metadata.appintents" "${BUNDLE_PATH}/" || exit 1
# /usr/bin/codesign --force --sign "$SIGNING_IDENTITY" --timestamp\=none --preserve-metadata\=identifier,entitlements,flags --generate-entitlement-der ${BUNDLE_PATH}/Frameworks/YtIntent.framework || exit 1

# Add the executable to the bundle.
cp $EXECUTABLE_PATH $BUNDLE_PATH || exit 1

# Sign the bundle with an entitlement if one is specified.
if [ -z "$ENTITLEMENTS_PATH" ]; then
  # Sign with an appropriate identity to avoid popups asking if the app should
  # be allowed to accept incoming connections. Without entitlements, this
  # bundle will only run with the simulator.
  codesign --force --sign "$SIGNING_IDENTITY" --timestamp=none "$BUNDLE_PATH" || exit 1
else
  # Sign with an appropriate identity. The target device needs to have the
  # relevant provisioning profile in order to install and run this bundle.
  KEYCHAIN=()
  if [ -f "$HOME/Library/Keychains/iPhoneDeveloperSigning.keychain" ]; then
   KEYCHAIN=("--keychain" "${HOME}/Library/Keychains/iPhoneDeveloperSigning.keychain")
  fi
  if ! codesign --force --sign "$SIGNING_IDENTITY" --deep "${KEYCHAIN[@]}" --entitlements "$ENTITLEMENTS_PATH" --timestamp=none "$BUNDLE_PATH"; then
   echo "FAILED: codesign could not be completed. Please follow all steps in starboard/darwin/tvos/shared/PRE_TEST_SETUP.md. Exiting..."
   exit 1
  fi
fi
