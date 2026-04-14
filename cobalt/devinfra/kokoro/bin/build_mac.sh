#!/bin/bash
set -ueEx

# Load common routines.
. $(dirname "$0")/common.sh

# Using repository root as work directory.
WORKSPACE_COBALT="${KOKORO_ARTIFACTS_DIR}/git/src"
cd "${WORKSPACE_COBALT}"

# Clean up workspace on exit or error.
trap "bash ${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/bin/cleanup.sh" EXIT INT TERM


# Mac build script.
pipeline () {
  # Run common configuration steps.
  configure_environment

  # Run mac specific setup steps.
  setup_mac

  # Build Cobalt.
  local out_dir="${WORKSPACE_COBALT}/out/${PLATFORM}_${CONFIG}"
  run_gn "${out_dir}" "${TARGET_PLATFORM}" "${EXTRA_GN_ARGUMENTS:-}"
  ninja_build "${out_dir}" "${TARGET}"

  # Package and upload nightly release archive.
  if is_release_build && is_release_config; then
    local package_dir="${WORKSPACE_COBALT}/package/${PLATFORM}_${CONFIG}"
    mkdir -p "${package_dir}"

    # TODO(b/294130306): Move build_info to gn packaging.
    local build_info_path="${out_dir}/gen/build_info.json"
    cp "${build_info_path}" "${package_dir}/"

    # Create release package.
    python3 "${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/build/tvos/simple_packager.py" \
      "${WORKSPACE_COBALT}" \
      "${out_dir}" \
      "${package_dir}" \
      "${build_info_path}"

    # Create and upload nightly archive.
    create_and_upload_nightly_archive \
      "${PLATFORM}" \
      "${package_dir}" \
      "${package_dir}.tar.gz" \
      "${build_info_path}"
  fi

  if has_simulator_tests; then
    time python3 "${WORKSPACE_COBALT}/cobalt/tools/buildbot/run_unit_tests.py" \
      --platform "${PLATFORM}" \
      --config "${CONFIG}" \
      --action run
  fi
}


setup_mac () {
  # Pull signing profiles.
  mkdir -p "$HOME/Library/MobileDevice/Provisioning Profiles"
  cp -f ${KOKORO_PIPER_DIR}/google3/googlemac/iPhone/Shared/ProvisioningProfiles/*.mobileprovision \
      "$HOME/Library/MobileDevice/Provisioning Profiles"

  export DEVELOPER_DIR="/Applications/Xcode_${XCODE_VERSION}.app/Contents/Developer"

  # Install GN.
  cp -a ${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/third-party/mac/bin/gn \
    /usr/local/bin

  # Install Sccache v0.3.3 - last version before OpenDAL was introduced.
  # Sccache with OpenDAL doesn't work on Ventura internal cluster.
  sudo cp -a ${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/third-party/mac/bin/sccache \
    /usr/local/bin

  # Configure PATH.
  export PATH="/usr/local/opt/bison/bin:/usr/libexec:$PATH"
  # "gcloud storage" is GA and recommened over "gsutil". See
  # https://cloud.google.com/storage/docs/gsutil#gcloud-storage-command for
  # details.
  gcloud_storage_shim() {
    gcloud storage "$@"
  }
  export GSUTIL="gcloud_storage_shim"

  if is_release_build && is_release_config; then
    # Configure gsutil to use auth from keystore.
    gcloud auth activate-service-account --key-file="${KOKORO_KEYSTORE_DIR}/${GCLOUD_KEY_FILE_NAME}"
  fi
}


has_simulator_tests () {
  [[ "${PLATFORM}" == "darwin-tvos-simulator" && "${CONFIG}" == "devel" ]]
}


# Run the pipeline.
pipeline
