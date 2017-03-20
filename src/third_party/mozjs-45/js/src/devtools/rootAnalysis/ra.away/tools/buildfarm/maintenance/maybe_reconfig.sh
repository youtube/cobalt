#!/bin/bash

function usage () {
    echo
    echo "This script is typically used via cron on a buildbot master."
    echo "It updates the buildbot-configs and buildbotcustom repos on disk,"
    echo "and then kicks off a reconfig if the production(-0.8) tag for"
    echo "either repo has moved. It also updates the tools repo on disk."
    echo
    echo "USAGE"
    echo "    $(basename "${0}") /path/to/buildbot-master"
    echo
}

function reconfig_needed () {
    is_reconfig_needed=1
    for d in buildbotcustom buildbot-configs tools; do
        tag="production"
        original_production_rev=""
        current_production_rev=""
        if [ "${d}" == "buildbotcustom" ]; then
            tag="production-0.8"
        elif [ "${d}" == "tools" ]; then
            tag="default"
        fi
        # Find the current production rev
        original_production_rev=$(${HG} -R "${d}" heads ${tag} -T '{node}\n')
        timeout 5m ${HG} -R "${d}" pull > /dev/null
        if [ "$?" != "0" ]; then
            error_msg="hg pull failed for ${d}"
            log_error "${error_msg}"
            exit 6
        fi
        current_production_rev=$(${HG} -R "${d}" heads ${tag} -T '{node}\n')
        if [ "${original_production_rev}" != "${current_production_rev}" ]; then
            log_info "${d}: ${tag} tag has moved - old rev: ${original_production_rev}; new rev: ${current_production_rev}"
            timeout 5m ${HG} -R "${d}" update -r ${tag} >> ${LOGFILE}
            if [ "$?" != "0" ]; then
                error_msg="hg update failed for ${d} with tag ${tag}"
                log_error "${error_msg}"
                exit 7
            fi
            # Changes to the tools repo don't trigger a reconfig.
            if [ "${d}" != "tools" ]; then
                is_reconfig_needed=0
            fi
        else
            log_info "${d}: ${tag} tag is unchanged - rev: ${original_production_rev}"
        fi
    done
    return ${is_reconfig_needed}
}

function finish () {
    STOP_TIME="$(date +%s)"
    ELAPSED_TIME=$((STOP_TIME - START_TIME))
    ELAPSED_DISPLAY=`show_time ${ELAPSED_TIME}`
    log_info "Elapsed: ${ELAPSED_DISPLAY}"
    log_info "=================================================="
    rm -f ${LOCKFILE}
}

MASTER_DIR=$1
START_TIME="$(date +%s)"
HG=/usr/local/bin/hg

if [ "${MASTER_DIR}" == "" ]; then
    usage >&2
    exit 1
fi

if [ ! -d ${MASTER_DIR} ]; then
    echo "ERROR: Master dir ${MASTER_DIR} not found." >&2
    exit 2
fi

# Include our shared function library
. ${MASTER_DIR}/tools/lib/shell/functions

# Check to see if a reconfig is already in progress. Bail if one is.
LOCKFILE=${MASTER_DIR}/reconfig.lock
LOGFILE=${MASTER_DIR}/reconfig.log

# Check whether we should rotate our log file.
if [ -e ${LOGFILE} ]; then
    /usr/sbin/logrotate -s /tmp/reconfig-logrotate.status ${MASTER_DIR}/tools/buildfarm/maintenance/reconfig-logrotate.conf
fi

log_info "Checking whether we need to reconfig..."

OUTPUT=`lockfile -5 -r 1 ${LOCKFILE} 2>&1`
LOCKFILE_MAX_AGE=120 # Reconfigs should not take more than 2 hours (120 min)
if [ "${OUTPUT}" != "" ]; then
    if [ -e ${LOCKFILE} ]; then
        # Check the age of the lockfile.
        if [ "$(find ${LOCKFILE} -mmin +${LOCKFILE_MAX_AGE})" ]; then
            # If the lockfile is older than is acceptable, log the error, but also print
            # the error to STDOUT so cronmail will trigger.
            error_msg="Reconfig lockfile is older than ${LOCKFILE_MAX_AGE} minutes."
            log_error "${error_msg}"
        else
            # This is an acceptable state.
            log_info "Lockfile ${LOCKFILE} exists. Assuming reconfig is still in process. Exiting."
            exit 0
        fi
    else
        log_error "lockfile output: $OUTPUT"
    fi
    exit 3
fi
trap finish EXIT

# Main
pushd ${MASTER_DIR} > /dev/null

# Activate our venv
if [ ! -e bin/activate ]; then
    log_error "activate script not found: are you sure this is a buildbot venv?"
    exit 4
fi
source bin/activate

if reconfig_needed; then
    # We append the START_TIME to the reconfig milestone messages to make it easier to match up milestones
    # (start/finish) from a single reconfig event.
    log_info "Starting reconfig. - ${START_TIME}"
    make checkconfig >> ${LOGFILE} 2>&1
    RC=$?
    if [ "${RC}" == "0" ]; then
        python ./tools/buildfarm/maintenance/buildbot-wrangler.py reconfig ${MASTER_DIR}/master >> ${LOGFILE}
        RC=$?
    else
        log_error "Checkconfig failed."
    fi
    if [ "${RC}" == "0" ]; then
        log_info "Reconfig completed successfuly. - ${START_TIME}"
    else
        log_error "Reconfig failed. - ${START_TIME}"
        exit 5
    fi
else
    log_info "No reconfig required."
fi

exit 0
