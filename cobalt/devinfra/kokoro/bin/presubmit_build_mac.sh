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
  luci-auth context -service-account-json "${KOKORO_KEYSTORE_DIR}/${GCLOUD_KEY_FILE_NAME}" -- \
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
  local rbe_flag="--no-rbe"
  if [[ "${CONFIG}" == "devel" ]] || [[ "${CONFIG}" == "qa" ]]; then
    rbe_flag=""
  fi

  cobalt/build/gn.py -p "${TARGET_PLATFORM}" -C "${CONFIG}" \
    --script-executable=/usr/bin/python3 ${rbe_flag}
  for gn_arg in ${EXTRA_GN_ARGUMENTS:-}; do
    echo "${gn_arg}" >> "out/${TARGET_PLATFORM}_${CONFIG}/args.gn"
  done
  # Build Cobalt.
  local out_dir="${WORKSPACE_COBALT}/out/${TARGET_PLATFORM}_${CONFIG}"

  # Extract test targets from JSON.
  local test_targets_json="${WORKSPACE_COBALT}/cobalt/build/testing/targets/tvos-arm64-simulator/test_targets.json"
  if [[ -f "${test_targets_json}" ]]; then
    # Extract test targets from the JSON file (list of dicts schema)
    # and format them as a space-separated list of target names (after the colon).
    local json_targets=$(python3 -c "import json, re; data=json.loads(re.sub(r'//.*', '', open('${test_targets_json}').read())); targets=[e['target'] for e in data if isinstance(e, dict) and 'target' in e]; print(' '.join(t.split(':')[-1] for t in targets))")
    GN_TARGET="${GN_TARGET:-} ${json_targets}"
  fi

  autoninja -C "${out_dir}" ${GN_TARGET}

  if [[ "${GN_TARGET}" == *"cobalt_browsertests"* ]]; then
    echo "Running Cobalt Browser Tests in Simulator..."
    time "${out_dir}"/iossim \
     -x tvos \
     -d "Apple TV 4K (3rd generation)" \
     -v \
     -i \
     -c '--gtest_filter=ContentMainRunnerImplBrowserTest.*' \
     "${out_dir}"/cobalt_browsertests.app
  fi

  # TODO(b/507872651): Revisit this and see if we could shard tests on multiple bots.
  # if has_simulator_tests; then
  #   time python3 "${WORKSPACE_COBALT}/cobalt/tools/buildbot/run_unit_tests.py" \
  #     --platform "${PLATFORM}" \
  #     --config "${CONFIG}" \
  #     --action run
  # fi
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

  # Authenticate gcloud service account if key is available in keystore.
  if [ -n "${KOKORO_KEYSTORE_DIR:-}" ] && [ -n "${GCLOUD_KEY_FILE_NAME:-}" ] && [ -f "${KOKORO_KEYSTORE_DIR}/${GCLOUD_KEY_FILE_NAME}" ]; then
    echo "Authenticating Gcloud Service Account..."
    gcloud auth activate-service-account --key-file="${KOKORO_KEYSTORE_DIR}/${GCLOUD_KEY_FILE_NAME}"
    export SISO_CREDENTIAL_HELPER="gcloud"
  fi
}

has_simulator_tests () {
  [[ "${PLATFORM}" == "darwin-tvos-simulator" && "${CONFIG}" == "devel" ]]
}


# Run the pipeline.
pipeline
