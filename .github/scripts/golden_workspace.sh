#!/usr/bin/env bash
#
# Golden workspace snapshots: a fully `gclient sync`ed checkout of the public
# repository at a commit on `main` (plus depot_tools and .gclient), used to
# warm the OverlayFS lower dir on CI runners.
#
#   publish <sha> <root> <platform>   post-submit builds of main: verify the
#                                     freshly synced workspace and upload it
#   hydrate <root> <platform>         runners: download the latest snapshot,
#                                     verify it, leave it extracted in <root>
#
# Snapshots are keyed by profile, i.e. the .gclient variant that
# .github/actions/depot_tools writes for a platform, so DEPS/.gclient match on
# the consumer and its gclient sync can be skipped.
#
# Trust model (best effort): the consumer does not trust the bucket. Before a
# snapshot is used it is verified against github.com to be exactly the tracked
# tree of a commit that is an ancestor of `main`, with dependency checkouts at
# their pinned revisions and no known-sensitive paths. Any failure means the
# runner proceeds without a golden workspace.
#
# Environment:
#   GW_GCS_PREFIX   gs://<bucket>/<path> holding one <profile>/ dir each (required)
#   GW_REPO_URL     public repository URL (default https://github.com/youtube/cobalt)
#   GW_REPO_SLUG    owner/repo for the GitHub API (default youtube/cobalt)
#   GW_MAX_AGE_DAYS maximum snapshot age accepted by the consumer (default 30)
#   GW_FAIL_PREFIX  prefix for failure messages (default "::error::")
#   GITHUB_TOKEN    optional, raises GitHub API rate limits

set -euo pipefail

GW_REPO_URL="${GW_REPO_URL:-https://github.com/youtube/cobalt}"
GW_REPO_SLUG="${GW_REPO_SLUG:-youtube/cobalt}"
GW_MAX_AGE_DAYS="${GW_MAX_AGE_DAYS:-30}"

ARCHIVE_NAME="golden-workspace.tar.zst"
MANIFEST_NAME="manifest.json"
LATEST_NAME="latest.json"
SCHEMA_VERSION="2"

log()  { echo "==> $*" >&2; }
fail() { echo "${GW_FAIL_PREFIX:-::error::}golden_workspace: $*" >&2; exit 1; }

gh_api() {
  local auth=()
  [[ -n "${GITHUB_TOKEN:-}" ]] && auth=(-H "Authorization: Bearer ${GITHUB_TOKEN}")
  curl -sS -f -m 30 -H "Accept: application/vnd.github+json" \
    -H "X-GitHub-Api-Version: 2022-11-28" "${auth[@]}" "https://api.github.com/$1"
}

# Must stay in sync with the "Set target OS" / "Set target CPU" steps in
# .github/actions/depot_tools/action.yaml.
profile_for_platform() {
  local platform="$1" os="" cpu=""
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
  local name="${os}${os:+${cpu:+-}}${cpu}"
  echo "${name:-default}"
}

# ---------------------------------------------------------------------------
# Verification of an extracted workspace <root> (src/, depot_tools/, .gclient)
# ---------------------------------------------------------------------------

verify_commit_on_main() {
  # compare/<sha>...main describes main relative to <sha>: "ahead" or
  # "identical" means <sha> is in main's history.
  local sha="$1" status
  status="$(gh_api "repos/${GW_REPO_SLUG}/compare/${sha}...main" | jq -r '.status')" \
    || fail "GitHub compare API call failed for ${sha}"
  case "${status}" in
    ahead|identical) log "commit ${sha} is on main (${status})" ;;
    *) fail "commit ${sha} is NOT an ancestor of main (status=${status})" ;;
  esac
}

verify_tree() {
  # Tracked content of src/ must be byte-for-byte GitHub's tree for <sha>,
  # with no modifications or untracked files (gitlinks included, which covers
  # the revision of every dependency checkout).
  local sha="$1" src="$2/src" head local_tree remote_tree dirty origin bad_cfg
  git -C "${src}" rev-parse --is-inside-work-tree >/dev/null 2>&1 || fail "${src} is not a git checkout"

  head="$(git -C "${src}" rev-parse HEAD)"
  [[ "${head}" == "${sha}" ]] || fail "HEAD ${head} != expected ${sha}"

  local_tree="$(git -C "${src}" rev-parse 'HEAD^{tree}')"
  remote_tree="$(gh_api "repos/${GW_REPO_SLUG}/git/commits/${sha}" | jq -r '.tree.sha')" \
    || fail "GitHub commits API call failed for ${sha}"
  [[ "${local_tree}" == "${remote_tree}" ]] \
    || fail "tree ${local_tree} != GitHub tree ${remote_tree} for ${sha}"

  git -C "${src}" update-index --refresh -q >/dev/null 2>&1 || true
  dirty="$(git -C "${src}" -c core.quotePath=false status --porcelain --untracked-files=all | head -n 20)"
  [[ -z "${dirty}" ]] || fail "src/ is not clean:"$'\n'"${dirty}"

  origin="$(git -C "${src}" remote get-url origin)"
  case "${origin}" in
    "${GW_REPO_URL}"|"${GW_REPO_URL}.git") ;;
    *) fail "unexpected origin URL: ${origin}" ;;
  esac

  bad_cfg="$(git -C "${src}" config --local --list \
    | grep -Ei '^(http\.[^=]*cookiefile|credential\.|url\.[^=]*insteadof|http\.[^=]*extraheader)' || true)"
  [[ -z "${bad_cfg}" ]] || fail "disallowed git config in src/.git/config:"$'\n'"${bad_cfg}"

  git -C "${src}" fsck --connectivity-only --no-dangling --no-progress \
    || fail "git fsck --connectivity-only failed"
  log "src/ tree matches GitHub for ${sha}"
}

verify_deps() {
  # Every populated dependency checkout must be at the commit its gitlink
  # records (verify_tree proved the gitlinks equal GitHub's tree).
  local src="$1/src" checked=0 bad="" sha path actual
  while IFS=$'\t' read -r sha path; do
    [[ -e "${src}/${path}/.git" ]] || continue
    actual="$(git -C "${src}/${path}" rev-parse HEAD 2>/dev/null || echo missing)"
    [[ "${actual}" == "${sha}" ]] || bad+="${path}: ${actual} != ${sha}"$'\n'
    checked=$((checked + 1))
  done < <(git -C "${src}" -c core.quotePath=false ls-files -s \
             | awk -F'\t' '$1 ~ /^160000 / { split($1, a, " "); print a[2] "\t" $2 }')
  [[ -z "${bad}" ]] || fail "dependency checkouts differ from pinned gitlinks:"$'\n'"$(echo "${bad}" | head -n 20)"
  (( checked > 0 )) || fail "no dependency checkouts found; is this a synced workspace?"
  log "${checked} dependency checkouts match pinned revisions"
}

verify_policy() {
  # Paths that must never appear in a public snapshot.
  local root="$1" hits urls
  hits="$(cd "${root}" && find src -path src/out -prune -o \( \
      -path 'src/starboard/keyboxes*' -o -path 'src/internal/*' -o \
      -path 'src/cobalt/internal/*' -o -path 'src/third_party/internal/*' -o \
      -name '*.secret' -o -name 'DO.NOT.RELEASE*' \) -print 2>/dev/null | head -n 20)"
  [[ -z "${hits}" ]] || fail "disallowed paths present:"$'\n'"${hits}"
  urls="$(grep -IlE 'rpc://|lbshell-internal|googlesource\.com/a/' \
    "${root}/.gclient" "${root}/src/.git/config" 2>/dev/null || true)"
  [[ -z "${urls}" ]] || fail "internal URLs present in: ${urls}"
  log "policy scan clean"
}

verify_workspace() {
  local sha="$1" root="$2"
  verify_commit_on_main "${sha}"
  verify_tree "${sha}" "${root}"
  verify_policy "${root}"
  verify_deps "${root}"
}

# ---------------------------------------------------------------------------
# publish: called from .github/actions/depot_tools right after gclient sync
# on post-submit builds of main.
# ---------------------------------------------------------------------------

cmd_publish() {
  local sha="$1" root="$2" platform="$3"
  [[ -n "${GW_GCS_PREFIX:-}" ]] || fail "GW_GCS_PREFIX is not set"
  [[ "${sha}" =~ ^[0-9a-f]{40}$ ]] || fail "expected a full commit sha, got ${sha}"
  local profile prefix dest staging archive
  profile="$(profile_for_platform "${platform}")"
  [[ "${profile}" != "unsupported" ]] || { log "no profile for ${platform}; skipping"; return 0; }
  prefix="${GW_GCS_PREFIX}/${profile}"
  dest="${prefix}/${sha}"

  if gcloud storage objects describe "${dest}/${MANIFEST_NAME}" >/dev/null 2>&1; then
    log "snapshot ${profile}@${sha} already published; skipping"
    return 0
  fi

  verify_workspace "${sha}" "${root}"

  staging="$(mktemp -d)"
  trap 'rm -rf "${staging}"' EXIT
  archive="${staging}/${ARCHIVE_NAME}"
  log "packaging ${profile}@${sha}"
  tar --exclude='src/out' -C "${root}" -cf - src depot_tools .gclient \
    | zstd -T0 -3 -q -o "${archive}"
  jq -n --arg schema "${SCHEMA_VERSION}" --arg sha "${sha}" --arg profile "${profile}" \
    --arg digest "$(sha256sum "${archive}" | awk '{print $1}')" \
    --argjson size "$(stat -c%s "${archive}")" \
    --arg ts "$(date -u +'%Y-%m-%dT%H:%M:%SZ')" \
    '{schema_version: $schema, src_commit: $sha, profile: $profile,
      archive_sha256: $digest, archive_size_bytes: $size, timestamp_utc: $ts}' \
    > "${staging}/${MANIFEST_NAME}"
  jq -n --arg sha "${sha}" '{src_commit: $sha}' > "${staging}/${LATEST_NAME}"

  log "uploading to ${dest}/"
  # Per-commit objects are immutable; latest.json is a pointer the consumer
  # re-validates.
  gcloud storage cp --no-clobber "${archive}" "${dest}/${ARCHIVE_NAME}"
  gcloud storage cp --no-clobber "${staging}/${MANIFEST_NAME}" "${dest}/${MANIFEST_NAME}"
  gcloud storage cp "${staging}/${LATEST_NAME}" "${prefix}/${LATEST_NAME}"
  log "published ${profile}@${sha}"
}

# ---------------------------------------------------------------------------
# hydrate: called from .github/actions/overlay_workspace on runners without a
# golden workspace. Leaves a verified workspace in <root> or fails.
# ---------------------------------------------------------------------------

cmd_hydrate() {
  local root="$1" platform="$2"
  [[ -n "${GW_GCS_PREFIX:-}" ]] || fail "GW_GCS_PREFIX is not set"
  local profile prefix tmp sha dest ts age_s
  profile="$(profile_for_platform "${platform}")"
  [[ "${profile}" != "unsupported" ]] || fail "no profile for platform '${platform}'"
  prefix="${GW_GCS_PREFIX}/${profile}"
  tmp="$(mktemp -d)"
  trap 'rm -rf "${tmp}"' EXIT

  gcloud storage cp -q "${prefix}/${LATEST_NAME}" "${tmp}/${LATEST_NAME}" \
    || fail "no ${LATEST_NAME} at ${prefix}"
  sha="$(jq -r '.src_commit' "${tmp}/${LATEST_NAME}")"
  [[ "${sha}" =~ ^[0-9a-f]{40}$ ]] || fail "latest.json has no valid src_commit"
  dest="${prefix}/${sha}"

  gcloud storage cp -q "${dest}/${MANIFEST_NAME}" "${tmp}/${MANIFEST_NAME}" \
    || fail "missing manifest for ${sha}"
  [[ "$(jq -r '.schema_version' "${tmp}/${MANIFEST_NAME}")" == "${SCHEMA_VERSION}" ]] \
    || fail "unsupported manifest schema"
  [[ "$(jq -r '.src_commit' "${tmp}/${MANIFEST_NAME}")" == "${sha}" ]] || fail "manifest sha mismatch"
  [[ "$(jq -r '.profile' "${tmp}/${MANIFEST_NAME}")" == "${profile}" ]] || fail "manifest profile mismatch"
  ts="$(jq -r '.timestamp_utc' "${tmp}/${MANIFEST_NAME}")"
  age_s=$(( $(date -u +%s) - $(date -u -d "${ts}" +%s) ))
  (( age_s >= 0 && age_s <= GW_MAX_AGE_DAYS * 86400 )) \
    || fail "snapshot timestamp ${ts} outside accepted window (${GW_MAX_AGE_DAYS}d)"

  # Cheap check before the multi-GB download.
  verify_commit_on_main "${sha}"

  log "downloading ${dest}/${ARCHIVE_NAME}"
  gcloud storage cp -q "${dest}/${ARCHIVE_NAME}" "${tmp}/${ARCHIVE_NAME}" || fail "archive download failed"
  [[ "$(stat -c%s "${tmp}/${ARCHIVE_NAME}")" == "$(jq -r '.archive_size_bytes' "${tmp}/${MANIFEST_NAME}")" ]] \
    || fail "archive size mismatch"
  [[ "$(sha256sum "${tmp}/${ARCHIVE_NAME}" | awk '{print $1}')" == "$(jq -r '.archive_sha256' "${tmp}/${MANIFEST_NAME}")" ]] \
    || fail "archive sha256 mismatch"

  mkdir -p "${root}"
  log "extracting into ${root}"
  zstd -dc -T0 "${tmp}/${ARCHIVE_NAME}" | tar --no-same-owner -xf - -C "${root}"
  [[ -e "${root}/src/.git" && -d "${root}/depot_tools" && -f "${root}/.gclient" ]] \
    || fail "archive layout unexpected"

  verify_workspace "${sha}" "${root}"
  log "golden workspace ${profile}@${sha} verified"
}

case "${1:-}" in
  publish) [[ $# -eq 4 ]] || fail "usage: publish <sha> <root> <platform>"; cmd_publish "$2" "$3" "$4" ;;
  hydrate) [[ $# -eq 3 ]] || fail "usage: hydrate <root> <platform>";       cmd_hydrate "$2" "$3" ;;
  verify)  [[ $# -eq 3 ]] || fail "usage: verify <sha> <root>";             verify_workspace "$2" "$3" ;;
  *) fail "unknown command '${1:-}' (publish|hydrate|verify)" ;;
esac
