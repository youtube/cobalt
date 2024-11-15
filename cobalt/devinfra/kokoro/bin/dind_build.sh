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

# Using repository root as work directory.
WORKSPACE_COBALT="${KOKORO_ARTIFACTS_DIR}/git/src"
cd "${WORKSPACE_COBALT}"

configure_environment

# Build Cobalt.
pipeline () {
  local out_dir="${WORKSPACE_COBALT}/out/${TARGET_PLATFORM}_${CONFIG}"
  run_gn "${out_dir}" "${TARGET_PLATFORM}" "${EXTRA_GN_ARGUMENTS:-}"
  ninja_build "${out_dir}" "${TARGET}"

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
