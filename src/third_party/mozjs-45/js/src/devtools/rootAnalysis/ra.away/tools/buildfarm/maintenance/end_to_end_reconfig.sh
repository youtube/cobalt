#!/bin/bash -eu

################### TO DO ###################
#
# * Check hg is installed
# * Check hg clone is working (error code 255)
# * Check virtualenv is installed
# * Add an option not to refresh tools repo
# * Check connectivity to an arbitrary master and report nicely
# * Summarize failures from manage_masters.py at end of log
# * Option to checkconfig only without update
# * Only update wiki if there really are changes
# * Only reconfig if there really are changes, or if forced
# * Send console output also to a log file
#
#############################################

# Include our shared function library
if [ -e ../../lib/shell/functions ]; then
    . ../../lib/shell/functions
else
    echo "Shared functions missing: ../../lib/shell/functions" >&2
    exit 1
fi

START_TIME="$(date +%s)"

# Explicitly unset any pre-existing environment variables to avoid variable collision
unset PREPARE_ONLY FORCE_RECONFIG MERGE_TO_PRODUCTION UPDATE_BUGZILLA UPDATE_WIKI RECONFIG_DIR RECONFIG_CREDENTIALS_FILE WIKI_USERNAME WIKI_PASSWORD

# Force python into unbuffered mode, so we get full output, as it happens from the fabric scripts
export PYTHONUNBUFFERED=1

function usage {
    echo
    echo "This script can be used to reconfig interactively, or non-interactively. It will merge"
    echo "buildbotcustom, buildbot-configs, mozharness from default to production(-0.8)."
    echo "It will then reconfig the buildbot masters, update the foopies with the latest tools"
    echo "changes, and afterwards if all was successful, it will also update the wiki page"
    echo "https://wiki.mozilla.org/ReleaseEngineering/Maintenance and additionally post comments"
    echo "on the Bugzilla bugs it finds in the commit messages of merged changes, providing a link"
    echo "to the hg web interface of the commits that have been deployed to production."
    echo
    echo "USAGE"
    echo "     1) $(basename "${0}") -h"
    echo "     2) $(basename "${0}") [-b] [-f] [-m] [-n] [-p] [-r RECONFIG_DIR] [-t] [-w RECONFIG_CREDENTIALS_FILE]"
    echo
    echo "OPTIONS"
    echo "    -b:                            No posting of comments to Bugzilla."
    echo "    -f:                            Force reconfig, even if no changes merged."
    echo "    -h:                            Display help."
    echo "    -m:                            No merging of default -> production(-0.8) of hg branches."
    echo "    -n:                            No wiki update."
    echo "    -p:                            Prepare only; does not push changes to hg, nor perform"
    echo "                                   reconfig, nor update wiki, nor post to Bugzilla. Useful"
    echo "                                   for validating setup, or resolving merge conflicts in"
    echo "                                   advance early rather than waiting until real reconfig to"
    echo "                                   resolve conflicts."
    echo "    -r RECONFIG_DIR:               Use directory RECONFIG_DIR for storing temporary files"
    echo "                                   (default is /tmp/reconfig). This directory, and any"
    echo "                                   necessary parent directories will be created if required."
    echo "    -w RECONFIG_CREDENTIALS_FILE:  Source environment variables: BUGZILLA_USERNAME,"
    echo "                                   BUGZILLA_PASSWORD, WIKI_USERNAME, WIKI_PASSWORD from file"
    echo "                                   RECONFIG_CREDENTIALS_FILE (default is ~/.reconfig/config)."
    echo
    echo "EXIT CODES"
    echo "     0        Success"
    echo "     1        Error loading shared functions"
    echo "     2        Bad command line options specified"
    echo "    64        Could not create directory to store results (RECONFIG_DIR)"
    echo "    65        Wiki credentials file not found"
    echo "    66        Python 2.7 not found in PATH"
    echo "    67        Reconfig aborted by user after incomplete reconfig found"
    echo "    68        Aborted due to incomplete reconfig found but non-interactive session, so"
    echo "              could not ask user how to proceed"
    echo "    69        Error during hg merge (most likely due to merge conflict(s))"
    echo "    70        Theoretically not possible - update_maintenance_wiki.sh tries and fails"
    echo "              to create the RECONFIG_DIR when it detects it does not exist (but should"
    echo "              have already been created by end_to_end_reconfig.sh)"
    echo "    71        Theoretically not possible - no wiki markdown file passed to"
    echo "              update_maintenance_wiki.sh"
    echo "    72        Theoretically not possible - non-existing markdown file passed to"
    echo "              update_maintenance_wiki.sh"
    echo "    73        WIKI_USERNAME not specified in reconfig credentials file"
    echo "    74        WIKI_PASSWORD not specified in reconfig credentials file"
    echo "    75        Could not retreieve login token from wiki - probably a connectivity issue"
    echo "    76        Wiki user/password not authorized to update wiki page"
    echo "              https://wiki.mozilla.org/ReleaseEngineering/Maintenance"
    echo "    77        Wiki API provided a reason not to supply an edit token for the wiki page,"
    echo "              which is something other than user/password not authorized (which would"
    echo "              result in exit code 76)"
    echo "    78        Wiki API response to request to provide an edit token produced a"
    echo "              non-parsable response"
    echo "    79        BUGZILLA_USERNAME not specified in reconfig credentials file"
    echo "    80        BUGZILLA_PASSWORD not specified in reconfig credentials file"
    echo "    81        Bugzilla user/password not authorized to update Bugzilla"
    echo "    82        Could not install python packages into a virtualenv"
    echo
}

# Simple function to output the name of this script and the options that were passed to it
function command_called {
    echo -n "Command called:"
    for ((INDEX=0; INDEX<=$#; INDEX+=1))
    do
        echo -n " '${!INDEX}'"
    done
    echo ''
    echo "From directory: '$(pwd)'"
}

# writes hg commands to the hg log, and just summarizes command called in main log
function hg_wrapper {
    if [ $1 == 'clone' ]; then
        command=('hg' "${@}" "${RECONFIG_DIR}/${repo}")
    else
        command=('hg' -R "${RECONFIG_DIR}/${repo}" "${@}")
    fi
    command_full="Running:"
    for ((INDEX=0; INDEX<${#command[@]}; INDEX+=1)); do
        command_full="${command_full} '${command[$INDEX]}'"
    done
    echo "  * ${command_full}"
    {
        echo
        echo "$(date): ${command_full}"
        echo
        HG_START="$(date +%s)"
        set +e
        "${command[@]}"
        HG_RETURN_CODE=$?
        set -e
        HG_STOP="$(date +%s)"
        echo
        echo "$(date): Completed ($((HG_STOP - HG_START))s)"
    } >>"${RECONFIG_DIR}/hg-${START_TIME}.log" 2>&1
    return "${HG_RETURN_CODE}"
}

set +u
command_called "${@}" | sed '1s/^/  * /;2s/^/    /'
set -u

echo "  * Start timestamp: ${START_TIME}"
echo "  * Parsing parameters of $(basename "${0}")..."
# Parse parameters passed to this script
while getopts ":bfhmnpr:w:" opt; do
    case "${opt}" in
        b)  UPDATE_BUGZILLA=0
            ;;
        f)  FORCE_RECONFIG=1
            ;;
        h)  echo "  * Help option requested"
            usage
            exit 0
            ;;
        m)  MERGE_TO_PRODUCTION=0
            ;;
        n)  UPDATE_WIKI=0
            ;;
        p)  PREPARE_ONLY=1
            ;;
        r)  RECONFIG_DIR="${OPTARG}"
            ;;
        w)  RECONFIG_CREDENTIALS_FILE="${OPTARG}"
            ;;
        ?)  usage >&2
            exit 2
            ;;
    esac
done

echo "  * Setting defaults for parameters not provided in command line options..."

PREPARE_ONLY="${PREPARE_ONLY:-0}"
FORCE_RECONFIG="${FORCE_RECONFIG:-0}"
MERGE_TO_PRODUCTION="${MERGE_TO_PRODUCTION:-1}"
UPDATE_BUGZILLA="${UPDATE_BUGZILLA:-1}"
UPDATE_WIKI="${UPDATE_WIKI:-1}"
RECONFIG_DIR="${RECONFIG_DIR:-/tmp/reconfig}"
RECONFIG_CREDENTIALS_FILE="${RECONFIG_CREDENTIALS_FILE:-${HOME}/.reconfig/config}"

RECONFIG_UPDATE_FILE="reconfig_update_for_maintenance.wiki"

##### Now check parsed parameters are valid...

echo "  * Validating parameters..."

if [ "${PREPARE_ONLY}" == 0 ]; then
    echo "  * Will be preparing *and performing* reconfig, all being well"
else
    echo "  * Preparing reconfig only; will not enact changes"
fi

if [ ! -d "${RECONFIG_DIR}" ]; then
    echo "  * Storing reconfig output under '${RECONFIG_DIR}'..."
    if ! mkdir -p "${RECONFIG_DIR}"; then
        echo "ERROR: Directory '${RECONFIG_DIR}' could not be created from directory '$(pwd)'" >&2
        exit 64
    fi
else
    echo "  * Reconfig directory '${RECONFIG_DIR}' exists - OK"
fi

# Convert ${RECONFIG_DIR} to an absolute directory, in case it is relative, by stepping into it...
pushd "${RECONFIG_DIR}" >/dev/null
if [ "${RECONFIG_DIR}" != "$(pwd)" ]; then
    echo "  * Reconfig directory absolute path: '$(pwd)'"
fi
RECONFIG_DIR="$(pwd)"
popd >/dev/null

# To avoid user getting confused about parent directory, tell user the
# absolute path of the credentials file...
echo "  * Checking we have a reconfig credentials file..."
RECONFIG_CREDS_PARENT_DIR="$(dirname "${RECONFIG_CREDENTIALS_FILE}")"
if [ ! -e "${RECONFIG_CREDS_PARENT_DIR}" ]; then
echo "  * Reconfig credentials file parent directory '${RECONFIG_CREDS_PARENT_DIR}' not found; creating..."
    mkdir -p "${RECONFIG_CREDS_PARENT_DIR}"
fi
pushd "${RECONFIG_CREDS_PARENT_DIR}" >/dev/null
ABS_RECONFIG_CREDENTIALS_FILE="$(pwd)/$(basename "${RECONFIG_CREDENTIALS_FILE}")"
popd >/dev/null
if [ "${RECONFIG_CREDENTIALS_FILE}" == "${ABS_RECONFIG_CREDENTIALS_FILE}" ]; then
    echo "  * Reconfig credentials file location: '${RECONFIG_CREDENTIALS_FILE}'"
else
    echo "  * Reconfig credentials file location: '${RECONFIG_CREDENTIALS_FILE}' (absolute path: '${ABS_RECONFIG_CREDENTIALS_FILE}')"
fi
if [ ! -e "${ABS_RECONFIG_CREDENTIALS_FILE}" ]; then
    echo "  * Reconfig credentials file '${ABS_RECONFIG_CREDENTIALS_FILE}' not found; creating..."
    {
        echo "# Needed if updating wiki - note the wiki does *not* use your LDAP credentials..."
        echo "export WIKI_USERNAME='naughtymonkey'"
        echo "export WIKI_PASSWORD='nobananas'"
        echo
        echo "# Needed if updating Bugzilla bugs to mark them as in production - *no* Persona integration - must be a native Bugzilla account..."
        echo "# Details for the 'Release Engineering SlaveAPI Service' <slaveapi@mozilla.releng.tld> Bugzilla user can be found in the RelEng"
        echo "# private repo, in file passwords/slaveapi-bugzilla.txt.gpg (needs decrypting with an approved gpg key)."
        echo "export BUGZILLA_USERNAME='naughtymonkey'"
        echo "export BUGZILLA_PASSWORD='nobananas'"
    } > "${RECONFIG_CREDENTIALS_FILE}"
    echo "  * Created credentials file '${ABS_RECONFIG_CREDENTIALS_FILE}'. Please edit this file, setting appropriate values, then rerun."
    exit 65
else
    echo "  * Loading config from '${ABS_RECONFIG_CREDENTIALS_FILE}'..."
    source "${RECONFIG_CREDENTIALS_FILE}"
fi

# Now step into directory this script is in...
cd "$(dirname "${0}")"

# Test python version, and availability of fabric...
echo "  * Checking python version is 2.7..."
if ! python --version 2>&1 | grep -q '^Python 2\.7'; then
    echo "ERROR: Python version 2.7 not found - please make sure python 2.7 is in your PATH" >&2
    exit 66
fi

for package in fabric requests; do
    installed_package=false
    echo "  * Checking ${package} package is available in python environment..."
    if ! python -c "import ${package}" >/dev/null 2>&1; then
        echo "  * Package ${package} not found"
        if [ ! -e "${RECONFIG_DIR}/reconfig-virtual-env" ]; then
            echo "  * Creating virtualenv directory '${RECONFIG_DIR}/reconfig-virtual-env' for reconfig tool..."
            echo "  * Logging to: '${RECONFIG_DIR}/virtualenv-installation.log'..."
            virtualenv "${RECONFIG_DIR}/reconfig-virtual-env" >"${RECONFIG_DIR}/virtualenv-installation.log" 2>&1
            set +u
            source "${RECONFIG_DIR}/reconfig-virtual-env/bin/activate"
            set -u
            echo "  * Installing ${package} under '${RECONFIG_DIR}/reconfig-virtual-env'..."
            pip install "${package}" >>"${RECONFIG_DIR}/virtualenv-installation.log" 2>&1
            installed_package=true
        else
            echo "  * Attempting to use existing virtualenv found in '${RECONFIG_DIR}/reconfig-virtual-env'..."
            set +u
            source "${RECONFIG_DIR}/reconfig-virtual-env/bin/activate"
            set -u
            if ! python -c "import ${package}" >/dev/null 2>&1; then
                echo "  * Package ${package} not found under virtualenv '${RECONFIG_DIR}/reconfig-virtual-env', installing..."
                pip install "${package}" >"${RECONFIG_DIR}/virtualenv-installation.log" 2>&1
                installed_package=true
            else
                echo "  * Using existing package ${package} found in '${RECONFIG_DIR}/reconfig-virtual-env'"
            fi
        fi
    else
        echo "  * Package ${package} found"
    fi
    if "${installed_package}"; then
        echo "  * Re-checking if package ${package} is now available in python environment..."
        if ! python -c "import ${package}" >/dev/null 2>&1; then
            echo "ERROR: Could not successfully install package ${package} into python virtualenv '${RECONFIG_DIR}/reconfig-virtual-env'" >&2
            echo "Exiting..." >&2
            exit 82
        else
            echo "  * Package ${package} installed successfully into python environment"
        fi
    fi
done

if [ "${UPDATE_BUGZILLA}" == '1' ]; then
    echo "  * Updates to Bugzilla enabled"
    echo "  * Testing existence of Bugzilla login credentials..."
    if [ -z "${BUGZILLA_USERNAME}" ]; then
        echo "ERROR: Environment variable BUGZILLA_USERNAME must be set for posting comments to https://bugzilla.mozilla.org/" >&2
        echo "Exiting..." >&2
        exit 79
    else
        echo "  * Environment variable BUGZILLA_USERNAME defined ('${BUGZILLA_USERNAME}')"
    fi
    if [ -z "${BUGZILLA_PASSWORD}" ]; then
        echo "ERROR: Environment variable BUGZILLA_PASSWORD must be set for posting comments to https://bugzilla.mozilla.org/" >&2
        echo "Exiting..." >&2
        exit 80
    else
        echo "  * Environment variable BUGZILLA_PASSWORD defined"
    fi
fi

if [ "${UPDATE_WIKI}" == '1' ]; then
    echo "  * Wiki update enabled"
    # Now validate wiki credentials by performing a dry run...
    echo "  * Testing login credentials for wiki..."
    ./update_maintenance_wiki.sh -d
else
    echo "  * Not updating wiki"
fi

if [ "${UPDATE_BUGZILLA}" == '1' ]; then
    echo "  * Testing Bugzilla login as user '${BUGZILLA_USERNAME}'..."
    BUGZILLA_LOGIN_RESPONSE="$(mktemp -t bugzilla_login.XXXXXXXXXX)"
    curl -s -G --data-urlencode "login=${BUGZILLA_USERNAME}" --data-urlencode "password=${BUGZILLA_PASSWORD}" 'https://bugzilla.mozilla.org/rest/login' > "${BUGZILLA_LOGIN_RESPONSE}"
    if grep -q '"token":' "${BUGZILLA_LOGIN_RESPONSE}"; then
        echo "  * Login to Bugzilla successful"
    else
        echo "ERROR: Login to Bugzilla failed - response given:" >&2
        cat "${BUGZILLA_LOGIN_RESPONSE}" | sed 's/^/    /' >&2
        rm "${BUGZILLA_LOGIN_RESPONSE}"
        echo >&2
        echo "Please check values of BUGZILLA_USERNAME and BUGZILLA_PASSWORD set in this file: '${RECONFIG_CREDENTIALS_FILE}'" >&2
        echo "Exiting..." >&2
        exit 81
    fi
    rm "${BUGZILLA_LOGIN_RESPONSE}"
else
    echo "  * Not updating Bugzilla"
fi

# Check if a previous reconfig did not complete
if [ -f "${RECONFIG_DIR}/pending_changes" ]; then
    echo "  * It looks like a previous reconfig did not complete"
    echo "  * Checking if 'standard in' is connected to a terminal..."
    if [ -t 0 ]; then
        # 'standard in' is connected to a terminal, can ask user a question!
        echo "  * Please select one of the following options:"
        echo "        1) Continue with existing reconfig (e.g. if you have resolved a merge conflict)"
        echo "        2) Delete saved state for existing reconfig, and start from fresh"
        echo "        3) Abort and exit reconfig process"
        choice=''
        while [ "${choice}" != 1 ] && [ "${choice}" != 2 ] && [ "${choice}" != 3 ]; do
            echo -n "    Your choice: "
            read choice
        done
        case "${choice}" in
            1) echo "  * Continuing with stalled reconfig..."
               ;;
            2) echo "  * Cleaning out previous reconfig from '${RECONFIG_DIR}'..."
               rm -rf "${RECONFIG_DIR}"/{buildbot-configs,buildbotcustom,mozharness,pending_changes,${RECONFIG_UPDATE_FILE}}
               ;;
            3) echo "  * Aborting reconfig..."
               exit 67
               ;;
        esac
    else
        # 'standard in' not connected to a terminal, assume no user connected...
        echo "  * Non-interactive shell detected, cannot ask whether to continue or not, therefore aborting..."
        exit 68
    fi
fi

### If we get this far, all our preflight checks have passed, so now on to business...
echo "  * All preflight checks passed in '$(basename "${0}")'"

if [ "${PREPARE_ONLY}" == '0' ]; then
    # Failing to update IRC is non-fatal.
    IRC_MSG="Merge to production has started for buildbot repos."
    if [ "${FORCE_RECONFIG}" == '1' ]; then
        IRC_MSG="Reconfig has started."
    fi
    ./update_irc.sh "${IRC_MSG}" || true
fi

# Merges mozharness, buildbot-configs from default -> production.
# Merges buildbostcustom from default -> production-0.8.
# Returns 0 if something got merged, otherwise returns 1.
function merge_to_production {
    [ "${MERGE_TO_PRODUCTION}" == 0 ] && return 0
    echo "  * hg log for this session: '${RECONFIG_DIR}/hg-${START_TIME}.log'"
    # Create an empty merge file to compare against so we don't claim merge success when nothing was merged.
    {
        echo "Merging from default"
        echo
    } > "${RECONFIG_DIR}/empty_merge"
    for repo in mozharness buildbot-configs buildbotcustom; do
        if [ -d "${RECONFIG_DIR}/${repo}" ]; then
            echo "  * Existing hg clone of ${repo} found: '${RECONFIG_DIR}/${repo}' - pulling for updates..."
            hg_wrapper pull
        else
            echo "  * Cloning ssh://hg.mozilla.org/build/${repo} into '${RECONFIG_DIR}/${repo}'..."
            hg_wrapper clone "ssh://hg.mozilla.org/build/${repo}"
        fi
        if [ "${repo}" == 'buildbotcustom' ]; then
            branch='production-0.8'
        else
            branch='production'
        fi
        hg_wrapper up -r "${branch}"
        echo "  * Finding ${repo} changesets that would get merged from default to ${branch}..."
        {
            echo "Merging from default"
            echo
            hg -R "${RECONFIG_DIR}/${repo}" merge -P default
        } >> "${RECONFIG_DIR}/${repo}_preview_changes.txt" 2>/dev/null
        # Merging can fail if there are no changes between default and "${branch}"
        set +e
        hg_wrapper merge default
        RETVAL="${?}"
        set -e
        if [ "${RETVAL}" == '255' ]; then
            echo "  * No new changes found in ${repo}"
        elif [ "${RETVAL}" != '0' ]; then
            echo "ERROR: An error occurred during hg merge (exit code was ${RETVAL}). Please resolve conflicts/issues in '${RECONFIG_DIR}/${repo}',"
            echo "       push to ${branch} branch, and run this script again." >&2
            exit 69
        else
            echo "  * ${repo} merge resulted in change"
        fi
        # Still commit and push, even if no new changes, since a merge conflict might have been resolved
        hg_wrapper commit -l "${RECONFIG_DIR}/${repo}_preview_changes.txt"
        if [ "${PREPARE_ONLY}" == '0' ]; then
            echo "  * Pushing '${RECONFIG_DIR}/${repo}' ${branch} branch to ssh://hg.mozilla.org/build/${repo}..."
            hg_wrapper push
        fi
        if ! diff "${RECONFIG_DIR}/empty_merge" "${RECONFIG_DIR}/${repo}_preview_changes.txt" > /dev/null; then
  	    echo "${repo}" >> "${RECONFIG_DIR}/pending_changes"
        fi
    done
    [ -f "${RECONFIG_DIR}/pending_changes" ] && return 0 || return 1
}

# Now we process the commit messages from all the changes we landed. This is handled by the python script
# process_commit_comments.py. We pass options to this script based on what steps are enabled (e.g. wiki
# updates, bugzilla updates). Create an array for this, and if it is empty at the end, we know we don't
# have to do anything (arrays are better than strings since they can contains spaces).
commit_processing_options=()

if [ "${UPDATE_WIKI}" == '1' ]; then
    commit_processing_options+=('--wiki-markup-file' "${RECONFIG_DIR}/${RECONFIG_UPDATE_FILE}")
fi
if [ "${UPDATE_BUGZILLA}" == '1' ] && [ "${PREPARE_ONLY}" == '0' ]; then
    commit_processing_options+=('--update-bugzilla')
fi

# Display the commands in advance in case the reconfig fails part way through.
if [ "${#commit_processing_options[@]}" -gt 0 ]; then
    echo "  * Providing post-processing commands in case reconfig aborts."
    echo -n "  * Will run '$(pwd)/process_commit_comments.py' --logdir '${RECONFIG_DIR}'"
    for ((i=0; i<${#commit_processing_options[@]}; i+=1)); do
        echo -n " '${commit_processing_options[${i}]}'"
    done
    echo
    echo "  * Will run: './update_maintenance_wiki.sh -r \"${RECONFIG_DIR}\" -w \"${RECONFIG_DIR}/${RECONFIG_UPDATE_FILE}\"'"
fi

# Return code of merge_to_production is 0 if merge performed successfully and changes made
if merge_to_production; then
    echo "  * All changes merged to production."
fi

if [ "${FORCE_RECONFIG}" == '1' ]; then
    production_masters_url='http://hg.mozilla.org/build/tools/raw-file/tip/buildfarm/maintenance/production-masters.json'
    if [ "${PREPARE_ONLY}" != '0' ]; then
        echo "  * Preparing reconfig only; not running: '$(pwd)/manage_masters.py' -f '${production_masters_url}' -j16 -R scheduler -R build -R try -R tests show_revisions update"
        echo "  * Preparing reconfig only; not running: '$(pwd)/manage_masters.py' -f '${production_masters_url}' -j32 -R scheduler -R build -R try -R tests checkconfig reconfig"
        echo "  * Preparing reconfig only; not running: '$(pwd)/manage_masters.py' -f '${production_masters_url}' -j16 -R scheduler -R build -R try -R tests show_revisions"
    else
        echo "  * Fabric log for buildbot masters: '${RECONFIG_DIR}/manage_masters-${START_TIME}.log'"
        # Split into two steps so -j option can be varied between them
        echo "  * Running: '$(pwd)/manage_masters.py' -f '${production_masters_url}' -j16 -R scheduler -R build -R try -R tests show_revisions update"
        ./manage_masters.py -f "${production_masters_url}" -j16 -R scheduler -R build -R try -R tests show_revisions update >>"${RECONFIG_DIR}/manage_masters-${START_TIME}.log" 2>&1
        echo "  * Running: '$(pwd)/manage_masters.py' -f '${production_masters_url}' -j32 -R scheduler -R build -R try -R tests checkconfig reconfig"
        ./manage_masters.py -f "${production_masters_url}" -j32 -R scheduler -R build -R try -R tests checkconfig reconfig >>"${RECONFIG_DIR}/manage_masters-${START_TIME}.log" 2>&1
        # delete this now, since changes have been deployed
        [ -f "${RECONFIG_DIR}/pending_changes" ] && mv "${RECONFIG_DIR}/pending_changes" "${RECONFIG_DIR}/pending_changes_${START_TIME}"
        echo "  * Running: '$(pwd)/manage_masters.py' -f '${production_masters_url}' -j16 -R scheduler -R build -R try -R tests show_revisions"
        ./manage_masters.py -f "${production_masters_url}" -j16 -R scheduler -R build -R try -R tests show_revisions >>"${RECONFIG_DIR}/manage_masters-${START_TIME}.log" 2>&1
        echo "  * Reconfig of masters completed"
    fi
fi

if [ "${#commit_processing_options[@]}" -gt 0 ]; then
    echo -n "  * Running '$(pwd)/process_commit_comments.py' --logdir '${RECONFIG_DIR}'"
    for ((i=0; i<${#commit_processing_options[@]}; i+=1)); do echo -n " '${commit_processing_options[${i}]}'"; done
    echo
    ./process_commit_comments.py --logdir "${RECONFIG_DIR}" "${commit_processing_options[@]}"
else
    echo "  * Skipping commit messages processing step, since no wiki update nor Bugzilla update enabled, so not required"
fi

if [ -f "${RECONFIG_DIR}/${RECONFIG_UPDATE_FILE}" ]; then
    if [ "${UPDATE_WIKI}" == "1" ]; then
        if [ "${PREPARE_ONLY}" != '0' ]; then
            echo "  * Running: './update_maintenance_wiki.sh -d -r \"${RECONFIG_DIR}\" -w \"${RECONFIG_DIR}/${RECONFIG_UPDATE_FILE}\"'"
            ./update_maintenance_wiki.sh -d -r "${RECONFIG_DIR}" -w "${RECONFIG_DIR}/${RECONFIG_UPDATE_FILE}"
        else
            echo "  * Running: './update_maintenance_wiki.sh -r \"${RECONFIG_DIR}\" -w \"${RECONFIG_DIR}/${RECONFIG_UPDATE_FILE}\"'"
            ./update_maintenance_wiki.sh -r "${RECONFIG_DIR}" -w "${RECONFIG_DIR}/${RECONFIG_UPDATE_FILE}"
            for file in "${RECONFIG_DIR}"/*_preview_changes.txt
            do
                mv "${file}" "$(echo "${file}" | sed "s/\\.txt\$/_${START_TIME}&/")"
            done 2>/dev/null || true
            RECONFIG_STOP_TIME="$(date +%s)"
            RECONFIG_ELAPSED=$((RECONFIG_STOP_TIME - START_TIME))
            RECONFIG_ELAPSED_DISPLAY=`show_time ${RECONFIG_ELAPSED}`
            IRC_MSG="Merge has finished in ${RECONFIG_ELAPSED_DISPLAY}. See http://bit.ly/reconfigs for details. Masters will reconfig themselves automatically on the hour."
            if [ "${FORCE_RECONFIG}" == '1' ]; then
                IRC_MSG="Reconfig has finished in ${RECONFIG_ELAPSED_DISPLAY}. See http://bit.ly/reconfigs for details."
            fi
            # Failing to update IRC is non-fatal.
            ./update_irc.sh "${IRC_MSG}" || true
        fi
    fi

    echo "  * Summary of changes:"
    cat "${RECONFIG_DIR}/${RECONFIG_UPDATE_FILE}" | sed 's/^/        /'
fi

# Manage foopies after everything else.
# No easy way to see if there are changes to the tools repo since there is no production branch
# and we do not tag it - so aside from running show_revision and then checking if every version
# is already on the latest commit, we should probably just run it regardless.
devices_json_url='http://hg.mozilla.org/build/tools/raw-file/tip/buildfarm/mobile/devices.json'
if [ "${PREPARE_ONLY}" != '0' ]; then
    echo "  * Preparing foopy update only; not running: '$(pwd)/manage_foopies.py' -f '${devices_json_url}' -j16 -H all show_revision update"
    echo "  * Preparing foopy update only; not running: '$(pwd)/manage_foopies.py' -f '${devices_json_url}' -j16 -H all show_revision"
else
    echo "  * Fabric log for foopies: '${RECONFIG_DIR}/manage_foopies-${START_TIME}.log'"
    echo "  * Running: '$(pwd)/manage_foopies.py' -f '${devices_json_url}' -j16 -H all show_revision update"
    ./manage_foopies.py -f "${devices_json_url}" -j16 -H all show_revision update >>"${RECONFIG_DIR}/manage_foopies-${START_TIME}.log" 2>&1
    echo "  * Running: '$(pwd)/manage_foopies.py' -f '${devices_json_url}' -j16 -H all show_revision"
    ./manage_foopies.py -f "${devices_json_url}" -j16 -H all show_revision >>"${RECONFIG_DIR}/manage_foopies-${START_TIME}.log" 2>&1
    echo "  * Foopies updated"
fi

echo "  * Directory '${RECONFIG_DIR}' contains artefacts from reconfig process"

STOP_TIME="$(date +%s)"
ELAPSED_TIME=$((STOP_TIME - START_TIME))
ELAPSED_DISPLAY=`show_time ${ELAPSED_TIME}`
echo "  * Finish timestamp: ${STOP_TIME}"
echo "  * Time taken: ${ELAPSED_DISPLAY}"
