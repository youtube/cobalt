#!/bin/bash
#
# This is the entrypoint script from the Kokoro Instance which runs the Generic
# DinD Image. It does the following:
#   * Configures the environment
#   * Invokes the dockerized build via python script
#   * Runs the packaging scripts if applicable
#
# Kokoro Instance
# └── Generic DinD Image
#     ├── dind_builder_runner.sh
#     │   ├── main_build_image_and_run.py
#     │   │   └── Specific Cobalt Image
#     │   │       └── dind_build.sh
#     │   └── run_package_release_pipeline (common.sh)
#     └── dind_runner.sh                                    <== THIS SCRIPT
#         ├── main_pull_image_and_run.py
#         │   └── Specific Cobalt Image
#         │       └── dind_build.sh
#         └── run_package_release_pipeline (common.sh)
set -ueEx

# Load common routines.
. $(dirname "$0")/common.sh

# Using repository root as work directory.
export WORKSPACE_COBALT="${KOKORO_ARTIFACTS_DIR}/git/src"
cd "${WORKSPACE_COBALT}"

# Clean up workspace on exit or error.
trap "bash ${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/bin/cleanup.sh" EXIT INT TERM

configure_dind_environment

set -x
# The python script is responsible for running containerized Cobalt builds.
python3 "${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/bin/dind_py/main_pull_image_and_run.py"

# The following script is responsible for creating packages and nightly builds
# which do not need to run inside Cobalt containers.
# If this build is not a release or nightly, then it will be a no-op.
run_package_release_pipeline
