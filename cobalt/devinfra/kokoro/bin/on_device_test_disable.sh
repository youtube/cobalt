set -ex

# This script determines if ODT should run or not.
# Exits gracefully (0) if ODT should run, otherwise
# exit 1 which prevents downstream ODT from running.

# src/ maps to cobalt_src/.
readonly WORKSPACE_COBALT="${KOKORO_ARTIFACTS_DIR}/git/src"
readonly ODT_TRAILER="Test-On-Device"

# Load common routines.
. $(dirname "$0")/common.sh

configure_environment

# Check the commit message for the 'Test-On-Device' trailer.
value=$(git -C ${WORKSPACE_COBALT} log --pretty=format:%B -n1 | grep "${ODT_TRAILER}" || true)
if [[ -n "${value}" ]]; then
  exit 0
fi

# Check PRESUBMIT_ON_DEVICE_TEST environment variable.
if [[ "${PRESUBMIT_ON_DEVICE_TEST}" == "false" ]]; then
  exit 1
fi
