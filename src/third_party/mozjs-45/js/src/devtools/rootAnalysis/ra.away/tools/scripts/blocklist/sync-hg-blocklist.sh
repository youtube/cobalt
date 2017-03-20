#!/bin/bash

# 2011-02-17 - add product flag to make script usable for other apps (Callek)
# 2010-09-10 - compare downloaded single files before cloning (ccooper)
# 2008-06-27 - Copied from sync-blocklist (dtownsend)
# 2008-07-14 - Use a permanent local clone (dtownsend)
# 2008-07-24 - Fix hg username (dtownsend)

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
BRANCH=""
CLOSED_TREE=false
DONTBUILD=false
APPROVAL=false
HG_SSH_USER='ffxbld'
HG_SSH_KEY='~cltbld/.ssh/ffxbld_rsa'
PRODUCT='firefox'
REPODIR='blocklist'
HGTOOL=''
MIRROR=''
BUNDLE=''

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
fi

case "$PRODUCT" in
    firefox)
        APP_DIR="browser";
        APP_ID="%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D";
        APP_NAME="Firefox";;
    seamonkey)
        APP_DIR="suite";
        APP_ID="%7B92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a%7D";
        APP_NAME="SeaMonkey";;
    thunderbird)
        APP_DIR="mail";
        APP_ID="%7B3550f703-e582-4d05-9a08-453d09bdfdc6%7D";
        APP_NAME="Thunderbird";;
    *)
        echo >&2 "Invalid Product was passed to $0. Passed value was: $PRODUCT";
        USAGE
        exit 1;;
esac

HGHOST="hg.mozilla.org"
HGREPO="http://${HGHOST}/${BRANCH}"
HGPUSHREPO="ssh://${HGHOST}/${BRANCH}"
BLOCKLIST_URL_HG="${HGREPO}/raw-file/default/${APP_DIR}/app/blocklist.xml"

# Try to find hgtool if it hasn't been set.
if [ ! -f "${HGTOOL}" ]; then
    HGTOOL=`which hgtool.py 2>/dev/null | head -n1`
fi

HG=hg
WGET=wget
DIFF=diff
HOST=`/bin/hostname -s`

compare_blocklists()
{
    VERSION_URL_HG="${HGREPO}/raw-file/default/${APP_DIR}/config/version.txt"
    rm -f version.txt
    ${WGET} --no-check-certificate -O version.txt ${VERSION_URL_HG}
    WGET_STATUS=$?
    if [ ${WGET_STATUS} != 0 ]; then
        echo "ERROR wget exited with a non-zero exit code: $WGET_STATUS"
        return ${WGET_STATUS}
    fi
    VERSION=`cat version.txt | sed 's/[^.0-9]*$//'`
    if [ "${VERSION}" == "" ]; then
        echo "ERROR Unable to parse version from version.txt"
    fi

    BLOCKLIST_URL_AMO="https://blocklist.addons.mozilla.org/blocklist/3/${APP_ID}/${VERSION}/${APP_NAME}/20090105024647/blocklist-sync/en-US/nightly/blocklist-sync/default/default/"
    rm -f blocklist_amo.xml
    ${WGET} --no-check-certificate -O blocklist_amo.xml ${BLOCKLIST_URL_AMO}
    WGET_STATUS=$?
    if [ ${WGET_STATUS} != 0 ]; then
        echo "ERROR wget exited with a non-zero exit code: ${WGET_STATUS}"
        return ${WGET_STATUS}
    fi

    rm -f blocklist_hg.xml
    ${WGET} -O blocklist_hg.xml ${BLOCKLIST_URL_HG}
    WGET_STATUS=$?
    if [ ${WGET_STATUS} != 0 ]; then
        echo "ERROR wget exited with a non-zero exit code: ${WGET_STATUS}"
        return ${WGET_STATUS}
    fi

    # The downloaded files should be non-empty and have a valid xml header 
    # if they were retrieved properly, and some random HTML garbage if not.
    XML_HEADER='<?xml version="1.0"?>'
    AMO_HEADER=`head -n1 blocklist_amo.xml`
    HG_HEADER=`head -n1 blocklist_hg.xml`
    if [ ! -s "blocklist_amo.xml" -o "${XML_HEADER}" != "${AMO_HEADER}" ]; then
        echo "AMO blocklist does not appear to be an XML file. wget error?"
        exit 1
    fi 
    if [ ! -s "blocklist_hg.xml" -o "${XML_HEADER}" != "${HG_HEADER}" ]; then
        echo "HG blocklist does not appear to be an XML file. wget error?"
        exit 1
    fi

    ${DIFF} blocklist_hg.xml blocklist_amo.xml >/dev/null 2>&1
    DIFF_STATUS=$?
    case "${DIFF_STATUS}" in
        0|1) ;;
        *) echo "ERROR diff exited with exit code: ${DIFF_STATUS}"
           exit ${DIFF_STATUS}
    esac
    return ${DIFF_STATUS}
}

update_blocklist_in_hg()
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
            echo "ERROR hg clone exited with a non-zero exit code: ${CLONE_STATUS}"
            return ${CLONE_STATUS}
        fi
    fi

    echo ${HG} -R ${REPODIR} pull
    ${HG} -R ${REPODIR} pull
    PULL_STATUS=$?
    if [ ${PULL_STATUS} != 0 ]; then
        echo "ERROR hg pull exited with a non-zero exit code: ${PULL_STATUS}"
        return ${PULL_STATUS}
    fi
    echo ${HG} -R ${REPODIR} update -C default
    ${HG} -R ${REPODIR} update -C default
    UPDATE_STATUS=$?
    if [ ${UPDATE_STATUS} != 0 ]; then
        echo "ERROR hg update exited with a non-zero exit code: ${UPDATE_STATUS}"
        return ${UPDATE_STATUS}
    fi

    cp -f blocklist_amo.xml ${REPODIR}/${APP_DIR}/app/blocklist.xml
    COMMIT_MESSAGE="No bug, Automated blocklist update from host $HOST"
    if [ ${DONTBUILD} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - (DONTBUILD)"
    fi
    if [ ${CLOSED_TREE} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - CLOSED TREE"
    fi
    if [ ${APPROVAL} == true ]; then
        COMMIT_MESSAGE="${COMMIT_MESSAGE} - a=blocklist-update"
    fi
    echo ${HG} -R ${REPODIR} commit -u \"${HG_SSH_USER}\" -m \"${COMMIT_MESSAGE}\"
    ${HG} -R ${REPODIR} commit -u "${HG_SSH_USER}" -m "${COMMIT_MESSAGE}"
    echo ${HG} -R ${REPODIR} push -e \"ssh -l ${HG_SSH_USER} -i ${HG_SSH_KEY}\" ${HGPUSHREPO}
    ${HG} -R ${REPODIR} push -e "ssh -l ${HG_SSH_USER} -i ${HG_SSH_KEY}" ${HGPUSHREPO}
    PUSH_STATUS=$?
    if [ ${PUSH_STATUS} != 0 ]; then
        echo "ERROR hg push exited with exit code: ${PUSH_STATUS}, probably raced another changeset"
        echo ${HG} -R ${REPODIR} rollback
        ${HG} -R ${REPODIR} rollback
        ROLLBACK_STATUS=$?
        if [ ${ROLLBACK_STATUS} != 0 ]; then
            echo "ERROR hg rollback failed with exit code: ${ROLLBACK_STATUS}"
            echo "This is unrecoverable, removing the local clone to start fresh next time."
            rm -rf ${REPODIR}
            return ${ROLLBACK_STATUS}
        fi
    fi
    return ${PUSH_STATUS}
}

compare_blocklists
result=$?
if [ ${result} != 0 ]; then
    if [ "${DRY_RUN}" == "true" ]; then
        echo "Blocklist files differ, but not updating hg in dry-run mode."
    else   
        echo "Blocklist files differ, updating hg."
        update_blocklist_in_hg
        result=$?
    fi
else
    echo "Blocklist files are identical. Nothing to update."
fi
exit ${result}
