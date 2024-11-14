set -x

REPO_ROOT="$(dirname "$0")/../../.."
# After kokoro build finishes, changed files in c:/tmpfs get rsync'ed to a borg executor,
# from where the artifacts and reports are processed.
# Especially on Windows, the rsync can take long time, so we cleanup the cobalt workspace
# after finishing each build.
set +e
pushd $REPO_ROOT
time git clean -fdx
popd
exit 0
