#!/usr/bin/env python

import os
import signal
import sys
import glob

import site
site.addsitedir(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../lib/python"))

from mozdevice import devicemanagerSUT as devicemanager
from sut_lib import log

ip_addr = None

def main(argv):
    global ip_addr

    if (len(argv) < 4):
        log.info("usage: logcat.py <device ip address> <output filename> <logcat options>")
        return 1
    ip_addr = argv[1]
    output_filename = argv[2]
    logcat_options = argv[3]
    max_runtime = 3600   # 3600 seconds == 1 hour

    status = 0
    dm = devicemanager.DeviceManagerSUT(ip_addr)
    if not dm:
        log.error("logcat.py: unable to open device manager")
        return 2
    command = 'execext su t=%d logcat %s' % (max_runtime, logcat_options)
    log.debug('logcat.py running SUT command: %s' % command)
    try:
        with open(output_filename, 'w') as f:
            dm._sendCmds([{'cmd': command}], f)
    except devicemanager.DMError, e:
        log.error("Remote Device Error: Exception caught running logcat: %s" % str(e))
        status = -1
    cleanup()
    return status

def findpid(dm, pname):

    # Execute ps on the device and parse the output to find the process id
    # associated with the specified process name.
    #
    # Expected ps format:
    #
    # USER     PID   PPID  VSIZE  RSS     WCHAN    PC         NAME
    # root      1     0     296    280   ffffffff 00000000 S /oldinit

    ps = dm.shellCheckOutput(['ps'])
    rows = ps.split('\r\n')
    for row in rows:
        columns = filter(lambda x: x != '', row.split(' '))
        if len(columns) > 8:
            pid = columns[1]
            name = columns[8]
            if name == pname:
                return int(pid)
    return None

def cleanup():
    # Kill remote logcat process. Most process operations in devicemanager
    # work on applications, like org.mozilla.fennec, but not on native
    # processes like logcat. We work around that by directly executing
    # ps and kill.
    dm = devicemanager.DeviceManagerSUT(ip_addr)
    pid = findpid(dm, "logcat")
    if pid and pid > 0:
        log.debug('logcat.py killing logcat with pid %d' % pid)
        try:
            dm.shellCheckOutput(['kill', str(pid)], root=True)
        except devicemanager.DMError, e:
            log.error("Error killing logcat (pid %s): %s" % (str(pid), str(e)))

def handlesig(signum, frame):
    log.debug('logcat.py received SIGINT; cleaning up and exiting...')

    # restore the original signal handler
    signal.signal(signal.SIGINT, original_sigint)

    cleanup()
    sys.exit(0)

if __name__ == '__main__':
    original_sigint = signal.getsignal(signal.SIGINT)
    signal.signal(signal.SIGINT, handlesig)
    sys.exit(main(sys.argv))
