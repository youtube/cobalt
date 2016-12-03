#!/usr/bin/env python


import os
import sys

import site
site.addsitedir(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../lib/python"))

from mozdevice import devicemanagerSUT as devicemanager
from sut_lib import getOurIP, calculatePort, setFlag, log, \
    powermanagement


def reboot(dm):
    cwd = os.getcwd()
    deviceName = os.path.basename(cwd)
    errorFile = os.path.join(cwd, '..', 'error.flg')
    proxyIP = getOurIP()
    proxyPort = calculatePort()

    if 'panda' not in deviceName:
        # Attempt to set devicename via env variable 'SUT_NAME'
        sname = os.getenv('SUT_NAME')
        if sname.strip():
            deviceName = sname.strip()
        else:
            log.info("Unable to find a proper devicename, will attempt to "
                     "reboot device")

    if dm is not None:
        try:
            dm.getInfo('process')
            log.info(dm._runCmds(
                [{'cmd': 'exec su -c "logcat -d -v time *:W"'}], timeout=10))
        except:
            log.info("Failure trying to run logcat on device")
    else:
        log.info("We were unable to connect to device %s, skipping logcat" %
                 deviceName)

    try:
        log.info('forcing device %s reboot' % deviceName)
        status = powermanagement.soft_reboot_and_verify(
            dm=dm,
            device=deviceName,
            ipAddr=proxyIP,
            port=proxyPort,
            silent=True
        )
        log.info(status)
    except:
        log.info("Failure while rebooting device")
        setFlag(errorFile,
                "Remote Device Error: Device failed to recover after reboot",
                True)
        return 1

    sys.stdout.flush()
    return 0

if __name__ == '__main__':
    if (len(sys.argv) != 2):
        print "usage: reboot.py <ip address>"
        sys.exit(1)

    deviceIP = sys.argv[1]
    dm = None
    try:
        log.info("connecting to: %s" % deviceIP)
        dm = devicemanager.DeviceManagerSUT(deviceIP)
        dm.debug = 0
        dm.default_timeout = 30  # Set our timeout lower for deviceManager here
    except:
        pass
    sys.exit(reboot(dm))
