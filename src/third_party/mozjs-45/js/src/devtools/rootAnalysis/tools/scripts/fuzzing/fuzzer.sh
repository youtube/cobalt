#!/bin/bash
set -e
eval `ssh-agent`
ssh-add ~/.ssh/ffxbld_rsa
trap "ssh-agent -k" EXIT

SCRIPTS_DIR="$(dirname $0)/../.."
VCS="git"

# Call the Python 2.7 package in Win64 machines.
if [ "$OS" = "Windows_NT" ] && [ -e "/c/mozilla-build/python27/python.exe" ]; then
    PYBIN="/c/mozilla-build/python27/python.exe"
    GITBIN="/c/mozilla-build/Git/bin/git.exe"
elif [ "$TERM" = "linux" ] && [ -e "/usr/local/bin/python2.7" ]; then
    PYBIN="/usr/local/bin/python2.7"
    GITBIN="git"
else
    PYBIN="python"
    GITBIN="git"
fi

if [ "$VCS" = "git" ]; then
    # Make sure required env vars are set.
    if [ "$GIT_FUNFUZZ_REPO" == "" ]; then
        echo "GIT_FUNFUZZ_REPO not set."
	exit 11
    fi
    if [ "$GIT_LITHIUM_REPO" == "" ]; then
        echo "GIT_LITHIUM_REPO not set."
	exit 12
    fi
    if [ "$GIT_FUNFUZZ_PRIVATE_REPO" == "" ]; then
        echo "GIT_FUNFUZZ_PRIVATE_REPO not set."
	exit 13
    fi
    REPO_NAME="funfuzz"
    # We need to wrap our git operation for private repos in a shell script so
    # we can set things like the user (gitolite3). We use GIT_PRIVATE_SSH to
    # point to this script
    GIT_PRIVATE_SSH=./git_private_ssh.bash
    echo -e '#!/bin/bash\nssh -l gitolite3 $1 $2' > $GIT_PRIVATE_SSH
    chmod 755 $GIT_PRIVATE_SSH
    rm -rf $REPO_NAME lithium funfuzz-private
    $GITBIN clone $GIT_FUNFUZZ_REPO $REPO_NAME
    $GITBIN clone $GIT_LITHIUM_REPO lithium
    GIT_SSH=$GIT_PRIVATE_SSH $GITBIN clone $GIT_FUNFUZZ_PRIVATE_REPO funfuzz-private
else
    test $HG_BUNDLE && BUNDLE_ARGS="--bundle $HG_BUNDLE"
    REPO_NAME="fuzzing"
    $PYBIN $SCRIPTS_DIR/buildfarm/utils/hgtool.py $BUNDLE_ARGS $HG_REPO $REPO_NAME
fi

$PYBIN $REPO_NAME/bot.py --remote-host "$FUZZ_REMOTE_HOST" --basedir "$FUZZ_BASE_DIR"
