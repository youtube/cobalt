#!/bin/bash
set -uex

# This script runs the Mobile Harness test trigger.

WORKSPACE_COBALT="${KOKORO_ARTIFACTS_DIR}/git/src"

. $(dirname "$0")/common.sh
configure_environment

# Tests are always run on the 'devel' config.
CONFIG=devel

# Get GCS path from build job artifact.
GCS_ARCHIVE_PATH="$(<${KOKORO_GFILE_DIR}/gcs_archive_path)"

# The path is determined by the name of the blaze action in the build config
# followed by the path to the target in blaze-bin.
BLAZE_PATH="${KOKORO_BLAZE_DIR}/mh_trigger/blaze-bin/video/youtube/testing/maneki/cobalt/mh_client"

TEST_ATTEMPTS=1
if is_release_build; then
  # Increase test attempts for nightly jobs.
  TEST_ATTEMPTS=3
fi

"${BLAZE_PATH}/trigger_tests.par" \
  --enforce_kernel_ipv6_support=false --gid= --uid= \
  --platform=${PLATFORM} \
  --config=${CONFIG} \
  --branch=${KOKORO_GOB_BRANCH} \
  --remote_archive_path=${GCS_ARCHIVE_PATH} \
  --test_type=${TEST_TYPE} \
  --test_attempts=${TEST_ATTEMPTS} \
  --unittest_shard_index=${TEST_SHARD}
