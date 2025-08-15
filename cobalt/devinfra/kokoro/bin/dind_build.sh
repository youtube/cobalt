#!/bin/bash
#
# This script is the entrypoint script for the inner-container created from the
# Cobalt Docker Image. This image is the build environment for running GN and
# Ninja for the target platforms. It is invoked by the outer-container which is
# a generic Docker-in-Docker capable linux image.
#
# Kokoro Instance
# └── Generic DinD Image
#     ├── dind_builder_runner.sh
#     │   ├── configure_environment (common.sh)
#     │   ├── main_build_image_and_run.py
#     │   │   └── Specific Cobalt Image
#     │   │       └── dind_build.sh                             <= THIS SCRIPT
#     │   └── run_package_release_pipeline (common.sh)
#     └── dind_runner.sh
#         ├── configure_environment (common.sh)
#         ├── main_pull_image_and_run.py
#         │   └── Specific Cobalt Image
#         │       └── dind_build.sh                             <= THIS SCRIPT
#         └── run_package_release_pipeline (common.sh)

set -ueEx

# Invalid image check:
# If this imge is invalid, then the file `/invalid_image_check` will exist. This
# indicates that the Docker image built is a placeholder failure artifact. The
# current build should exit with failure after outputting a helpful log message
# to guide people to check the failed preceding image build steps.
if [ -f "/invalid_image_check" ]; then
  set +x
  echo "*****************************************************"
  echo "Docker build from a previous step has failed."
  echo "See logs from the corresponding QA-build for this job"
  echo "The QA config version of this job ends with '/qa'."
  echo "This Docker image is an invalid placeholder artifact."
  echo "*****************************************************"
  set -x
  exit 1
fi

# Load common routines (we only need run_gn and ninja_build).
. $(dirname "$0")/common.sh

WORKSPACE_COBALT="${KOKORO_ARTIFACTS_DIR}/git/src"

configure_environment

pipeline () {
  local out_dir="${WORKSPACE_COBALT}/out/${TARGET_PLATFORM}_${CONFIG}"
  local gclient_root="${KOKORO_ARTIFACTS_DIR}/chromium"
  mkdir "${gclient_root}"
  cp -a "${WORKSPACE_COBALT}" "${gclient_root}/src"  # work around b/382250271

  # Set up gclient and run sync.
  ##############################################################################
  cd "${gclient_root}"
  # Clone depot_tools as the GitHub action does, rather than Kokoro doing it.
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git tools/depot_tools
  export PATH="${PATH}:${gclient_root}/tools/depot_tools"
  gclient config --name=src rpc://lbshell-internal/cobalt_src
  if [[ "${TARGET_PLATFORM}" =~ "android" ]]; then
    echo "target_os=['android']" >> .gclient
  fi
  git config --global --add safe.directory "${gclient_root}/src"
  gclient sync -v --shallow --no-history -r "${KOKORO_GIT_COMMIT_src}"

  # Run GN and Ninja.
  ##############################################################################
  cd "${gclient_root}/src"
  cobalt/build/gn.py -p "${TARGET_PLATFORM}" -C "${CONFIG}"
  ninja -C "out/${TARGET_PLATFORM}_${CONFIG}" ${TARGET}  # TARGET may expand to multiple args
  cp -a out "${WORKSPACE_COBALT}"  # preserve build outputs

  cd "${WORKSPACE_COBALT}"
  rm -rf "${gclient_root}"  # remove extraneous artifacts

  # Build bootloader config if set.
  if [ -n "${BOOTLOADER:-}" ]; then
    echo "Evergreen Loader (or Bootloader) is configured."

    # Deploy static manifest.json (see b/338287855#comment24)
    if [[ "${PLATFORM}" =~ "evergreen-arm-hardfp" ]]; then
      cp "${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/bin/manifest.json" "${out_dir}/install/usr/share/manifest.json"
    elif [[ "${PLATFORM}" =~ "evergreen-arm-softfp" ]]; then
      cp "${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/bin/manifest.json" "${WORKSPACE_COBALT}/chrome/updater/version_manifest/manifest.json"
    fi

    local bootloader_out_dir="${WORKSPACE_COBALT}/out/${BOOTLOADER}_${CONFIG}"
    run_gn \
      "${bootloader_out_dir}" \
      "${BOOTLOADER}" \
      "${BOOTLOADER_EXTRA_GN_ARGUMENTS:-}"
    ninja_build "${bootloader_out_dir}" "${BOOTLOADER_TARGET}"
  else
    echo "Evergreen Loader (or Bootloader) is not configured."
  fi
}
pipeline
