#!/bin/bash -eu
export PATH='/usr/local/bin:/usr/local/bin:/usr/local/bin:/bin:/usr/bin:/usr/local/sbin:/usr/sbin:/sbin:/home/cltbld/bin'
PYTHON=`which python`

# MAGIC NUMBERS (global)
# used to determine how long we sleep when...
SUCCESS_WAIT=200 # ... seconds after we startup buildbot
FAIL_WAIT=500 # ... seconds after we stop buildbot due to error.flg

log() {
  echo "$(date +"%Y-%m-%d %H:%M:%S") -- ${1}" >&2
}
death() {
  log "*** ERROR *** ${1}"
  exit "${2}"
}

function check_buildbot_running() {
  # returns success if running
  #         !0 if not running
  local device=$1
  if [ ! -f /builds/$device/twistd.pid ]; then
     return 1
  fi
  local expected_pid=`cat /builds/$device/twistd.pid`
  log "buildbot pid is $expected_pid"
  kill -0 $expected_pid >/dev/null 2>&1
  return $?
}

function device_check_exit() {
  local device="${1}"
  rm -f "/builds/${device}/watcher.lock"
  log "Cycle for our device (${device}) complete" >>"/builds/${device}/watcher.log" 2>&1
}

function device_check() {
  local device=$1
  export PYTHONPATH=/builds/sut_tools
  deviceIP="$(nslookup "${device}" 2>/dev/null | sed -n '/^Name/,$s/^Address: *//p')"
  # 43200s = 12 hours - this is when stale lockfiles will get cleaned up
  if ! lockfile -l 43200 -r0 "/builds/${device}/watcher.lock" >/dev/null 2>&1; then
    death "failed to aquire lockfile" 67
  fi
  log_message="Starting cycle for our device ($device = $deviceIP) now"
  log "##${log_message//?/#}##"
  log "# ${log_message} #"
  log "##${log_message//?/#}##"
  # Trap here, not earlier so that if lockfile fails, we don't clear the lock
  # From another process
  trap "device_check_exit $device" EXIT
  log "contacting slavealloc"
  if ! check_buildbot_running "${device}"; then
    rm -f "/builds/${device}/disabled.flg"
    log "Buildbot is not running"
    /builds/tools/buildfarm/mobile/manage_buildslave.sh gettac $device
    if grep -q "SLAVE DISABLED" /builds/$device/buildbot.tac; then
      death "Not Starting, slavealloc says we're disabled" 64
    fi
    if [ -f /builds/$device/error.flg ]; then
      log "error.flg file detected"
      local contents=`cat /builds/$device/error.flg`
      log "error.flg contents: $contents"
      # Clear flag if older than an hour
      if [ `find /builds/$device/error.flg -mmin +60` ]; then
        log "removing $device error.flg (older than an hour) and trying again"
        rm -f /builds/$device/error.flg
      else
        death "Error flag less than an hour old, so exiting" 65
      fi
    fi
    export SUT_NAME=$device
    export SUT_IP=$deviceIP
    if ! "${PYTHON}" /builds/sut_tools/verify.py --success-if-mozpool-ready $device; then
      log "Verify procedure failed"
      if [ ! -f /builds/$device/error.flg ]; then
        log "error.flg file does not exist, so creating it..."
        echo "Unknown verify failure" | tee "/builds/$device/error.flg"
      fi
      death "Exiting due to verify failure" 66
    fi
    log "starting buildbot slave"
    /builds/tools/buildfarm/mobile/manage_buildslave.sh start $device
    log "Sleeping for ${SUCCESS_WAIT} sec after startup, to prevent premature flag killing"
    sleep ${SUCCESS_WAIT} # wait a bit before checking for an error flag or otherwise
  else # buildbot running
    log "(heartbeat) buildbot is running"
    if [ -f /builds/$device/error.flg ]; then
      log "Found an error.flg, expecting buildbot to self-kill after this job"
      local contents=`cat /builds/$device/error.flg`
      log "error.flg contents: $contents"
    fi
    if [ -f /builds/$device/disabled.flg ]; then
      log "disabled.flg wants us to force kill buildbot..."
      set +e # These steps are ok to fail, not a great thing but not critical
      log "Stopping device $device..."
      "${PYTHON}" /builds/sut_tools/stop.py --device $device
      # Stop.py should really do foopy cleanups and not touch device
      log "Attempting cleanup of device $device..."
      SUT_NAME=$device python /builds/sut_tools/cleanup.py $device
      set -e
      log "sleeping for ${FAIL_WAIT} seconds after killing, to prevent startup before master notices"
      sleep ${FAIL_WAIT} # Wait a while before allowing us to turn buildbot back on
    fi
  fi
  # Force disable only once. If we got this far, then we did all that we must.
  # Don't act on it next cycle.
  rm -f "/builds/${device}/disabled.flg"
}


function watch_launcher(){
  log "STARTING Watcher"
  ls -d /builds/panda-*[0-9] 2>/dev/null | sed 's:.*/::' | while read device; do
    log "..checking $device"
    "${0}" "${device}" &
  done
  log "Watcher completed."
}

# SCRIPT ENTRY POINT HERE...

if [ "$#" -eq 0 ]; then
  watch_launcher 2>&1 | tee -a "/builds/watcher.log"
else
  device="${1}"
  device_check "${device}" >>"/builds/${device}/watcher.log" 2>&1
fi
