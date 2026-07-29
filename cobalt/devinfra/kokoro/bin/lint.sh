#!/bin/bash
# Continue on errors.
set -ux

. $(dirname "$0")/common.sh

WORKSPACE_COBALT="${KOKORO_ARTIFACTS_DIR}/git/src"

### Clean up workspace on exit or error.
trap "bash ${WORKSPACE_COBALT}/cobalt/devinfra/kokoro/bin/cleanup.sh" EXIT INT TERM

configure_environment

# Install GN.
curl --location --silent --output /tmp/gn.zip "https://chrome-infra-packages.appspot.com/dl/gn/gn/linux-amd64/+/${GN_HASH}"
echo ${GN_SHA256SUM} | sha256sum --check
unzip /tmp/gn.zip -d /usr/local/bin
rm /tmp/gn.zip

# Install cobalt pip dependencies.
# pip install -r ${WORKSPACE_COBALT}/docker/precommit_hooks/requirements.txt

# Install and run pre-commit checks
pushd ${WORKSPACE_COBALT}
# pre-commit install -t pre-commit -t pre-push
# pre-commit run --from-ref ${STYLE_CHECK_START} --to-ref HEAD --hook-stage commit
# pre-commit run --from-ref ${STYLE_CHECK_START} --to-ref HEAD --hook-stage push

echo "Not running linter, see b/311566658"

# Print status and any diffs that resulted from running pre-commit
git status
git diff
popd
