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


configure_environment () {
  # Tell git the repo is safe.
  git config --global --add safe.directory "${WORKSPACE_COBALT}"

  # Use Kokor's default credentials instead of boto file.
  unset BOTO_PATH

  # Ensure vpython virtualenvs are populated in a persistent directory for snapshot packaging
  if [ -n "${KOKORO_ARTIFACTS_DIR:-}" ]; then
    export VPYTHON_VIRTUALENV_ROOT="${KOKORO_ARTIFACTS_DIR}/git/vpython"
    mkdir -p "${VPYTHON_VIRTUALENV_ROOT}"
  fi

  # Add repository root to PYTHONPATH.
  export PYTHONPATH="${WORKSPACE_COBALT}${PYTHONPATH:+:${PYTHONPATH}}"

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
  gcloud config set account "$(curl -s -H 'Metadata-Flavor: Google' "${svc_account}/email")"
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

  if [[ -z "${GSUTIL:-}" ]]; then
    if command -v gcloud >/dev/null 2>&1 && gcloud storage --help >/dev/null 2>&1; then
      gcloud_storage_shim() {
        gcloud storage "$@"
      }
      export GSUTIL="gcloud_storage_shim"
    else
      local gsutil="$(command -v gsutil || true)"

      # Installs Google Cloud CLI if not already present.
      if [[ -z "${gsutil}" || ! -f "${gsutil}" ]]; then
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
    fi
  fi

  # Authenticate gcloud and gsutil if running on Kokoro or GCE.
  if command -v gcloud >/dev/null 2>&1; then
    gcloud config set pass_credentials_to_gsutil true 2>/dev/null || true
    local current_account
    current_account="$(gcloud config get-value account 2>/dev/null || true)"
    if [[ -z "${current_account}" || "${current_account}" == "(unset)" ]]; then
      local meta_url="${REGISTRY_METADATA:-http://metadata.google.internal./computeMetadata/v1}"
      local svc_account="${meta_url}/instance/service-accounts/default"
      local sa_email
      sa_email="$(curl -s -f -m 5 -H 'Metadata-Flavor: Google' "${svc_account}/email" 2>/dev/null || true)"
      if [[ -z "${sa_email}" ]]; then
        meta_url="http://169.254.169.254/computeMetadata/v1"
        svc_account="${meta_url}/instance/service-accounts/default"
        sa_email="$(curl -s -f -m 5 -H 'Metadata-Flavor: Google' "${svc_account}/email" 2>/dev/null || true)"
      fi
      if [[ -n "${sa_email}" ]]; then
        echo "Configuring gcloud account: ${sa_email}"
        gcloud config set account "${sa_email}" 2>/dev/null || true
      fi
    fi
  fi
}


is_release_build () {
  [[ "${KOKORO_ROOT_JOB_TYPE:-}" == "RELEASE" ]]
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
  local starboard_target_platform=$2
  local gn_arguments=$3

  set +u
  echo 'Running GN...'
  gn gen "${out_dir}" --args="starboard_target_platform=\"${starboard_target_platform}\" \
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

  echo 'Running Ninja build...'
  time ninja -C "${out_dir}" ${targets[@]}

  echo "Generated build info"
  build_info_file="${out_dir}/gen/build_info.json"
  if [ -f $build_info_file ]; then
    echo "$(<$build_info_file)"
  else
    echo "No build_info.json file was generated."
  fi
}

run_package_release_pipeline () {
  # NOTE: For DinD builds, we only run the GN and Ninja steps in the container.
  # Artifacts and build-products from these steps are used in subsequent steps
  # for packaging and nightly post-build tasks, and do not need to be run in
  # the inner container environment (which has build tools and deps).

  if is_release_build && is_release_config; then
    local out_dir="${WORKSPACE_COBALT}/out/${TARGET_PLATFORM}_${CONFIG}"
    local package_dir="${WORKSPACE_COBALT}/package/${PLATFORM}_${CONFIG}"
    mkdir -p "${package_dir}"

    # Create release package
    export PYTHONPATH="${WORKSPACE_COBALT}"

    # IMPORTANT: chromedriver must be built without starboardizations. We ensure
    # that the biary is built with the linux-x64x11-no-starboard config in a
    # previous build step. Then copy the file into this out directory to
    # simulate having built it in-situ (even though that's not possible). This
    # simplifies the execution of the packaging scripts.
    if [[ "${TARGET_PLATFORM}" =~ "linux" ]]; then
      local src_platform="linux-x64x11-no-starboard"
      local src_out="${WORKSPACE_COBALT}/out/${src_platform}_${CONFIG}"
      local dst_out="${WORKSPACE_COBALT}/out/${TARGET_PLATFORM}_${CONFIG}"
      cp "${src_out}/chromedriver" "${dst_out}/chromedriver"
    fi

    # Use config-specific (e.g. package_devel.json) if it exists, otherwise fallback to package.json.
    local json_path="${WORKSPACE_COBALT}/cobalt/build/${PACKAGE_PLATFORM}/package.json"
    local config_json_path="${WORKSPACE_COBALT}/cobalt/build/${PACKAGE_PLATFORM}/package_${CONFIG}.json"
    if [[ -f "${config_json_path}" ]]; then
      json_path="${config_json_path}"
    fi

    # NOTE: Name is required because the Json recipe is not platform and config
    # specific. GCS upload is also done separately because the Json recipe is
    # not branch, date, and build number specific though this can be added.
    python3 "${WORKSPACE_COBALT}/cobalt/build/packager.py" \
      --print_contents \
      --name="${PLATFORM}_${CONFIG}" \
      --json_path="${json_path}" \
      --out_dir="${out_dir}" \
      --package_dir="${package_dir}"

    # Upload release package
    local bucket="cobalt-internal-build-artifacts"
    if [[ "$(get_kokoro_env)" == "qa" ]]; then
      bucket="cobalt-internal-build-artifacts-qa"
    fi
    local gcs_archive_path="gs://${bucket}/${PLATFORM}${GCS_PLATFORM_SUFFIX:-}_${KOKORO_GOB_BRANCH_src}/$(date +%F)/${KOKORO_ROOT_BUILD_NUMBER}/"
    init_gcloud
    # Ensure that only package directory contents are uploaded and not the
    # directory itself.
    "${GSUTIL}" cp -r "${package_dir}/." "${gcs_archive_path}"
  fi
}

should_publish_golden_workspace () {
  if [[ "${PUBLISH_GOLDEN_WORKSPACE:-0}" != "1" ]]; then
    return 1
  fi
  if [[ -n "${KOKORO_ROOT_JOB_TYPE:-}" ]] && ! is_release_build; then
    return 1
  fi
  return 0
}

_do_publish_golden_workspace_snapshot () {
  local gclient_root="$1"
  local platform="$2"
  local bucket="$3"
  local src_commit="$4"

  local staging_dir="${WORKSPACE_COBALT}/out/golden_workspace_staging_$$"
  mkdir -p "${staging_dir}"

  local archive="${staging_dir}/golden-workspace-latest.tar.zst"
  local extra_tar_args=()
  if [[ -d "${gclient_root}/vpython" ]]; then
    extra_tar_args+=("vpython")
  elif [[ -d "${HOME}/.cache/vpython" ]]; then
    extra_tar_args+=("-C" "${HOME}/.cache" "vpython")
  fi

  tar --exclude='src/out' \
      --use-compress-program="zstd -T0 -3" \
      -cf "${archive}" \
      -C "${gclient_root}" src tools/depot_tools .gclient \
      "${extra_tar_args[@]}"

  local archive_sha
  if command -v sha256sum >/dev/null 2>&1; then
    archive_sha=$(sha256sum "${archive}" | awk '{print $1}')
  else
    archive_sha=$(shasum -a 256 "${archive}" | awk '{print $1}')
  fi

  local archive_size
  if stat -c%s "${archive}" >/dev/null 2>&1; then
    archive_size=$(stat -c%s "${archive}")
  else
    archive_size=$(stat -f%z "${archive}")
  fi

  local manifest="${staging_dir}/manifest.json"
  cat > "${manifest}" <<EOF
{
  "schema_version": "1.0",
  "platform": "${platform}",
  "archive_sha256": "${archive_sha}",
  "archive_size_bytes": ${archive_size},
  "src_commit": "${src_commit}",
  "timestamp_utc": "$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
}
EOF

  echo "==> Uploading golden workspace snapshot to gs://${bucket}/${platform}/..."
  "${GSUTIL}" cp "${archive}" "gs://${bucket}/${platform}/golden-workspace-latest.tar.zst"
  "${GSUTIL}" cp "${manifest}" "gs://${bucket}/${platform}/manifest.json"
  echo "==> Successfully uploaded golden workspace snapshot to gs://${bucket}/${platform}/"

  rm -rf "${staging_dir}"
}

publish_golden_workspace_snapshot () {
  if ! should_publish_golden_workspace; then
    echo "==> Skipping golden workspace snapshot (not designated publisher job)."
    return 0
  fi

  local gclient_root="${KOKORO_ARTIFACTS_DIR:-}/git"
  local platform="${TARGET_PLATFORM:-${PLATFORM:-}}"
  if [[ -z "${platform}" ]]; then
    echo "==> Error: Neither TARGET_PLATFORM nor PLATFORM is set. Cannot determine snapshot platform." >&2
    return 1
  fi
  local bucket="${GOLDEN_WORKSPACE_BUCKET:-cobalt-internal-build-artifacts/golden-workspace}"
  local src_commit
  src_commit=$(git -C "${WORKSPACE_COBALT}" rev-parse HEAD)

  init_gcloud
  local manifest_url="gs://${bucket}/${platform}/manifest.json"
  local remote_commit
  remote_commit="$("${GSUTIL}" cat "${manifest_url}" 2>/dev/null | grep -o '"src_commit": *"[^"]*"' | head -n1 | cut -d'"' -f4 || true)"
  if [[ -n "${remote_commit}" && "${remote_commit}" == "${src_commit}" ]]; then
    echo "==> Golden workspace snapshot for ${platform} at commit ${src_commit} is already uploaded. Skipping."
    return 0
  fi

  echo "==> Packaging golden workspace snapshot for ${platform}..."
  if ! _do_publish_golden_workspace_snapshot "${gclient_root}" "${platform}" "${bucket}" "${src_commit}"; then
    echo "==> WARNING: Failed to publish golden workspace snapshot. Continuing build." >&2
    return 0
  fi
}

