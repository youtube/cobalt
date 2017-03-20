#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
import sys
import os
import subprocess
import re

import site
site.addsitedir(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../lib/python"))

from mozdevice import devicemanagerSUT as devicemanager
from sut_lib import log, getSUTLogger
from installApp import installOneApp
from verify import verifyDevice
import time


def runTests(device, proc_name, number):
    numfailed = -1
    log.info("Starting to run tests")
    httpport = 8000 + int(number)

    # TODO: fix the host/port information so we don't have conflicts in
    # parallel runs
    cmd = ["python",
           os.path.join(os.path.dirname(__file__), "tests/mochitest/runtestsremote.py"),
           "--app=%s" % proc_name,
           "--deviceIP=%s" % device,
           "--xre-path=%s" % os.path.join(os.path.dirname(__file__), "xre"),
           "--utility-path=%s" % os.path.join(os.path.dirname(__file__), "bin"),
           # "--test-path=dom/tests/mochitest/dom-level0",
           "--test-path=dom/tests/mochitest",
           "--http-port=%s" % httpport]
    log.info("Going to run test: %s" % subprocess.list2cmdline(cmd))
    proc = ""
    try:
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        proc = p.communicate()[0]
    except:
        log.error("Exception found while running unittests: %s" % sys.exc_info()[1])
    log.info("Finished running mochitests")
    refinished = re.compile('([0-9]+) INFO SimpleTest FINISHED')
    refailed = re.compile('([0-9]+) INFO Failed: ([0-9]+)')
    for line in proc.split('\n'):
        log.debug(line)
        finished = refinished.match(line)
        failed = refailed.match(line)
        if failed:
            numfailed = int(failed.group(2))
        if finished:
            if numfailed > 0:
                log.error("Found %s failures while running mochitest"
                          % numfailed)
                return False
            return True
    return False


def smoketest(device_name, number):
    global dm
    global appFileName, processName

    dm = devicemanager.DeviceManagerSUT(device_name, 20701)
    deviceRoot = dm.getDeviceRoot()

    # This does all the steps of verify.py including the cleanup.
    if verifyDevice(device_name, checksut=False, doCheckStalled=False) is False:
        log.error("failed to run verify on %s" % (device_name))
        return 1  # Not ok to proceed
    log.info("Successfully verified the device")

    if dm._sock:
        dm._sock.close()
    time.sleep(30)
    dm = devicemanager.DeviceManagerSUT(device_name, 20701)
    print "in smoketest, going to call installOneApp with dm: %s, %s" \
          % (dm, dm._sock)
    if installOneApp(dm, deviceRoot, os.path.abspath(appFileName), None, logcat=False):
        log.error("failed to install %s on device %s" % (appFileName, device_name))
        return 1
    log.info("Successfully installed the application")

    if not runTests(device_name, processName, number):
        log.error("failed to run dom tests on %s" % (device_name))
        return 1
    log.info("Successfully ran the mochitests")
    return 0

if __name__ == '__main__':
    global appFileName, processName
    appFileName = os.path.join(os.path.dirname(__file__),
                               "fennec-19.0a1.multi.android-arm.apk")
    processName = "org.mozilla.fennec"

    device_name = os.getenv('SUT_NAME')
    if (len(sys.argv) != 2):
        if device_name in (None, ''):
            print "usage: smoketest.py <device name>"
            print "   Must have $SUT_NAME set in environ to omit device name"
            sys.exit(1)
        else:
            print "INFO: Using device '%s' found in env variable" % device_name
    else:
        device_name = sys.argv[1]

    num = re.compile('.*([0-9]+)$')
    match = num.match(device_name)
    deviceNum = match.group(1)
    getSUTLogger(filename="smoketest-%s.log" % deviceNum,
                 loggername="smoketest")
    sys.exit(smoketest(device_name, deviceNum))
