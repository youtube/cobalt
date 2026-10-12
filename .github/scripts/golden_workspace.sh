#!/usr/bin/env bash
#
# Golden workspace snapshot tooling shared by the GitHub Actions producer
# (.github/workflows/golden_workspace.yaml) and consumer
# (.github/actions/overlay_workspace/action.yaml).
#
# A "golden workspace" is a fully `gclient sync`ed checkout of the public
# repository at a commit on `main`, packaged as a tar.zst together with
# depot_tools and the .gclient file. It is used to warm the OverlayFS lower
# dir on CI runners so that PR jobs do not pay for a full sync.
#
# Snapshots are produced per "profile": the .gclient variant that
# .github/actions/depot_tools would write for a given platform. The consumer
# resolves the same profile for its platform so that DEPS/.gclient match and
# the follow-up gclient sync can be skipped.
#
# Trust model (best effort):
#   * The producer runs only from `main` and never touches node-local caches.
#   * The consumer does not trust the producer. Before using a snapshot it
#     verifies, against github.com itself, that the snapshot is exactly the
#     tracked tree of a commit that is an ancestor of `main`, that dependency
#     checkouts are at their pinned revisions, and that no known-sensitive
#     paths are present. On any failure the consumer falls back to a cold
#     workspace.
#
# Usage:
#   golden_workspace.sh profile <platform>              print profile name
#   golden_workspace.sh produce <sha> <root> <profile>  clone+sync+verify+package
#   golden_workspace.sh upload  <sha> <root> <profile>  upload archive+manifest
#   golden_workspace.sh fetch   <root> <profile>        download+validate+extract latest
#   golden_workspace.sh verify  <sha> <root>            run consumer checks on <root>
#
# Environment:
#   GW_GCS_PREFIX   gs://<bucket>/golden-workspace (required for upload/fetch)
#   GW_REPO_URL     public repository URL (default https://github.com/youtube/cobalt)
#   GW_REPO_SLUG    owner/repo for the GitHub API (default youtube/cobalt)
#   GW_BASE_BRANCH  branch the snapshot commit must be an ancestor of (default main)
#   GW_MAX_AGE_DAYS maximum manifest age accepted by the consumer (default 7)
#   GW_DEEP_VERIFY  "true" to run a full `git fsck` (slow) instead of connectivity-only
#   GW_FAIL_PREFIX  prefix for failure messages (default "::error::")
#   GW_RBE_INSTANCE rbe_instance custom var (must match .github/actions/depot_tools)
#   GITHUB_TOKEN    optional, raises GitHub API rate limits
#   DEPOT_TOOLS_REF pinned depot_tools commit

set -euo pipefail

GW_REPO_URL="${GW_REPO_URL:-https://github.com/youtube/cobalt}"
GW_REPO_SLUG="${GW_REPO_SLUG:-youtube/cobalt}"
GW_BASE_BRANCH="${GW_BASE_BRANCH:-main}"
GW_MAX_AGE_DAYS="${GW_MAX_AGE_DAYS:-7}"
GW_DEEP_VERIFY="${GW_DEEP_VERIFY:-false}"
GW_RBE_INSTANCE="${GW_RBE_INSTANCE:-projects/cobalt-actions-prod/instances/default_instance}"
# TODO(b/562551706): Pinned before upstream 20aff01e (2026-09-16), which added
# `--end-of-options` to `git checkout`. Need to update git on runners.
DEPOT_TOOLS_REF="${DEPOT_TOOLS_REF:-4a978d8f1f3567d5bd729aec018bfc345a14e1cd}"
DEPOT_TOOLS_URL="https://chromium.googlesource.com/chromium/tools/depot_tools.git"

ARCHIVE_NAME="golden-workspace.tar.zst"
MANIFEST_NAME="manifest.json"
LATEST_NAME="latest.json"
SCHEMA_VERSION="2"

log()  { echo "==> $*" >&2; }
# GW_FAIL_PREFIX lets the consumer downgrade failures to warnings, since it
# falls back to a cold workspace instead of failing the job.
fail() { echo "${GW_FAIL_PREFIX:-::error::}golden_workspace: $*" >&2; exit 1; }

sha256_of() {
  sha256sum "$1" | awk '{print $1}'
}

gh_api() {
  # Thin wrapper around the GitHub REST API. Unauthenticated calls are rate
  # limited per IP, so pass GITHUB_TOKEN whenever available.
  local url="https://api.github.com/$1"
  local auth=()
  if [[ -n "${GITHUB_TOKEN:-}" ]]; then
    auth=(-H "Authorization: Bearer ${GITHUB_TOKEN}")
  fi
  curl -sS -f -m 30 \
    -H "Accept: application/vnd.github+json" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    "${auth[@]}" "${url}"
}

require_tools() {
  local t
  for t in git curl jq tar zstd sha256sum; do
    command -v "$t" >/dev/null 2>&1 || fail "missing required tool: $t"
  done
}

# ---------------------------------------------------------------------------
# Profiles. MUST stay in sync with the "Set target OS" / "Set target CPU"
# steps in .github/actions/depot_tools/action.yaml, which is what the
# consumer's .gclient is compared against byte-for-byte.
# ---------------------------------------------------------------------------

profile_for_platform() {
  local platform="$1"
  local os="" cpu=""
  if [[ "${platform}" == *"android"* || "${platform}" == *"aosp"* ]]; then
    os="android"
  elif [[ "${platform}" == *"tvos"* ]]; then
    echo "unsupported"
    return 0
  fi
  if [[ "${platform}" == *"evergreen-arm64"* ]]; then
    cpu="evergreen-arm64"
  elif [[ "${platform}" == *"evergreen-arm"* ]]; then
    cpu="evergreen-arm"
  fi
  local name="${os:+${os}}${os:+${cpu:+-}}${cpu}"
  echo "${name:-default}"
}

write_gclient() {
  # Reproduces the .gclient that .github/actions/depot_tools writes.
  local root="$1" profile="$2"
  (
    cd "${root}"
    rm -f .gclient
    gclient config --name=src \
      --custom-var="rbe_instance=\"${GW_RBE_INSTANCE}\"" \
      "${GW_REPO_URL}"
    case "${profile}" in
      android|android-*) echo "target_os=['android']" >> .gclient ;;
    esac
    case "${profile}" in
      *evergreen-arm64) echo "target_cpu=['x64', 'arm64']" >> .gclient ;;
      *evergreen-arm)   echo "target_cpu=['x64', 'arm']" >> .gclient ;;
    esac
    case "${profile}" in
      default|android|evergreen-arm64|evergreen-arm|android-evergreen-arm64|android-evergreen-arm) ;;
      *) fail "unknown profile '${profile}'" ;;
    esac
  )
}

# ---------------------------------------------------------------------------
# Consumer-side checks. Each takes <sha> <root> where <root> contains
# src/, depot_tools/ and .gclient.
# ---------------------------------------------------------------------------

verify_commit_on_base_branch() {
  # Proves, using GitHub as the oracle, that <sha> is an ancestor of
  # GW_BASE_BRANCH. compare/<sha>...<branch> reports the branch relative to
  # <sha>: "ahead" or "identical" means <sha> is in the branch history.
  local sha="$1"
  local status
  status="$(gh_api "repos/${GW_REPO_SLUG}/compare/${sha}...${GW_BASE_BRANCH}" | jq -r '.status')" \
    || fail "GitHub compare API call failed for ${sha}"
  case "${status}" in
    ahead|identical) log "commit ${sha} is on ${GW_BASE_BRANCH} (${status})" ;;
    *) fail "commit ${sha} is NOT an ancestor of ${GW_BASE_BRANCH} (status=${status})" ;;
  esac
}

verify_tree() {
  # The tracked content of src/ must be byte-for-byte the tree GitHub has for
  # <sha>, with no modifications or untracked files.
  local sha="$1" root="$2"
  local src="${root}/src"
  [[ -d "${src}/.git" ]] || fail "${src} is not a git checkout"

  local head
  head="$(git -C "${src}" rev-parse HEAD)"
  [[ "${head}" == "${sha}" ]] || fail "HEAD ${head} != expected ${sha}"

  local local_tree remote_tree
  local_tree="$(git -C "${src}" rev-parse 'HEAD^{tree}')"
  remote_tree="$(gh_api "repos/${GW_REPO_SLUG}/git/commits/${sha}" | jq -r '.tree.sha')" \
    || fail "GitHub commits API call failed for ${sha}"
  [[ "${local_tree}" == "${remote_tree}" ]] \
    || fail "tree ${local_tree} != GitHub tree ${remote_tree} for ${sha}"

  # Refresh stat info first so files restored from a tarball are not rehashed
  # one by one, then require a fully clean tree (tracked, untracked and
  # gitlinks for dependency checkouts).
  git -C "${src}" update-index --refresh -q >/dev/null 2>&1 || true
  local dirty
  dirty="$(git -C "${src}" -c core.quotePath=false status --porcelain --untracked-files=all | head -n 20)"
  [[ -z "${dirty}" ]] || fail "src/ is not clean:"$'\n'"${dirty}"

  local origin
  origin="$(git -C "${src}" remote get-url origin)"
  case "${origin}" in
    "${GW_REPO_URL}"|"${GW_REPO_URL}.git") ;;
    *) fail "unexpected origin URL: ${origin}" ;;
  esac

  # No credential material or URL rewriting may be baked into the snapshot.
  local bad_cfg
  bad_cfg="$(git -C "${src}" config --local --list \
    | grep -Ei '^(http\.[^=]*cookiefile|credential\.|url\.[^=]*insteadof|http\.[^=]*extraheader)' || true)"
  [[ -z "${bad_cfg}" ]] || fail "disallowed git config in src/.git/config:"$'\n'"${bad_cfg}"

  if [[ "${GW_DEEP_VERIFY}" == "true" ]]; then
    log "running full git fsck (deep verify)"
    git -C "${src}" fsck --no-dangling --no-progress || fail "git fsck failed"
  else
    git -C "${src}" fsck --connectivity-only --no-dangling --no-progress \
      || fail "git fsck --connectivity-only failed"
  fi
  log "src/ tree matches GitHub for ${sha}"
}

verify_deps() {
  # Git DEPS entries are mirrored as gitlinks in the src index, and
  # verify_tree already proved that index equals GitHub's tree for <sha>.
  # Here we check that every populated dependency checkout is at exactly the
  # commit recorded by that gitlink. Dependencies that are not present
  # (platform/condition not selected) are skipped. CIPD packages are not
  # covered here.
  local root="$1"
  local src="${root}/src"
  local checked=0 bad=""
  local sha path
  while IFS=$'\t' read -r sha path; do
    [[ -e "${src}/${path}/.git" ]] || continue
    local actual
    actual="$(git -C "${src}/${path}" rev-parse HEAD 2>/dev/null || echo missing)"
    if [[ "${actual}" != "${sha}" ]]; then
      bad+="${path}: ${actual} != ${sha}"$'\n'
    fi
    checked=$((checked + 1))
  done < <(git -C "${src}" -c core.quotePath=false ls-files -s \
             | awk -F'\t' '$1 ~ /^160000 / { split($1, a, " "); print a[2] "\t" $2 }')
  [[ -z "${bad}" ]] || fail "dependency checkouts differ from pinned gitlinks:"$'\n'"$(echo "${bad}" | head -n 20)"
  (( checked > 0 )) || fail "no dependency checkouts found; is this a synced workspace?"
  log "${checked} dependency checkouts match pinned revisions"
}

verify_policy() {
  # Defence in depth: paths that must never appear in a public snapshot.
  local root="$1"
  local hits
  hits="$(cd "${root}" && find src -path src/out -prune -o \( \
      -path 'src/starboard/keyboxes*' -o \
      -path 'src/internal/*' -o \
      -path 'src/cobalt/internal/*' -o \
      -path 'src/third_party/internal/*' -o \
      -name '*.secret' -o \
      -name 'DO.NOT.RELEASE*' \
    \) -print 2>/dev/null | head -n 20)"
  [[ -z "${hits}" ]] || fail "disallowed paths present:"$'\n'"${hits}"

  local urls
  urls="$(grep -IlE 'rpc://|lbshell-internal|googlesource\.com/a/' \
    "${root}/.gclient" "${root}/src/.git/config" 2>/dev/null || true)"
  [[ -z "${urls}" ]] || fail "internal URLs present in: ${urls}"
  log "policy scan clean"
}

verify_manifest_file() {
  # Structural checks on manifest.json; returns the src_commit on stdout.
  local manifest="$1"
  jq -e . "${manifest}" >/dev/null || fail "manifest is not valid JSON"
  local schema sha ts
  schema="$(jq -r '.schema_version' "${manifest}")"
  [[ "${schema}" == "${SCHEMA_VERSION}" ]] || fail "unsupported manifest schema ${schema}"
  sha="$(jq -r '.src_commit' "${manifest}")"
  [[ "${sha}" =~ ^[0-9a-f]{40}$ ]] || fail "manifest src_commit is not a sha: ${sha}"
  ts="$(jq -r '.timestamp_utc' "${manifest}")"
  local age_s
  age_s=$(( $(date -u +%s) - $(date -u -d "${ts}" +%s) ))
  (( age_s >= 0 && age_s <= GW_MAX_AGE_DAYS * 86400 )) \
    || fail "manifest timestamp ${ts} outside accepted window (${GW_MAX_AGE_DAYS}d)"
  echo "${sha}"
}

cmd_verify() {
  local sha="$1" root="$2"
  require_tools
  verify_commit_on_base_branch "${sha}"
  verify_tree "${sha}" "${root}"
  verify_policy "${root}"
  verify_deps "${root}"
  log "all checks passed for ${sha}"
}

# ---------------------------------------------------------------------------
# Producer
# ---------------------------------------------------------------------------

cmd_produce() {
  local sha="$1" root="$2" profile="$3"
  require_tools
  [[ "${sha}" =~ ^[0-9a-f]{40}$ ]] || fail "expected a full commit sha, got ${sha}"
  rm -rf "${root}"
  mkdir -p "${root}"

  log "cloning ${GW_REPO_URL} into ${root}/src"
  git clone --filter=blob:none --no-checkout "${GW_REPO_URL}" "${root}/src"
  git -C "${root}/src" checkout --quiet "${sha}"

  log "cloning depot_tools @ ${DEPOT_TOOLS_REF}"
  git clone --quiet "${DEPOT_TOOLS_URL}" "${root}/depot_tools"
  git -C "${root}/depot_tools" checkout --quiet "${DEPOT_TOOLS_REF}"

  export DEPOT_TOOLS_UPDATE=0
  export PATH="${root}/depot_tools:${PATH}"
  write_gclient "${root}" "${profile}"
  cat "${root}/.gclient" >&2
  (
    cd "${root}"
    # Mirrors the sync flags in .github/actions/depot_tools.
    gclient sync \
      --shallow \
      --no-history \
      --nohooks \
      --force \
      --delete_unversioned_trees \
      --revision "src@${sha}"
  )

  # Never ship anything we would not accept on the consumer side.
  cmd_verify "${sha}" "${root}"

  local staging="${root}.staging"
  rm -rf "${staging}"
  mkdir -p "${staging}"
  log "packaging ${staging}/${ARCHIVE_NAME}"
  tar --exclude='src/out' -C "${root}" -cf - src depot_tools .gclient \
    | zstd -T0 -3 -q -o "${staging}/${ARCHIVE_NAME}"

  local archive="${staging}/${ARCHIVE_NAME}"
  jq -n \
    --arg schema "${SCHEMA_VERSION}" \
    --arg sha "${sha}" \
    --arg profile "${profile}" \
    --arg tree "$(git -C "${root}/src" rev-parse 'HEAD^{tree}')" \
    --arg digest "$(sha256_of "${archive}")" \
    --argjson size "$(stat -c%s "${archive}")" \
    --arg ts "$(date -u +'%Y-%m-%dT%H:%M:%SZ')" \
    --arg repo "${GITHUB_REPOSITORY:-}" \
    --arg wf "${GITHUB_WORKFLOW_REF:-}" \
    --arg run "${GITHUB_RUN_ID:-}" \
    --arg attempt "${GITHUB_RUN_ATTEMPT:-}" \
    '{schema_version: $schema, src_commit: $sha, profile: $profile, tree_sha: $tree,
      archive_sha256: $digest, archive_size_bytes: $size, timestamp_utc: $ts,
      producer: {repository: $repo, workflow_ref: $wf, run_id: $run, run_attempt: $attempt}}' \
    > "${staging}/${MANIFEST_NAME}"
  cat "${staging}/${MANIFEST_NAME}" >&2
}

cmd_upload() {
  local sha="$1" root="$2" profile="$3"
  [[ -n "${GW_GCS_PREFIX:-}" ]] || fail "GW_GCS_PREFIX is not set"
  local staging="${root}.staging"
  [[ -f "${staging}/${ARCHIVE_NAME}" && -f "${staging}/${MANIFEST_NAME}" ]] \
    || fail "nothing to upload in ${staging}; run 'produce' first"
  [[ "$(jq -r .src_commit "${staging}/${MANIFEST_NAME}")" == "${sha}" ]] \
    || fail "manifest sha mismatch"
  [[ "$(jq -r .profile "${staging}/${MANIFEST_NAME}")" == "${profile}" ]] \
    || fail "manifest profile mismatch"

  local prefix="${GW_GCS_PREFIX}/${profile}"
  local dest="${prefix}/${sha}"
  log "uploading to ${dest}/"
  # Per-commit objects are immutable: never overwrite.
  gcloud storage cp --no-clobber "${staging}/${ARCHIVE_NAME}" "${dest}/${ARCHIVE_NAME}"
  gcloud storage cp --no-clobber "${staging}/${MANIFEST_NAME}" "${dest}/${MANIFEST_NAME}"
  # latest.json is a mutable pointer; the consumer re-validates its target.
  jq -n --arg sha "${sha}" --arg ts "$(date -u +'%Y-%m-%dT%H:%M:%SZ')" \
    '{src_commit: $sha, updated_utc: $ts}' > "${staging}/${LATEST_NAME}"
  gcloud storage cp "${staging}/${LATEST_NAME}" "${prefix}/${LATEST_NAME}"
  log "published ${profile}@${sha}"
}

# ---------------------------------------------------------------------------
# Consumer download. Resolves latest.json -> manifest -> archive, validates
# the manifest and archive digest, and extracts into <root>. Prints the sha.
# Tree/DEPS/policy checks are done separately with `verify`.
# ---------------------------------------------------------------------------

cmd_fetch() {
  local root="$1" profile="$2"
  [[ -n "${GW_GCS_PREFIX:-}" ]] || fail "GW_GCS_PREFIX is not set"
  require_tools
  local prefix="${GW_GCS_PREFIX}/${profile}"
  local tmp
  tmp="$(mktemp -d)"
  trap 'rm -rf "${tmp}"' RETURN

  gcloud storage cp -q "${prefix}/${LATEST_NAME}" "${tmp}/${LATEST_NAME}" \
    || fail "no ${LATEST_NAME} at ${prefix}"
  local sha
  sha="$(jq -r '.src_commit' "${tmp}/${LATEST_NAME}")"
  [[ "${sha}" =~ ^[0-9a-f]{40}$ ]] || fail "latest.json has no valid src_commit"

  local dest="${prefix}/${sha}"
  gcloud storage cp -q "${dest}/${MANIFEST_NAME}" "${tmp}/${MANIFEST_NAME}" \
    || fail "missing manifest for ${sha}"
  local manifest_sha
  manifest_sha="$(verify_manifest_file "${tmp}/${MANIFEST_NAME}")"
  [[ "${manifest_sha}" == "${sha}" ]] || fail "manifest/latest sha mismatch"
  [[ "$(jq -r '.profile' "${tmp}/${MANIFEST_NAME}")" == "${profile}" ]] \
    || fail "manifest profile does not match ${profile}"

  # Check the commit before spending time on a multi-GB download.
  verify_commit_on_base_branch "${sha}"

  log "downloading ${dest}/${ARCHIVE_NAME}"
  gcloud storage cp -q "${dest}/${ARCHIVE_NAME}" "${tmp}/${ARCHIVE_NAME}" \
    || fail "archive download failed"
  local want_digest want_size
  want_digest="$(jq -r '.archive_sha256' "${tmp}/${MANIFEST_NAME}")"
  want_size="$(jq -r '.archive_size_bytes' "${tmp}/${MANIFEST_NAME}")"
  [[ "$(stat -c%s "${tmp}/${ARCHIVE_NAME}")" == "${want_size}" ]] || fail "archive size mismatch"
  [[ "$(sha256_of "${tmp}/${ARCHIVE_NAME}")" == "${want_digest}" ]] || fail "archive sha256 mismatch"

  rm -rf "${root}"
  mkdir -p "${root}"
  log "extracting into ${root}"
  zstd -dc -T0 "${tmp}/${ARCHIVE_NAME}" | tar --no-same-owner -xf - -C "${root}"
  [[ -d "${root}/src/.git" && -d "${root}/depot_tools" && -f "${root}/.gclient" ]] \
    || fail "archive layout unexpected"
  cp "${tmp}/${MANIFEST_NAME}" "${root}/.golden_manifest.json"
  echo "${sha}"
}

main() {
  local cmd="${1:-}"
  shift || true
  case "${cmd}" in
    profile) [[ $# -eq 1 ]] || fail "usage: profile <platform>";              profile_for_platform "$@" ;;
    produce) [[ $# -eq 3 ]] || fail "usage: produce <sha> <root> <profile>";  cmd_produce "$@" ;;
    upload)  [[ $# -eq 3 ]] || fail "usage: upload <sha> <root> <profile>";   cmd_upload "$@" ;;
    fetch)   [[ $# -eq 2 ]] || fail "usage: fetch <root> <profile>";          cmd_fetch "$@" ;;
    verify)  [[ $# -eq 2 ]] || fail "usage: verify <sha> <root>";             cmd_verify "$@" ;;
    *) fail "unknown command '${cmd}' (profile|produce|upload|fetch|verify)" ;;
  esac
}

main "$@"
