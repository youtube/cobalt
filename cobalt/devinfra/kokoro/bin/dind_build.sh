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
#     │   ├── main_build_image_and_run.py
#     │   │   └── Specific Cobalt Image
#     │   │       └── dind_build.sh                             <= THIS SCRIPT
#     │   └── run_package_release_pipeline (common.sh)
#     └── dind_runner.sh
#         ├── main_build_image_and_run.py
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

# Prints the revision to build.
#
# Release (nightly) builds of an LTS branch <N>.lts build the daily tag
# <N>.lts.nightly.<YYYY-MM-DD>, which a scheduled Kokoro job creates at the
# branch tip, so that every platform builds the same commit. The "nightly day"
# runs from 06:00 to 06:00 Pacific, so retries after midnight use the same tag.
# Falls back to the previous day's tag, then to the commit Kokoro checked out.
# All other builds use the commit Kokoro checked out.
get_build_revision () {
  local src_dir="$1"
  local branch="${KOKORO_GOB_BRANCH_src:-}"
  branch="${branch#refs/heads/}"
  if ! is_release_build || [[ ! "${branch}" =~ ^[0-9]+\.lts$ ]]; then
    echo "${KOKORO_GIT_COMMIT_src}"
    return
  fi
  if [[ ! -d "${src_dir}" ]]; then
    echo "WARNING: ${src_dir} does not exist; building ${KOKORO_GIT_COMMIT_src}." >&2
    echo "${KOKORO_GIT_COMMIT_src}"
    return
  fi
  local days_ago tag
  for days_ago in 0 1; do
    tag="${branch}.nightly.$(TZ=America/Los_Angeles date -d "-$(( 6 + 24 * days_ago )) hours" +%F)"
    if git -C "${src_dir}" fetch --depth=1 origin \
        "+refs/tags/${tag}:refs/tags/${tag}" >&2; then
      echo "Building nightly tag ${tag}." >&2
      git -C "${src_dir}" rev-parse "refs/tags/${tag}^{commit}"
      return
    fi
    echo "WARNING: Nightly tag ${tag} not found." >&2
  done
  echo "WARNING: No nightly tag found; building ${KOKORO_GIT_COMMIT_src}." >&2
  echo "${KOKORO_GIT_COMMIT_src}"
}

pipeline () {
  local out_dir="${WORKSPACE_COBALT}/out/${TARGET_PLATFORM}_${CONFIG}"
  local gclient_root="${KOKORO_ARTIFACTS_DIR}/git"

  git config --global --add safe.directory "${gclient_root}/src"
  local git_url="$(git -C "${gclient_root}/src" remote get-url origin)"

  # Set up gclient and run sync.
  ##############################################################################
  cd "${gclient_root}"
  configure_gob_auth
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git tools/depot_tools
  # TODO(b/562551706): Pinned before upstream 20aff01e (2026-09-16), which added
  # `--end-of-options` to `git checkout`. Need to update git on runners.
  git -C tools/depot_tools checkout 4a978d8f1f3567d5bd729aec018bfc345a14e1cd
  export DEPOT_TOOLS_UPDATE=0
  git config --global --add safe.directory '*'
  source tools/depot_tools/bootstrap_python3 && bootstrap_python3
  export PATH="${PATH}:${gclient_root}/tools/depot_tools"
  gclient config --name=src --custom-var='rbe_instance="projects/cobalt-actions-prod/instances/default_instance"' "${git_url}"
  if [[ "${TARGET_PLATFORM}" =~ (android|aosp) ]]; then
    echo "target_os=['android']" >> .gclient
  fi
  if [[ "${TARGET_PLATFORM}" =~ evergreen-arm64 ]]; then
    echo "target_cpu=['x64', 'arm64']" >> .gclient
  elif [[ "${TARGET_PLATFORM}" =~ evergreen-arm ]]; then
    echo "target_cpu=['x64', 'arm']" >> .gclient
  fi
  local build_revision
  build_revision="$(get_build_revision "${gclient_root}/src")"
  build_revision="${build_revision:-${KOKORO_GIT_COMMIT_src}}"
  # -D, --delete_unversioned_trees
  # -f, --force force update even for unchanged modules
  # -R, --reset resets any local changes before updating (git only)
  gclient sync -v \
    --shallow \
    --no-history \
    -D \
    -f \
    -R \
    -r "${build_revision}"
  build_telemetry opt-out

  # Run GN and Ninja.
  ##############################################################################
  cd "${gclient_root}/src"
  cobalt/build/gn.py -p "${TARGET_PLATFORM}" -C "${CONFIG}" \
    --script-executable=/usr/bin/python3
  for gn_arg in ${EXTRA_GN_ARGUMENTS:-}; do
    echo "${gn_arg}" >> "out/${TARGET_PLATFORM}_${CONFIG}/args.gn"
  done
  export SISO_CREDENTIAL_HELPER="gcloud"
  echo "Running autoninja build for ${TARGET_PLATFORM}_${CONFIG}..."
  autoninja -C "out/${TARGET_PLATFORM}_${CONFIG}" ${GN_TARGET}  # GN_TARGET may expand to multiple args

  # Build chromedriver used for testing.
  # TODO: Move this to separate build config.
  if [[ "${TARGET_PLATFORM}" =~ "linux-x64x11" ]]; then
    # Build the linux-x64x11-no-starboard configuration for chromedriver.
    LINUX_NO_SB_PLATFORM="linux-x64x11-no-starboard"
    cobalt/build/gn.py -p "${LINUX_NO_SB_PLATFORM}" -C "${CONFIG}" \
      --script-executable=/usr/bin/python3
    autoninja -C "out/${LINUX_NO_SB_PLATFORM}_${CONFIG}" "chromedriver"
  fi

  # Build evergreen loader config if set.
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

  # Copy libchrobalt.so to Kokoro Artifacts Directory for Android builds.
  if [[ "${TARGET_PLATFORM}" =~ android ]] && [[ -n "${KOKORO_ARTIFACTS_DIR:-}" ]]; then
    local build_out_dir="out/${TARGET_PLATFORM}_${CONFIG}"
    if [[ -d "${build_out_dir}" ]]; then
      echo "Copying libchrobalt.so to Kokoro Artifacts Directory..."
      mkdir -p "${KOKORO_ARTIFACTS_DIR}/lib_export"
      find "${build_out_dir}" -type f -name "libchrobalt.so" -exec cp {} "${KOKORO_ARTIFACTS_DIR}/lib_export/" \;
    fi
  fi
}
pipeline
