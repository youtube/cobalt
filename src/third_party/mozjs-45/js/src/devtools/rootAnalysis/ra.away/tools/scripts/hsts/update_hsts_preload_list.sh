#!/bin/bash

USAGE()
{
cat <<EOF
 DEPRECATED: use tools/scripts/periodic_file_updates/periodic_file_updates.sh instead.

 usage: `basename $0` [-n] [-c] [-d] [-a]
           [-p product]
           [--hgtool hgtool_location]
           [--mirror hg_mirror --bundle bundle_location]
           [-u hg_ssh_user]
           [-k hg_ssh_key]
           [-r existing_repo_dir]
           -b branch

EOF
}

DRY_RUN=false
PRODUCT="firefox"
BRANCH=""
LATEST_DIR=""
PLATFORM="linux-x86_64"
PLATFORM_EXT="tar.bz2"
UNPACK_CMD="tar jxf"
CLOSED_TREE=false
DONTBUILD=false
APPROVAL=false
HG_SSH_USER='ffxbld'
HG_SSH_KEY='~cltbld/.ssh/ffxbld_rsa'
REPODIR='hsts'
HGTOOL=''
MIRROR=''
BUNDLE=''
APP_DIR="browser"
LOCALHOST=`/bin/hostname -s`
HGHOST="hg.mozilla.org"
STAGEHOST="stage.mozilla.org"
HG=hg
WGET=wget
UNZIP="unzip -q"
DIFF="diff -up"
PRELOAD_SCRIPT="getHSTSPreloadList.js"
PRELOAD_ERRORS="nsSTSPreloadList.errors"
PRELOAD_INC="nsSTSPreloadList.inc"
BASEDIR=`pwd`

while [ $# -gt 0 ]; do
    case "$1" in
	-p) PRODUCT="$2"; shift;;
        -b) BRANCH="$2"; shift;;
        -n) DRY_RUN=true;;
        -c) CLOSED_TREE=true;;
        -d) DONTBUILD=true;;
        -a) APPROVAL=true;;
        -u) HG_SSH_USER="$2"; shift;;
        -k) HG_SSH_KEY="$2"; shift;;
        -r) REPODIR="$2"; shift;;
        --hgtool) HGTOOL="$2"; shift;;
        --mirror) MIRROR="$2"; shift;;
        --bundle) BUNDLE="$2"; shift;;
        -*) USAGE
            exit 1;;
        *)  break;; # terminate while loop
    esac
    shift
done

if [ "$BRANCH" == "" ]; then
    USAGE
    exit 1
else
    LATEST_DIR=latest-`basename ${BRANCH}`
fi

HGREPO="http://${HGHOST}/${BRANCH}"
HGPUSHREPO="ssh://${HGHOST}/${BRANCH}"

# Try to find hgtool if it hasn't been set.
if [ ! -f "${HGTOOL}" ]; then
    HGTOOL=`which hgtool.py 2>/dev/null | head -n1`
fi

compare_preload_lists()
{
    echo "INFO: Retrieving current version from hg..."
    VERSION_URL_HG="${HGREPO}/raw-file/default/${APP_DIR}/config/version.txt"
    rm -f version.txt
    ${WGET} --no-check-certificate -O version.txt ${VERSION_URL_HG}
    WGET_STATUS=$?
    if [ ${WGET_STATUS} != 0 ]; then
        echo "ERROR: wget exited with a non-zero exit code: $WGET_STATUS"
        exit ${WGET_STATUS}
    fi
    VERSION=`cat version.txt`
    if [ "${VERSION}" == "" ]; then
        echo "ERROR: Unable to parse version from version.txt"
	exit 1
    fi

    BROWSER_ARCHIVE="${PRODUCT}-${VERSION}.en-US.${PLATFORM}.${PLATFORM_EXT}"
    BROWSER_ARCHIVE_URL="http://${STAGEHOST}/pub/mozilla.org/${PRODUCT}/nightly/${LATEST_DIR}/${BROWSER_ARCHIVE}"
    TESTS_ARCHIVE="${PRODUCT}-${VERSION}.en-US.${PLATFORM}.tests.zip"
    TESTS_ARCHIVE_URL="http://${STAGEHOST}/pub/mozilla.org/${PRODUCT}/nightly/${LATEST_DIR}/${TESTS_ARCHIVE}"
    PRELOAD_SCRIPT_HG="${HGREPO}/raw-file/default/security/manager/tools/${PRELOAD_SCRIPT}"
    PRELOAD_ERRORS_HG="${HGREPO}/raw-file/default/security/manager/boot/src/${PRELOAD_ERRORS}"
    PRELOAD_INC_HG="${HGREPO}/raw-file/default/security/manager/boot/src/${PRELOAD_INC}"

    # Download everything we need: browser, tests, updater script, existing preload list and errors.
    echo "INFO: Downloading all the necessary pieces..."
    rm -rf ${PRODUCT} tests ${BROWSER_ARCHIVE} ${TESTS_ARCHIVE} ${PRELOAD_SCRIPT} ${PRELOAD_ERRORS} ${PRELOAD_INC}
    for URL in ${BROWSER_ARCHIVE_URL} ${TESTS_ARCHIVE_URL} ${PRELOAD_SCRIPT_HG} ${PRELOAD_ERRORS_HG} ${PRELOAD_INC_HG}; do
	${WGET} --no-check-certificate ${URL}
	WGET_STATUS=$?
	if [ ${WGET_STATUS} != 0 ]; then
            echo "ERROR: wget exited with a non-zero exit code: ${WGET_STATUS}"
            exit ${WGET_STATUS}
	fi
    done
    for F in ${BROWSER_ARCHIVE} ${TESTS_ARCHIVE} ${PRELOAD_SCRIPT} ${PRELOAD_ERRORS} ${PRELOAD_INC}; do
	if [ ! -f ${F} ]; then
	    echo "Downloaded file ${F} not found."
	    exit 1
	fi
    done

    # Unpack the browser and move xpcshell in place for updating the preload list.
    echo "INFO: Unpacking resources..."
    ${UNPACK_CMD} ${BROWSER_ARCHIVE}
    mkdir tests && cd tests
    ${UNZIP} ../${TESTS_ARCHIVE}
    cd ${BASEDIR}
    cp tests/bin/xpcshell ${PRODUCT}

    # Run the script to get an updated preload list.
    echo "INFO: Generating new HSTS preload list..."
    cd ${PRODUCT}
    echo INFO: Running \"LD_LIBRARY_PATH=. ./xpcshell ${BASEDIR}/${PRELOAD_SCRIPT} ${BASEDIR}/${PRELOAD_INC}\"
    LD_LIBRARY_PATH=. ./xpcshell ${BASEDIR}/${PRELOAD_SCRIPT} ${BASEDIR}/${PRELOAD_INC}

    # The created files should be non-empty.
    echo "INFO: Checking whether new HSTS preload list is valid..."
    if [ ! -s "${PRELOAD_ERRORS}" ]; then
        echo "New HSTS preload error list is empty. I guess that's good?"
    fi
    if [ ! -s "${PRELOAD_INC}" ]; then
        echo "New HSTS preload list is empty. That's less good."
        exit 1
    fi
    cd ${BASEDIR}

    # Check for differences
    echo "INFO: diffing old/new HSTS error lists..."
    ${DIFF} ${PRELOAD_ERRORS} ${PRODUCT}/${PRELOAD_ERRORS}
    DIFF_STATUS=$?
    case "${DIFF_STATUS}" in
        0|1) ;;
        *) echo "ERROR: diff exited with exit code: ${DIFF_STATUS}"
           exit ${DIFF_STATUS}
    esac

    echo "INFO: diffing old/new HSTS preload lists..."
    ${DIFF} ${PRELOAD_INC} ${PRODUCT}/${PRELOAD_INC}
    DIFF_STATUS=$?
    case "${DIFF_STATUS}" in
        0|1) ;;
        *) echo "ERROR: diff exited with exit code: ${DIFF_STATUS}"
           exit ${DIFF_STATUS}
    esac
    return ${DIFF_STATUS}
}

update_preload_list_in_hg()
{
    if [ ! -d ${REPODIR} ]; then
        CLONE_CMD=""
        if [ -f "${HGTOOL}" ]; then
	    # Need to pass the default branch here to avoid pollution from buildprops.json
	    # when hgtool.py is run in production.
	    CLONE_CMD="${HGTOOL} --branch default"
            if [ "${MIRROR}" != "" ]; then
                CLONE_CMD="${CLONE_CMD} --mirror ${MIRROR}"
            fi
            if [ "${BUNDLE}" != "" ]; then
                CLONE_CMD="${CLONE_CMD} --bundle ${BUNDLE}"
	    fi
        else
	    # Fallback on vanilla hg
	    echo "hgtool.py not found. Falling back to vanilla hg."
            CLONE_CMD="${HG} clone"
        fi
        CLONE_CMD="${CLONE_CMD} ${HGREPO} ${REPODIR}"

        echo ${CLONE_CMD}
        ${CLONE_CMD}
        CLONE_STATUS=$?
        if [ ${CLONE_STATUS} != 0 ]; then
            echo "ERROR: hg clone exited with a non-zero exit code: ${CLONE_STATUS}"
            return ${CLONE_STATUS}
        fi
    fi

    echo ${HG} -R ${REPODIR} pull
    ${HG} -R ${REPODIR} pull
    PULL_STATUS=$?
    if [ ${PULL_STATUS} != 0 ]; then
        echo "ERROR: hg pull exited with a non-zero exit code: ${PULL_STATUS}"
        return ${PULL_STATUS}
    fi
    echo ${HG} -R ${REPODIR} update -C default
    ${HG} -R ${REPODIR} update -C default
    UPDATE_STATUS=$?
    if [ ${UPDATE_STATUS} != 0 ]; then
        echo "ERROR: hg update exited with a non-zero exit code: ${UPDATE_STATUS}"
        return ${UPDATE_STATUS}
    fi

    cp -f ${PRODUCT}/${PRELOAD_ERRORS} ${REPODIR}/security/manager/boot/src/
    cp -f ${PRODUCT}/${PRELOAD_INC} ${REPODIR}/security/manager/boot/src/
    COMMIT_MESSAGE="No bug, Automated HSTS preload list update from host $LOCALHOST"
    if [ ${DONTBUILD} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
    fi
    if [ ${CLOSED_TREE} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
    fi
    if [ ${APPROVAL} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=hsts-update"
    fi
    echo ${HG} -R ${REPODIR} commit -u \"${HG_SSH_USER}\" -m \"${COMMIT_MESSAGE}\"
    ${HG} -R ${REPODIR} commit -u "${HG_SSH_USER}" -m "${COMMIT_MESSAGE}"
    echo ${HG} -R ${REPODIR} push -e \"ssh -l ${HG_SSH_USER} -i ${HG_SSH_KEY}\" ${HGPUSHREPO}
    ${HG} -R ${REPODIR} push -e "ssh -l ${HG_SSH_USER} -i ${HG_SSH_KEY}" ${HGPUSHREPO}
    PUSH_STATUS=$?
    if [ ${PUSH_STATUS} != 0 ]; then
        echo "ERROR: hg push exited with exit code: ${PUSH_STATUS}, probably raced another changeset"
        echo ${HG} -R ${REPODIR} rollback
        ${HG} -R ${REPODIR} rollback
        ROLLBACK_STATUS=$?
        if [ ${ROLLBACK_STATUS} != 0 ]; then
            echo "ERROR: hg rollback failed with exit code: ${ROLLBACK_STATUS}"
            echo "This is unrecoverable, removing the local clone to start fresh next time."
            rm -rf ${REPODIR}
            return ${ROLLBACK_STATUS}
        fi
    fi
    return ${PUSH_STATUS}
}

compare_preload_lists
result=$?
if [ ${result} != 0 ]; then
    if [ "${DRY_RUN}" == "true" ]; then
        echo "INFO: HSTS preload lists differ, but not updating hg in dry-run mode."
    else
        echo "INFO: HSTS preload lists differ, updating hg."
        update_preload_list_in_hg
        result=$?
    fi
else
    echo "INFO: HSTS preload lists are identical. Nothing to update."
fi
exit ${result}
