#!/bin/bash

function usage {
cat <<EOF

Usage: `basename $0` -h # Displays this usage/help text
Usage: `basename $0` -x # lists exit codes
Usage: `basename $0` [-n] [-c] [-d] [-a]
           [-p product]
           [--hgtool hgtool_location]
           [--mirror hg_mirror --bundle bundle_location]
           [-u hg_ssh_user]
           [-k hg_ssh_key]
           [-r existing_repo_dir]
           # Use mozilla-central builds to check HSTS & HPKP
           [--use-mozilla-central]
           # One (or more) of the following actions must be specified.
           --hsts | --hpkp | --blocklist
           -b branch

EOF
}

function exitcodes {
cat <<EOF

EXIT CODES for `basename $0`:
    0    Success (either no updates to push, or push was successful)
    2    Updates are available, but were not pushed due to dry-run mode.
   11    Unknown command-line option provided
   12    Branch not specified on command-line
   13    No update action specified on command-line
   14    Invalid product specified
   21    Unable to parse version from version.txt
   31    Missing downloaded browser artifact
   32    Missing downloaded tests artifact
   41    Missing downloaded HSTS file
   42    Generated HSTS preload list is empty
   51    Missing downloaded HPKP file
   52    Generated HPKP preload list is empty
   61    Downloaded AMO blocklist file isn't valid XML
   62    Downloaded hg blocklist file isn't valid XML
   70    HSTS script failed
   71    HPKP script failed

EOF
}

DRY_RUN=false
PRODUCT="firefox"
BRANCH=""
PLATFORM="linux-x86_64"
TBOX_BUILDS_PLATFORM="linux64"
PLATFORM_EXT="tar.bz2"
UNPACK_CMD="tar jxf"
CLOSED_TREE=false
DONTBUILD=false
APPROVAL=false
HG_SSH_USER='ffxbld'
HG_SSH_KEY='~cltbld/.ssh/ffxbld_rsa'
REPODIR=''
HGTOOL="$(dirname "${0}")/../../buildfarm/utils/hgtool.py"
MIRROR=''
BUNDLE=''
APP_DIR=''
APP_ID=''
APP_NAME=''
LOCALHOST=`/bin/hostname -s`
HGHOST="hg.mozilla.org"
STAGEHOST="stage.mozilla.org"
HG=hg
WGET="wget -nv"
UNZIP="unzip -q"
DIFF="diff -up"
BASEDIR=`pwd`
SCRIPTDIR=`dirname $0`
VERSION=''
MCVERSION=''
USE_MC=false
FLATTENED=true

DO_HSTS=false
HSTS_PRELOAD_SCRIPT="getHSTSPreloadList.js"
HSTS_PRELOAD_ERRORS="nsSTSPreloadList.errors"
HSTS_PRELOAD_INC="nsSTSPreloadList.inc"
HSTS_UPDATED=false

DO_HPKP=false
HPKP_PRELOAD_SCRIPT="genHPKPStaticPins.js"
HPKP_PRELOAD_ERRORS="StaticHPKPins.errors"
HPKP_DER_TEST="default-ee.der"
HPKP_PRELOAD_JSON="PreloadedHPKPins.json"
HPKP_PRELOAD_OUTPUT="StaticHPKPins.h"
HPKP_UPDATED=false

DO_BLOCKLIST=false
BLOCKLIST_URL_AMO=''
BLOCKLIST_URL_HG=''
BLOCKLIST_UPDATED=false

# Get the current in-tree version for a code branch.
function get_version {
    VERSION_REPO=$1
    VERSION_FILE='version.txt'

    cd "${BASEDIR}"
    VERSION_URL_HG="${VERSION_REPO}/raw-file/default/${APP_DIR}/config/version.txt"
    rm -f ${VERSION_FILE}
    ${WGET} --no-check-certificate -O ${VERSION_FILE} ${VERSION_URL_HG}
    WGET_STATUS=$?
    if [ ${WGET_STATUS} != 0 ]; then
        echo "ERROR: wget exited with a non-zero exit code: $WGET_STATUS" >&2
        exit ${WGET_STATUS}
    fi
    PARSED_VERSION=`cat version.txt`
    if [ "${PARSED_VERSION}" == "" ]; then
        echo "ERROR: Unable to parse version from $VERSION_FILE" >&2
        exit 21
    fi
    rm -f ${VERSION_FILE}
    echo ${PARSED_VERSION}
}

# Cleanup common artifacts.
function preflight_cleanup {
    cd "${BASEDIR}"
    rm -rf "${PRODUCT}" tests "${BROWSER_ARCHIVE}" "${TESTS_ARCHIVE}"
}

function download_shared_artifacts {
    cd "${BASEDIR}"

    # Download everything we need: browser, tests, updater script, existing preload list and errors.
    echo "INFO: Downloading all the necessary pieces..."
    POSSIBLE_ARTIFACT_DIRS="nightly/latest-${REPODIR} tinderbox-builds/${REPODIR}-${TBOX_BUILDS_PLATFORM}/latest"
    if [ "${USE_MC}" == "true" ]; then
        POSSIBLE_ARTIFACT_DIRS="nightly/latest-mozilla-central"
    fi
    for ARTIFACT_DIR in ${POSSIBLE_ARTIFACT_DIRS}; do
        BROWSER_ARCHIVE_URL="http://${STAGEHOST}/pub/mozilla.org/${PRODUCT}/${ARTIFACT_DIR}/${BROWSER_ARCHIVE}"
        TESTS_ARCHIVE_URL="http://${STAGEHOST}/pub/mozilla.org/${PRODUCT}/${ARTIFACT_DIR}/${TESTS_ARCHIVE}"

	echo "INFO: ${WGET} --no-check-certificate ${BROWSER_ARCHIVE_URL}"
        ${WGET} --no-check-certificate ${BROWSER_ARCHIVE_URL}
	echo "INFO: ${WGET} --no-check-certificate ${TESTS_ARCHIVE_URL}"
        ${WGET} --no-check-certificate ${TESTS_ARCHIVE_URL}
        if [ -f ${BROWSER_ARCHIVE} -a -f ${TESTS_ARCHIVE} ]; then
            break
	fi
    done
    if [ ! -f ${BROWSER_ARCHIVE} ]; then
        echo "Downloaded file '${BROWSER_ARCHIVE}' not found in directory '$(pwd)'." >&2
        exit 31
    fi
    if [ ! -f ${TESTS_ARCHIVE} ]; then
        echo "Downloaded file '${TESTS_ARCHIVE}' not found in directory '$(pwd)'." >&2
        exit 32
    fi
    # Unpack the browser and move xpcshell in place for updating the preload list.
    echo "INFO: Unpacking resources..."
    ${UNPACK_CMD} "${BROWSER_ARCHIVE}"
    mkdir tests && cd tests
    ${UNZIP} "../${TESTS_ARCHIVE}"
    cd "${BASEDIR}"
    cp tests/bin/xpcshell "${PRODUCT}"
}

# gtk3 is required to run xpcshell as of Gecko 42.
function download_gtk3 {
    sh ${SCRIPTDIR}/../tooltool/tooltool_wrapper.sh ${SCRIPTDIR}/periodic_file_updates.manifest https://api.pub.build.mozilla.org/tooltool/ setup.sh /builds/tooltool.py --authentication-file /builds/relengapi.tok
    LD_LIBRARY_PATH=${BASEDIR}/gtk3/usr/local/lib
}

# In bug 1164714, the public/src subdirectories were flattened away under security/manager.
# We need to check whether the HGREPO were processing has had that change uplifted yet so
# that we can find the files we need to update.
function is_flattened {
    # This URL will only be present in repos that have *not* been flattened.
    TEST_URL="${HGREPO}/raw-file/default/security/manager/boot/src/"
    HTTP_STATUS=`${WGET} --spider -S ${TEST_URL} 2>&1 | grep "HTTP/" | awk '{print $2}'`
    if [ "$HTTP_STATUS" == "200" ]; then
        export FLATTENED=false
    fi
}

# Downloads the current in-tree HSTS (HTTP Strict Transport Security) files.
# Runs a simple xpcshell script to generate up-to-date HSTS information.
# Compares the new HSTS output with the old to determine whether we need to update.
function compare_hsts_files {
    cd "${BASEDIR}"
    HSTS_PRELOAD_SCRIPT_HG="${HGREPO}/raw-file/default/security/manager/tools/${HSTS_PRELOAD_SCRIPT}"
    if [ "${FLATTENED}" == "true" ]; then
        HSTS_PRELOAD_ERRORS_HG="${HGREPO}/raw-file/default/security/manager/ssl/${HSTS_PRELOAD_ERRORS}"
        HSTS_PRELOAD_INC_HG="${HGREPO}/raw-file/default/security/manager/ssl/${HSTS_PRELOAD_INC}"
    else
        HSTS_PRELOAD_ERRORS_HG="${HGREPO}/raw-file/default/security/manager/boot/src/${HSTS_PRELOAD_ERRORS}"
        HSTS_PRELOAD_INC_HG="${HGREPO}/raw-file/default/security/manager/boot/src/${HSTS_PRELOAD_INC}"
    fi

    # Download everything we need: browser, tests, updater script, existing preload list and errors.
    echo "INFO: Downloading all the necessary pieces to update HSTS..."
    rm -rf "${HSTS_PRELOAD_SCRIPT}" "${HSTS_PRELOAD_ERRORS}" "${HSTS_PRELOAD_INC}"
    for URL in "${HSTS_PRELOAD_SCRIPT_HG}" "${HSTS_PRELOAD_ERRORS_HG}" "${HSTS_PRELOAD_INC_HG}"; do
        echo "INFO: ${WGET} --no-check-certificate ${URL}"
        ${WGET} --no-check-certificate ${URL}
        WGET_STATUS=$?
        if [ ${WGET_STATUS} != 0 ]; then
            echo "ERROR: wget exited with a non-zero exit code: ${WGET_STATUS}" >&2
            exit ${WGET_STATUS}
        fi
        if [ ! -f "$(basename "${URL}")" ]; then
            echo "Downloaded file '$(basename "${URL}")' not found in directory '$(pwd)' - this should have been downloaded above from ${URL}." >&2
            exit 41
        fi
    done

    # Run the script to get an updated preload list.
    echo "INFO: Generating new HSTS preload list..."
    cd "${BASEDIR}/${PRODUCT}"
    echo INFO: Running \"LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./xpcshell ${BASEDIR}/${HSTS_PRELOAD_SCRIPT} ${BASEDIR}/${HSTS_PRELOAD_INC}\"
    LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./xpcshell "${BASEDIR}/${HSTS_PRELOAD_SCRIPT}" "${BASEDIR}/${HSTS_PRELOAD_INC}"
    XPCSHELL_STATUS=$?
    if [ ${XPCSHELL_STATUS} != 0 ]; then
        cat ${HSTS_PRELOAD_ERRORS}
        echo "ERROR: xpcshell exited with a non-zero exit code: ${XPCSHELL_STATUS}" >&2
        exit 70
    fi

    # The created files should be non-empty.
    echo "INFO: Checking whether new HSTS preload list is valid..."
    if [ ! -s "${HSTS_PRELOAD_ERRORS}" ]; then
        echo "New HSTS preload error list ${HSTS_PRELOAD_ERRORS} is empty. I guess that's good?" >&2
    fi
    if [ ! -s "${HSTS_PRELOAD_INC}" ]; then
        echo "New HSTS preload list ${HSTS_PRELOAD_INC} is empty. That's less good." >&2
        exit 42
    fi
    cd "${BASEDIR}"

    # Check for differences
    echo "INFO: diffing old/new HSTS error lists..."
    ${DIFF} "${HSTS_PRELOAD_ERRORS}" "${PRODUCT}/${HSTS_PRELOAD_ERRORS}"
    DIFF_STATUS=$?
    case "${DIFF_STATUS}" in
        0) echo "INFO: New HSTS preload error list matches what is in-tree.";;
        1) echo "INFO: New HSTS preload error list differs from what is in-tree.";;
        *) echo "ERROR: diff exited with exit code: ${DIFF_STATUS}" >&2
           exit ${DIFF_STATUS}
    esac

    echo "INFO: diffing old/new HSTS preload lists..."
    ${DIFF} "${HSTS_PRELOAD_INC}" "${PRODUCT}/${HSTS_PRELOAD_INC}"
    DIFF_STATUS=$?
    case "${DIFF_STATUS}" in
        0) echo "INFO: New HSTS preload list matches what is in-tree.";;
        1) echo "INFO: New HSTS preload list differs from what is in-tree.";;
        *) echo "ERROR: diff exited with exit code: ${DIFF_STATUS}" >&2
           exit ${DIFF_STATUS}
    esac
    return ${DIFF_STATUS}
}

# Downloads the current in-tree HPKP (HTTP public key pinning) files.
# Runs a simple xpcshell script to generate up-to-date HPKP information.
# Compares the new HPKP output with the old to determine whether we need to update.
function compare_hpkp_files {
    cd "${BASEDIR}"

    HPKP_PRELOAD_SCRIPT_HG="${HGREPO}/raw-file/default/security/manager/tools/${HPKP_PRELOAD_SCRIPT}"
    HPKP_PRELOAD_JSON_HG="${HGREPO}/raw-file/default/security/manager/tools/${HPKP_PRELOAD_JSON}"
    HPKP_DER_TEST_HG="${HGREPO}/raw-file/default/security/manager/ssl/tests/unit/tlsserver/${HPKP_DER_TEST}"
    if [ "${FLATTENED}" == "true" ]; then
        HPKP_PRELOAD_ERRORS_HG="${HGREPO}/raw-file/default/security/manager/ssl/${HPKP_PRELOAD_ERRORS}"
        HPKP_PRELOAD_OUTPUT_HG="${HGREPO}/raw-file/default/security/manager/ssl/${HPKP_PRELOAD_OUTPUT}"
    else
        HPKP_PRELOAD_ERRORS_HG="${HGREPO}/raw-file/default/security/manager/boot/src/${HPKP_PRELOAD_ERRORS}"
        HPKP_PRELOAD_OUTPUT_HG="${HGREPO}/raw-file/default/security/manager/boot/src/${HPKP_PRELOAD_OUTPUT}"
    fi

    # Download everything we need: browser, tests, updater script, existing preload list and errors.
    echo "INFO: Downloading all the necessary pieces to update HPKP..."
    rm -f "${HPKP_PRELOAD_SCRIPT}" "${HPKP_PRELOAD_JSON}" "${HPKP_DER_TEST}" "${HPKP_PRELOAD_OUTPUT}" "${HPKP_PRELOAD_ERRORS}"
    for URL in "${HPKP_PRELOAD_SCRIPT_HG}" "${HPKP_PRELOAD_JSON_HG}" "${HPKP_DER_TEST_HG}" "${HPKP_PRELOAD_OUTPUT_HG}" "${HPKP_PRELOAD_ERRORS_HG}"; do
        echo "INFO: ${WGET} --no-check-certificate ${URL}"
        ${WGET} --no-check-certificate ${URL}
        WGET_STATUS=$?
        if [ ${WGET_STATUS} != 0 ]; then
            echo "ERROR: wget exited with a non-zero exit code: ${WGET_STATUS}" >&2
            exit ${WGET_STATUS}
        fi
        if [ ! -f "$(basename "${URL}")" ]; then
            echo "Downloaded file '$(basename "${URL}")' not found in directory '$(pwd)' - this should have been downloaded above from ${URL}." >&2
            exit 51
        fi
    done

    # Run the script to get an updated preload list.
    echo "INFO: Generating new HPKP preload list..."
    cd "${BASEDIR}/${PRODUCT}"
    echo INFO: Running \"LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./xpcshell ${BASEDIR}/${HPKP_PRELOAD_SCRIPT} ${BASEDIR}/${HPKP_PRELOAD_JSON} ${BASEDIR}/${HPKP_DER_TEST} ${BASEDIR}/${PRODUCT}/${HPKP_PRELOAD_OUTPUT}\"
    LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./xpcshell "${BASEDIR}/${HPKP_PRELOAD_SCRIPT}" "${BASEDIR}/${HPKP_PRELOAD_JSON}" "${BASEDIR}/${HPKP_DER_TEST}" "${BASEDIR}/${PRODUCT}/${HPKP_PRELOAD_OUTPUT}" > "${HPKP_PRELOAD_ERRORS}" 2>&1
    XPCSHELL_STATUS=$?
    if [ ${XPCSHELL_STATUS} != 0 ]; then
        cat ${HPKP_PRELOAD_ERRORS}
        echo "ERROR: xpcshell exited with a non-zero exit code: ${XPCSHELL_STATUS}" >&2
        exit 71
    fi

    # The created files should be non-empty.
    echo "INFO: Checking whether new HPKP preload list is valid..."
    if [ ! -s "${HPKP_PRELOAD_ERRORS}" ]; then
        echo "New ${HPKP_PRELOAD_ERRORS} error list is empty. I guess that's good?" >&2
    fi
    if [ ! -s "${HPKP_PRELOAD_OUTPUT}" ]; then
        echo "${HPKP_PRELOAD_OUTPUT} is empty. That's less good." >&2
        exit 52
    fi
    cd "${BASEDIR}"

    # Check for differences
    echo "INFO: diffing old/new HPKP error lists..."
    ${DIFF} "${HPKP_PRELOAD_ERRORS}" "${PRODUCT}/${HPKP_PRELOAD_ERRORS}"
    DIFF_STATUS=$?
    case "${DIFF_STATUS}" in
        0) echo "INFO: New HPKP preload error matches what is in-tree.";;
        1) echo "INFO: New HPKP preload error lists differs from what is in-tree.";;
        *) echo "ERROR: diff exited with exit code: ${DIFF_STATUS}" >&2
           exit ${DIFF_STATUS}
    esac

    echo "INFO: diffing old/new HPKP preload lists..."
    ${DIFF} "${HPKP_PRELOAD_OUTPUT}" "${PRODUCT}/${HPKP_PRELOAD_OUTPUT}"
    DIFF_STATUS=$?
    case "${DIFF_STATUS}" in
        0) echo "INFO: New HPKP preload lists matches what is in-tree.";;
        1) echo "INFO: New HPKP preload lists differs from what is in-tree.";;
        *) echo "ERROR: diff exited with exit code: ${DIFF_STATUS}" >&2
           exit ${DIFF_STATUS}
    esac
    return ${DIFF_STATUS}
}

# Downloads the current in-tree blocklist file.
# Downloads the current blocklist file from AMO.
# Compares the AMO blocklist with the in-tree blocklist to determine whether we need to update.
function compare_blocklist_files {
    BLOCKLIST_URL_AMO="https://blocklist.addons.mozilla.org/blocklist/3/${APP_ID}/${VERSION}/${APP_NAME}/20090105024647/blocklist-sync/en-US/nightly/blocklist-sync/default/default/"
    BLOCKLIST_URL_HG="${HGREPO}/raw-file/default/${APP_DIR}/app/blocklist.xml"

    cd "${BASEDIR}"
    rm -f blocklist_amo.xml
    echo "INFO: ${WGET} --no-check-certificate -O blocklist_amo.xml ${BLOCKLIST_URL_AMO}"
    ${WGET} --no-check-certificate -O blocklist_amo.xml ${BLOCKLIST_URL_AMO}
    WGET_STATUS=$?
    if [ ${WGET_STATUS} != 0 ]; then
        echo "ERROR: wget exited with a non-zero exit code: ${WGET_STATUS}"
        exit ${WGET_STATUS}
    fi

    rm -f blocklist_hg.xml
    echo "INFO: ${WGET} -O blocklist_hg.xml ${BLOCKLIST_URL_HG}"
    ${WGET} -O blocklist_hg.xml ${BLOCKLIST_URL_HG}
    WGET_STATUS=$?
    if [ ${WGET_STATUS} != 0 ]; then
        echo "ERROR: wget exited with a non-zero exit code: ${WGET_STATUS}" >&2
        exit ${WGET_STATUS}
    fi

    # The downloaded files should be non-empty and have a valid xml header
    # if they were retrieved properly, and some random HTML garbage if not.
    XML_HEADER='<?xml version="1.0"?>'
    AMO_HEADER=`head -n1 blocklist_amo.xml`
    HG_HEADER=`head -n1 blocklist_hg.xml`
    if [ ! -s "blocklist_amo.xml" -o "${XML_HEADER}" != "${AMO_HEADER}" ]; then
        echo "AMO blocklist does not appear to be an XML file. wget error?" >&2
        exit 61
    fi
    if [ ! -s "blocklist_hg.xml" -o "${XML_HEADER}" != "${HG_HEADER}" ]; then
        echo "HG blocklist does not appear to be an XML file. wget error?" >&2
        exit 62
    fi

    echo "INFO: diffing in-tree blocklist against the blocklist from AMO..."
    ${DIFF} blocklist_hg.xml blocklist_amo.xml >/dev/null 2>&1
    DIFF_STATUS=$?
    case "${DIFF_STATUS}" in
        0) echo "INFO: AMO blocklist is identical to the one in-tree.";;
        1) echo "INFO: AMO blocklist differs from the one in-tree.";;
        *) echo "ERROR: diff exited with exit code: ${DIFF_STATUS}" >&2
           exit ${DIFF_STATUS}
    esac
    return ${DIFF_STATUS}
}

# Clones an hg repo, using hgtool preferentially.
function clone_repo {
    cd "${BASEDIR}"
    if [ ! -d "${REPODIR}" ]; then
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
            echo "INFO: hgtool.py not found. Falling back to vanilla hg."
            CLONE_CMD="${HG} clone"
        fi
        CLONE_CMD="${CLONE_CMD} ${HGREPO} ${REPODIR}"

        echo "INFO: cloning from hg: ${CLONE_CMD}"
        ${CLONE_CMD}
        CLONE_STATUS=$?
        if [ ${CLONE_STATUS} != 0 ]; then
            echo "ERROR: hg clone exited with a non-zero exit code: ${CLONE_STATUS}" >&2
            exit ${CLONE_STATUS}
        fi
    fi

    echo "INFO: pulling from hg: ${HG} -R ${REPODIR} pull"
    ${HG} -R ${REPODIR} pull
    PULL_STATUS=$?
    if [ ${PULL_STATUS} != 0 ]; then
        echo "ERROR: hg pull exited with a non-zero exit code: ${PULL_STATUS}" >&2
        exit ${PULL_STATUS}
    fi
    echo "INFO: updating to default: ${HG} -R ${REPODIR} update -C default"
    ${HG} -R ${REPODIR} update -C default
    UPDATE_STATUS=$?
    if [ ${UPDATE_STATUS} != 0 ]; then
        echo "ERROR: hg update exited with a non-zero exit code: ${UPDATE_STATUS}" >&2
        exit ${UPDATE_STATUS}
    fi
}

# Copies new HSTS files in place, and commits them.
function commit_hsts_files {
    cd "${BASEDIR}"
    if [ "${FLATTENED}" == "true" ]; then
        cp -f ${PRODUCT}/${HSTS_PRELOAD_ERRORS} ${REPODIR}/security/manager/ssl/
        cp -f ${PRODUCT}/${HSTS_PRELOAD_INC} ${REPODIR}/security/manager/ssl/
    else
        cp -f ${PRODUCT}/${HSTS_PRELOAD_ERRORS} ${REPODIR}/security/manager/boot/src/
	cp -f ${PRODUCT}/${HSTS_PRELOAD_INC} ${REPODIR}/security/manager/boot/src/
    fi
    COMMIT_MESSAGE="No bug, Automated HSTS preload list update from host ${LOCALHOST}"
    if [ ${DONTBUILD} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
    fi
    if [ ${CLOSED_TREE} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
    fi
    if [ ${APPROVAL} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=hsts-update"
    fi
    echo "INFO: committing HSTS changes: ${HG} -R ${REPODIR} commit -u \"${HG_SSH_USER}\" -m \"${COMMIT_MESSAGE}\""
    ${HG} -R ${REPODIR} commit -u "${HG_SSH_USER}" -m "${COMMIT_MESSAGE}"
    COMMIT_STATUS=$?
    if [ ${COMMIT_STATUS} == 0 ]; then
        echo "INFO: hg commit succeeded for HSTS files."
    fi
    return ${COMMIT_STATUS}
}

# Copies new HPKP files in place, and commits them.
function commit_hpkp_files {
    cd "${BASEDIR}"
    if [ "${FLATTENED}" == "true" ]; then
        cp -f ${PRODUCT}/${HPKP_PRELOAD_ERRORS} ${REPODIR}/security/manager/ssl/
        cp -f ${PRODUCT}/${HPKP_PRELOAD_OUTPUT} ${REPODIR}/security/manager/ssl/
    else
        cp -f ${PRODUCT}/${HPKP_PRELOAD_ERRORS} ${REPODIR}/security/manager/boot/src/
        cp -f ${PRODUCT}/${HPKP_PRELOAD_OUTPUT} ${REPODIR}/security/manager/boot/src/
    fi
    COMMIT_MESSAGE="No bug, Automated HPKP preload list update from host ${LOCALHOST}"
    if [ ${DONTBUILD} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
    fi
    if [ ${CLOSED_TREE} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
    fi
    if [ ${APPROVAL} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=hpkp-update"
    fi
    echo "INFO: committing HPKP changes: ${HG} -R ${REPODIR} commit -u \"${HG_SSH_USER}\" -m \"${COMMIT_MESSAGE}\""
    ${HG} -R ${REPODIR} commit -u "${HG_SSH_USER}" -m "${COMMIT_MESSAGE}"
    COMMIT_STATUS=$?
    if [ ${COMMIT_STATUS} == 0 ]; then
        echo "INFO: hg commit succeeded for HPKP files."
    fi
    return ${COMMIT_STATUS}
}

# Copies new blocklist file in place, and commits it.
function commit_blocklist_files {
    cd "${BASEDIR}"
    cp -f blocklist_amo.xml ${REPODIR}/${APP_DIR}/app/blocklist.xml
    COMMIT_MESSAGE="No bug, Automated blocklist update from host ${LOCALHOST}"
    if [ ${DONTBUILD} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
    fi
    if [ ${CLOSED_TREE} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
    fi
    if [ ${APPROVAL} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=blocklist-update"
    fi
    echo "INFO: committing blocklist changes: ${HG} -R ${REPODIR} commit -u \"${HG_SSH_USER}\" -m \"${COMMIT_MESSAGE}\""
    ${HG} -R ${REPODIR} commit -u "${HG_SSH_USER}" -m "${COMMIT_MESSAGE}"
    COMMIT_STATUS=$?
    if [ ${COMMIT_STATUS} == 0 ]; then
        echo "INFO: hg commit succeeded for blocklist files."
    fi
    return ${COMMIT_STATUS}
}

# Push all pending commits to hg.
function push_repo {
    cd "${BASEDIR}"
    echo ${HG} -R ${REPODIR} push -e \"ssh -l ${HG_SSH_USER} -i ${HG_SSH_KEY}\" ${HGPUSHREPO}
    ${HG} -R ${REPODIR} push -e "ssh -l ${HG_SSH_USER} -i ${HG_SSH_KEY}" ${HGPUSHREPO}
    PUSH_STATUS=$?
    if [ ${PUSH_STATUS} != 0 ]; then
        echo "ERROR: hg push exited with exit code: ${PUSH_STATUS}, probably raced another changeset"
        echo ${HG} -R ${REPODIR} rollback
        ${HG} -R ${REPODIR} rollback
        ROLLBACK_STATUS=$?
        if [ ${ROLLBACK_STATUS} != 0 ]; then
            echo "ERROR: hg rollback failed with exit code: ${ROLLBACK_STATUS}" >&2
            echo "This is unrecoverable, removing the local clone to start fresh next time." >&2
            rm -rf ${REPODIR}
            exit ${ROLLBACK_STATUS}
        fi
    else
        echo "INFO: hg push succeeded."
    fi
    return ${PUSH_STATUS}
}

# Main

# Parse our command-line options.
while [ $# -gt 0 ]; do
    case "$1" in
        -h) usage; exit 0;;
        -x) exitcodes; exit 0;;
        -p) PRODUCT="$2"; shift;;
        -b) BRANCH="$2"; shift;;
        -n) DRY_RUN=true;;
        -c) CLOSED_TREE=true;;
        -d) DONTBUILD=true;;
        -a) APPROVAL=true;;
        --hsts) DO_HSTS=true;;
        --hpkp) DO_HPKP=true;;
        --blocklist) DO_BLOCKLIST=true;;
        -u) HG_SSH_USER="$2"; shift;;
        -k) HG_SSH_KEY="$2"; shift;;
        -r) REPODIR="$2"; shift;;
        --mirror) MIRROR="$2"; shift;;
        --bundle) BUNDLE="$2"; shift;;
        --use-mozilla-central) USE_MC=true;;
        -*) usage
            exit 11;;
        *)  break;; # terminate while loop
    esac
    shift
done

# Must supply a code branch to work with.
if [ "${BRANCH}" == "" ]; then
    echo "Error: You must specify a branch with -b branchname." >&2
    usage
    exit 12
fi

# Must choose at least one update action.
if [ "$DO_HSTS" == "false" -a "$DO_HPKP" == "false" -a "$DO_BLOCKLIST" == "false" ]; then
    echo "Error: you must specify at least one action from: --hsts, --hpkp, --blocklist" >&2
    usage
    exit 13
fi

# per-product constants
case "${PRODUCT}" in
    thunderbird)
        APP_DIR="mail"
        APP_ID="%7B3550f703-e582-4d05-9a08-453d09bdfdc6%7D"
        APP_NAME="Thunderbird"
        ;;
    firefox)
        APP_DIR="browser"
        APP_ID="%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D"
        APP_NAME="Firefox"
        ;;
    *)
        echo "Error: Invalid product specified"
        usage
        exit 14
        ;;
esac

if [ "${REPODIR}" == "" ]; then
   REPODIR=`basename ${BRANCH}`
fi

HGREPO="http://${HGHOST}/${BRANCH}"
HGPUSHREPO="ssh://${HGHOST}/${BRANCH}"
MCREPO="http://${HGHOST}/mozilla-central"

VERSION=$(get_version ${HGREPO})
echo "INFO: parsed version is ${VERSION}"
if [ "${USE_MC}" == "true" ]; then
    MCVERSION=$(get_version ${MCREPO})
    echo "INFO: parsed mozilla-central version is ${MCVERSION}"
fi

BROWSER_ARCHIVE="${PRODUCT}-${VERSION}.en-US.${PLATFORM}.${PLATFORM_EXT}"
TESTS_ARCHIVE="${PRODUCT}-${VERSION}.en-US.${PLATFORM}.common.tests.zip"
if [ "${USE_MC}" == "true" ]; then
    BROWSER_ARCHIVE="${PRODUCT}-${MCVERSION}.en-US.${PLATFORM}.${PLATFORM_EXT}"
    TESTS_ARCHIVE="${PRODUCT}-${MCVERSION}.en-US.${PLATFORM}.common.tests.zip"
fi

# Try to find hgtool if it hasn't been set.
if [ ! -f "${HGTOOL}" ]; then
    HGTOOL=`which hgtool.py 2>/dev/null | head -n1`
fi

preflight_cleanup
if [ "${DO_HSTS}" == "true" -o "${DO_HPKP}" == "true" ]; then
    download_shared_artifacts
    download_gtk3
fi
is_flattened
if [ "${DO_HSTS}" == "true" ]; then
    compare_hsts_files
    if [ $? != 0 ]; then
        HSTS_UPDATED=true
    fi
fi
if [ "${DO_HPKP}" == "true" ]; then
    compare_hpkp_files
    if [ $? != 0 ]; then
        HPKP_UPDATED=true
    fi
fi
if [ "${DO_BLOCKLIST}" == "true" ]; then
    compare_blocklist_files
    if [ $? != 0 ]; then
        BLOCKLIST_UPDATED=true
    fi
fi

if [ "${HSTS_UPDATED}" == "false" -a "${HPKP_UPDATED}" == "false" -a "${BLOCKLIST_UPDATED}" == "false" ]; then
    echo "INFO: no updates required. Exiting."
    exit 0
else
    if [ "${DRY_RUN}" == "true" ]; then
        echo "INFO: Updates are available, not updating hg in dry-run mode."
        exit 2
    fi
fi

clone_repo
if [ "${HSTS_UPDATED}" == "true" ]; then
  commit_hsts_files
fi

if [ "${HPKP_UPDATED}" == "true" ]; then
  commit_hpkp_files
fi

if [ "${BLOCKLIST_UPDATED}" == "true" ]; then
  commit_blocklist_files
fi
push_repo
