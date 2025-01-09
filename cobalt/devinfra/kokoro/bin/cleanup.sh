set -x

# After kokoro build finishes, changed files in c:/tmpfs get rsync'ed to a borg executor,
# from where the artifacts and reports are processed.
# Especially on Windows, the rsync can take long time, so we cleanup the cobalt workspace
# after finishing each build.
set +e
git config --global --add safe.directory "${WORKSPACE_COBALT}"
time git -C "${WORKSPACE_COBALT}" clean -dfx
exit 0
