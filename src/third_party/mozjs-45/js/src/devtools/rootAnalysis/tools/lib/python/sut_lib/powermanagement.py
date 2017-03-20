# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Assumes Python 2.6
#

import os
import time
import relay as relayModule


from sut_lib.__init__ import waitForDevice, getSUTLogger, dumpException
import devices

log = getSUTLogger()


def soft_reboot(device, dm, silent=False, *args, **kwargs):
    """
    Use the softest/kindest reboot method we think we should use.

    This does a reboot over devicemanager* in some cases, and a relay/pdu reboot in others
    """
    if 'panda-' in device:
        # Using devicemanager for reboots on pandas doesn't work reliably
        if reboot_relay(device):
            return True
        else:
            if not silent:
                log.warn("Automation Error: Unable to reboot %s via Relay Board." %
                         device)

    # If this panda doesn't successfully relay-reboot fall through to
    # devicemanager
    return dm.reboot(*args, **kwargs)


def reboot_relay(device):
    if device in devices.pandas and devices.pandas[device]['relayhost']:
        relay_host = devices.pandas[device]['relayhost']
        bank, relay = map(int, devices.pandas[device]['relayid'].split(":"))
        log.info("Calling PDU powercycle for %s, %s:%s:%s" % (
            device, relay_host, bank, relay))
        maxTries = 15
        curTry = 1
        while not relayModule.powercycle(relay_host, bank, relay):
            log.info("Was not able to powercycle, attempt %s of %s" %
                     (curTry, maxTries))
            curTry += 1
            if curTry > maxTries:
                log.error(
                    "Failed to powercycle %s times, giving up" % maxTries)
                return False  # Stop Trying
            time.sleep(1)  # Give us a chance to get a free socket next time
        return True
    return False


def soft_reboot_and_verify(device, dm, waitTime=90, max_attempts=5, silent=False, *args, **kwargs):
    attempt = 0
    while attempt < max_attempts:
        attempt += 1
        retVal = soft_reboot(device, dm, silent, *args, **kwargs)
        if not retVal:
            continue

        if waitForDevice(dm, waitTime, silent=True):
            return True
    return False



