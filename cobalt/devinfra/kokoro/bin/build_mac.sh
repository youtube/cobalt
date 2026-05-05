#!/bin/bash
set -ueEx

# Load common routines.
. $(dirname "$0")/common.sh

# Using repository root as work directory.
export WORKSPACE_COBALT="${KOKORO_ARTIFACTS_DIR}/github/src"
cd "${WORKSPACE_COBALT}"

# Clean up workspace on exit or error.
trap "bash ${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/bin/cleanup.sh" EXIT INT TERM


# Mac build script.
pipeline () {
  # Run common configuration steps.
  configure_environment

  # Run mac specific setup steps.
  setup_mac

  local gclient_root="${KOKORO_ARTIFACTS_DIR}/github"
  git config --global --add safe.directory "${gclient_root}/src"
  local git_url="$(git -C "${gclient_root}/src" remote get-url origin)"

  # Set up gclient and run sync.
  ##############################################################################
  cd "${gclient_root}"
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git tools/depot_tools --filter=blob:none
  export PATH="${PATH}:${gclient_root}/tools/depot_tools"
  # Conditionally enable RBE variables
  local custom_vars=""
  if [[ "${CONFIG}" == "devel" || "${CONFIG}" == "qa" ]]; then
    custom_vars="\"custom_vars\": {'download_remoteexec_cfg': True, 'rbe_instance': 'projects/cobalt-actions-prod/instances/default_instance'},"
  fi

  # Write the .gclient file directly with managed = False
  cat <<EOF > .gclient
solutions = [
  { "name"        : 'src',
    "url"         : '${git_url}',
    "deps_file"   : 'DEPS',
    "managed"     : False,
    "custom_deps" : {
    },
    ${custom_vars}
  },
]
target_os=['ios']
EOF
  # -D, --delete_unversioned_trees
  # -f, --force force update even for unchanged modules
  # -R, --reset resets any local changes before updating (git only)
  echo "Jetski Debug: Git status before sync:"
  git -C "${gclient_root}/src" status
  echo "Jetski Debug: Git diff before sync:"
  git -C "${gclient_root}/src" diff

  echo "Jetski Debug: Resetting workspace..."
  git -C "${gclient_root}/src" reset --hard HEAD
  git -C "${gclient_root}/src" clean -dfx

  gclient sync -v \
    --shallow \
    --no-history \
    -D \
    -f \
    -R
  build_telemetry opt-out

  # Run GN and Ninja.
  ##############################################################################
  cd "${gclient_root}/src"
  echo "Jetski Debug: CONFIG is ${CONFIG}"
  local rbe_flag="--no-rbe"
  if [[ "${CONFIG}" == "devel" ]] || [[ "${CONFIG}" == "qa" ]]; then
    echo "Jetski Debug: Enabling RBE"
    rbe_flag=""
  fi

  echo "Jetski Debug: rbe_flag is ${rbe_flag}"
  echo "Jetski Debug: GN target is ${GN_TARGET}"
  cobalt/build/gn.py -p "${TARGET_PLATFORM}" -C "${CONFIG}" \
    --script-executable=/usr/bin/python3 ${rbe_flag}
  for gn_arg in ${EXTRA_GN_ARGUMENTS:-}; do
    echo "${gn_arg}" >> "out/${TARGET_PLATFORM}_${CONFIG}/args.gn"
  done
  # Build Cobalt.
  local out_dir="${WORKSPACE_COBALT}/out/${TARGET_PLATFORM}_${CONFIG}"
  autoninja -C "${out_dir}" ${GN_TARGET}  # GN_TARGET may expand to multiple args

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
  mkdir -p "$HOME/Library/Developer/Xcode/UserData/Provisioning Profiles"
  if [ -n "${KOKORO_PIPER_DIR:-}" ]; then
    cp -f ${KOKORO_PIPER_DIR}/google3/googlemac/iPhone/Shared/ProvisioningProfiles/*.mobileprovision \
        "$HOME/Library/Developer/Xcode/UserData/Provisioning Profiles"
  else
    echo "WARNING: KOKORO_PIPER_DIR is unset. Skipping provisioning profiles copy."
  fi

  export DEVELOPER_DIR="/Applications/Xcode_${XCODE_VERSION}.app/Contents/Developer"

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
