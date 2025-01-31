#!/bin/bash
set -eux

#---------------------------------
# Reusable common kokoro functions.
# --------------------------------


log_cobalt_header () {
  set +x
  echo "############################################################################################################"
  echo "#::'######:::'#######::'########:::::'###::::'##::::'########::::'##::::::::'#######:::'######::::'######::#"
  echo "#:'##... ##:'##.... ##: ##.... ##:::'## ##::: ##::::... ##..::::: ##:::::::'##.... ##:'##... ##::'##... ##:#"
  echo "#: ##:::..:: ##:::: ##: ##:::: ##::'##:. ##:: ##::::::: ##::::::: ##::::::: ##:::: ##: ##:::..::: ##:::..::#"
  echo "#: ##::::::: ##:::: ##: ########::'##:::. ##: ##::::::: ##::::::: ##::::::: ##:::: ##: ##::'####:. ######::#"
  echo "#: ##::::::: ##:::: ##: ##.... ##: #########: ##::::::: ##::::::: ##::::::: ##:::: ##: ##::: ##:::..... ##:#"
  echo "#: ##::: ##: ##:::: ##: ##:::: ##: ##.... ##: ##::::::: ##::::::: ##::::::: ##:::: ##: ##::: ##::'##::: ##:#"
  echo "#:. ######::. #######:: ########:: ##:::: ##: ########: ##::::::: ########:. #######::. ######:::. ######::#"
  echo "#::......::::.......:::........:::..:::::..::........::..::::::::........:::.......::::......:::::......:::#"
  echo "############################################################################################################"
  set -x
}
log_cobalt_header


get_kokoro_env () {
  # Kokoro doesn't provide a direct way to detect which environment the job is
  # running in: https://yaqs.corp.google.com/eng/q/3810666508825133056
  # This is a workaround that uses the artifacts directory where the first path
  # segment is the env: "<environment>/<job name>/<build number>/<date>"
  echo "${KOKORO_BUILD_ARTIFACTS_SUBDIR}" | cut -d'/' -f1
}


get_project_name () {
  if [[ "$(get_kokoro_env)" == "prod" ]]; then
    echo "cobalt-kokoro"
  elif [[ "$(get_kokoro_env)" == "qa" ]]; then
    echo "cobalt-kokoro-qa"
  else
    exit 1
  fi
}


get_bucket_name () {
  if [[ "$(get_kokoro_env)" == "prod" ]]; then
    echo "cobalt-internal-build-artifacts"
  elif [[ "$(get_kokoro_env)" == "qa" ]]; then
    echo "cobalt-internal-build-artifacts-qa"
  else
    exit 1
  fi
}


configure_environment () {
  # Tell git the repo is safe.
  git config --global --add safe.directory "${WORKSPACE_COBALT}"

  # Use Kokor's default credentials instead of boto file.
  unset BOTO_PATH

  # Add repository root to PYTHONPATH.
  export PYTHONPATH="${WORKSPACE_COBALT}${PYTHONPATH:+:${PYTHONPATH}}"

  # Setup SCCACHE.
  if [ -n "${SCCACHE_GCS_KEY_FILE_NAME+set}" ]; then
    export SCCACHE_GCS_KEY_PATH="${KOKORO_KEYSTORE_DIR}/${SCCACHE_GCS_KEY_FILE_NAME}"
  fi

  # In some instances KOKORO_GOB_BRANCH_src is not set, set by default to 'COBALT'.
  if [ -z "${KOKORO_GOB_BRANCH_src+set}" ]; then
    echo 'WARNING: $KOKORO_GOB_BRANCH_src is unset. Defaulting to "COBALT"'
    export KOKORO_GOB_BRANCH_src="COBALT"
  fi

  # In some instances kokoro uses KOKORO_GOB_BRANCH instead.
  if [ -z "${KOKORO_GOB_BRANCH+set}" ]; then
    echo 'WARNING: $KOKORO_GOB_BRANCH is unset. Defaulting to "COBALT"'
    export KOKORO_GOB_BRANCH="COBALT"
  fi

  env | sort
}

configure_dind_environment () {
  export REGISTRY_PATH="$(get_registry_bucket_path)"

  # Authenticate with GCR or GAR.
  echo "Logging in to registry with access token"
  set +x  #  Temporarily turn off debug logging to hide access token.
  printf "$(get_registry_authentication_token)" | docker login -u \
    oauth2accesstoken --password-stdin "https://${REGISTRY_HOSTNAME}"

  printf "$(get_registry_authentication_token)" | docker login -u \
    oauth2accesstoken --password-stdin "https://marketplace.gcr.io"
  set -x
}

get_registry_authentication_token () {
  # Hide access token in logs.
  set +x
  # Helper method that uses a well-known workaround for authenticating Kokoro
  # workers to access GCP hosted container/artifiact registries.
  # go/gcs-tpc-bigstore-user-guide#authentication-http-bearer-tokensaccess-tokens
  local svc_account="${REGISTRY_METADATA}/instance/service-accounts/default"
  local access_token=$(curl -H 'Metadata-Flavor: Google' ${svc_account}/token | cut -d'"' -f 4)
  echo "${access_token}"
  set -x
}

get_registry_bucket_path () {
  # Outputs the fully specified URL to the registry to push/pull images to/from.
  echo "${REGISTRY_HOSTNAME}/${REGISTRY_GCP_PROJECT}/$(get_kokoro_env)"
}

init_gcloud () {
  # Check if gsutil is pre-installed on Kokoro windows container.
  local gsutil_win="c:/gcloud-sdk/google-cloud-sdk/bin/gsutil.cmd"
  if [[ -f "${gsutil_win}" ]]; then
    export GSUTIL="${gsutil_win}"
    return
  fi

  local gsutil="$(command -v gsutil)"

  # Installs Google Cloud CLI if not already present.
  if [[ ! -f "${gsutil}" ]]; then
    apt-get update -y
    apt-get install -y apt-transport-https ca-certificates gnupg
    curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | \
      gpg --dearmor -o /usr/share/keyrings/cloud.google.gpg
    echo "deb [signed-by=/usr/share/keyrings/cloud.google.gpg] https://packages.cloud.google.com/apt cloud-sdk main" | \
      tee -a /etc/apt/sources.list.d/google-cloud-sdk.list
    apt-get update -y
    apt-get install -y google-cloud-cli
    rm -rf "/var/lib/apt/lists"/* "/tmp"/* "/var/tmp"/*
    rm -rf "/var/lib"/{apt,dpkg,cache,log}

    gsutil="$(command -v gsutil)"
  fi

  export GSUTIL="${gsutil}"
}


is_release_build () {
  [[ "${KOKORO_ROOT_JOB_TYPE}" == "RELEASE" ]]
}


is_release_config () {
  [[ "${RELEASE_CONFIGS}" =~ "${CONFIG}" ]]
}


run_gn () {
  if [ $# -ne 3 ]; then
    echo "Error: Exactly 3 arguments required!"
    exit 1
  fi

  local out_dir=$1
  local target_platform=$2
  local gn_arguments=$3

  set +u
  echo 'Running GN...'
  gn gen "${out_dir}" --args="target_platform=\"${target_platform}\" \
    ${SB_API_VERSION} \
    ${TARGET_OS} \
    ${TARGET_CPU} \
    ${gn_arguments} \
    build_type=\"${CONFIG}\""
  set -u
  gn check "${out_dir}"
}


ninja_build () {
  if [ $# -ne 2 ]; then
    echo "Error: Exactly 2 arguments required!"
    exit 1
  fi

  local out_dir=$1
  local targets=("$2")

  sccache -z

  echo 'Running Ninja build...'
  time ninja -C "${out_dir}" ${targets[@]}

  sccache -s

  echo "Generated build info"
  build_info_file="${out_dir}/gen/build_info.json"
  if [ -f $build_info_file ]; then
    echo "$(<$build_info_file)"
  else
    echo "No build_info.json file was generated."
  fi
}


upload_on_device_test_artifacts () {
  if [[ $# -ne 4 ]]; then
    echo "Error: Exactly 4 arguments required!"
    exit 1
  fi

  local target_platform=$1
  local platform=$2
  local config=$3
  local out_dir=$4

  local artifact="${WORKSPACE_COBALT}/builder_tmp/${platform}_${config}/artifacts.tgz"
  local gcs_archive_path="gs://$(get_project_name)-test-artifacts/${target_platform}/${KOKORO_GIT_COMMIT_src}/${KOKORO_ROOT_BUILD_ID}/${platform}_${config}/artifacts.tgz"

  # TODO(b/294130306): Simple file filtering to reduce package sizes.
  # Disable globbing to prevent the wildcard expansion in the patterns below.
  set -f
  # Include the test_infra patterns by default. This covers raspi and android.
  # The flag can not be used with any of the platforms below as it replaces the
  # patterns passed via the --pattern flag.
  local pattern_arg="--test_infra"
  if [[ "${platform}" == "ps5" ]]; then
    pattern_arg='--patterns */ app_launcher/'
  elif [[ "${platform}" == "xb1" ]]; then
    pattern_arg='--patterns appx/ app_launcher/'
  fi

  time python3 "${WORKSPACE_COBALT}/tools/create_archive.py" \
    ${pattern_arg} \
    -s "${out_dir}" \
    -d "${artifact}"
  # Re-enable globbing.
  set +f

  init_gcloud

  # Uploads test artifact.
  time "${GSUTIL}" cp "${artifact}" "${gcs_archive_path}"

  # Passes gcs path to on device test jobs.
  echo -n "${gcs_archive_path}" > "${KOKORO_ARTIFACTS_DIR}/gcs_archive_path"
}


create_and_upload_nightly_archive () {
  if [[ $# -ne 4 ]]; then
    echo "Error: Exactly 4 arguments required!"
    exit 1
  fi

  local platform=$1
  local package_dir=$2
  local local_archive_path=$3
  local build_info_path=$4

  local gcs_path_suffix="_nightly"
  if [[ "${KOKORO_GOB_BRANCH_src}" != "COBALT" ]]; then
    gcs_path_suffix="_${KOKORO_GOB_BRANCH_src}"
  fi
  local gcs_archive_path="gs://$(get_bucket_name)/${platform}${gcs_path_suffix}/$(date +%F)/${KOKORO_ROOT_BUILD_NUMBER}/"

  init_gcloud

  "${GSUTIL}" cp -r "${package_dir}" "${gcs_archive_path}"
}

run_package_release_pipeline () {
  # NOTE: For DinD builds, we only run the GN and Ninja steps in the container.
  # Artifacts and build-products from these steps are used in subsequent steps
  # for packaging and nightly post-build tasks, and do not need to be run in the
  # inner container environment (which has build tools and deps).

  local out_dir="${WORKSPACE_COBALT}/out/${TARGET_PLATFORM}_${CONFIG}"

  # Package and upload nightly release archive.
  if is_release_build && is_release_config; then
    # Setup package dir.
    local package_dir="${WORKSPACE_COBALT}/package/${PLATFORM}_${CONFIG}"
    mkdir -p "${package_dir}"

    # TODO(b/294130306): Move build_info to gn packaging.
    local build_info_path="${out_dir}/gen/build_info.json"
    cp "${build_info_path}" "${package_dir}/"

    # Create release package.
    export PYTHONPATH="${WORKSPACE_COBALT}"
    if [[ "${PLATFORM}" =~ "android" ]]; then
      python3 "${WORKSPACE_COBALT}/cobalt/build/android/package.py" \
        --name=cobalt-android "${out_dir}" "${package_dir}"
    elif [[ "${PLATFORM}" =~ "evergreen" ]]; then
      local bootloader_out_dir=
      if [ -n "${BOOTLOADER:-}" ]; then
        bootloader_out_dir="${WORKSPACE_COBALT}/out/${BOOTLOADER}_${CONFIG}"
      fi
      # Creates Evergreen package directory.
      python3 "${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/build/evergreen/simple_packager.py" \
        "${out_dir}" \
        "${package_dir}" \
        "${bootloader_out_dir:-}"
    else
      python3 "${WORKSPACE_COBALT}/cobalt/build/linux/package.py" \
        --name=cobalt-linux "${out_dir}" "${package_dir}"
    fi

    # Create and upload nightly archive.
    create_and_upload_nightly_archive \
      "${PLATFORM}" \
      "${package_dir}" \
      "${package_dir}.tar.gz" \
      "${build_info_path}"
  fi
}
