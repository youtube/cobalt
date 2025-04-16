#!/bin/bash
set -ueEx

# Load common routines.
. $(dirname "$0")/common.sh

# Using repository root as work directory.
WORKSPACE_COBALT="${KOKORO_ARTIFACTS_DIR}/git/src"
cd "${WORKSPACE_COBALT}"

# Clean up workspace on exit or error.
trap "bash ${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/bin/cleanup.sh" EXIT INT TERM


# Linux build script.
pipeline () {
  # Run common configuration steps.
  configure_environment

  # Build Cobalt.
  local out_dir="${WORKSPACE_COBALT}/out/${TARGET_PLATFORM}_${CONFIG}"
  run_gn "${out_dir}" "${TARGET_PLATFORM}" "${EXTRA_GN_ARGUMENTS:-}"
  ninja_build "${out_dir}" "${TARGET}"

  # Generate license file
  echo "Generating license file"
  vpython3 tools/licenses/licenses.py \
    credits --gn-target cobalt:gn_all --gn-out-dir out/${TARGET_PLATFORM}_${CONFIG} > licenses_cobalt.txt

  # Build bootloader config if set.
  if [ -n "${BOOTLOADER:-}" ]; then
    echo "Evergreen Loader is configured."
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
    echo "Evergreen Loader is not configured."
  fi

  # Package and upload nightly release archive.
  if is_release_build && is_release_config; then
    # Setup package dir.
    local package_dir="${WORKSPACE_COBALT}/package/${PLATFORM}_${CONFIG}"
    mkdir -p "${package_dir}"

    # TODO(b/294130306): Move build_info to gn packaging.
    local build_info_path="${out_dir}/gen/build_info.json"
    cp "${build_info_path}" "${package_dir}/"

    # Create release package.
    if [[ "${PLATFORM}" =~ "android" ]]; then
      # Creates Android package directory.
      python3 "${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/build/android/simple_packager.py" \
        "${out_dir}" \
        "${package_dir}" \
        "${WORKSPACE_COBALT}"
    elif [[ "${PLATFORM}" =~ "evergreen" ]]; then
      # Creates Evergreen package directory.
      python3 "${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/build/evergreen/simple_packager.py" \
        "${out_dir}" \
        "${package_dir}" \
        "${bootloader_out_dir:-}"
    else
      # Sets package directory as out directory.
      package_dir="${out_dir}"
    fi

    # Create and upload nightly archive.
    create_and_upload_nightly_archive \
      "${PLATFORM}" \
      "${package_dir}" \
      "${package_dir}.tar.gz" \
      "${build_info_path}"
  fi
}

# Run the pipeline.
pipeline
