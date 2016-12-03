#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Assumes Python 2.6
#

import os
import sys
import time
import socket
import signal
import logging
import datetime
import traceback
import subprocess
import random
from mozdevice import devicemanagerSUT as devicemanager
import powermanagement

from optparse import OptionParser
import json


def getSUTLogger(filename=None, loggername=None):

    if not loggername:
        loggername = __name__

    logger = logging.getLogger('')
    logger.handlers = []

    logger = logging.getLogger(loggername)
    logger.handlers = []

    extra_kwargs = {}
    if filename:
        extra_kwargs['filename'] = filename
        extra_kwargs['filemode'] = "a"

    logging.basicConfig(level=logging.DEBUG,
                        format="%(asctime)s: %(levelname)s: %(message)s",
                        datefmt='%m/%d/%Y %H:%M:%S',
                        **extra_kwargs)
    if filename:
        console = logging.StreamHandler()
        console.setLevel(logging.INFO)
        root = logging.getLogger('')
        root.addHandler(console)

    return logging.getLogger(loggername)

log = getSUTLogger()

# look for devices.json where foopies have it
# if not loaded, then try relative to sut_lib.py's path
# as that is where it would be if run from tools repo


try:
    masters = json.load(open(
        '/builds/tools/buildfarm/maintenance/production-masters.json', 'r'))
except:
    masters = {}


def connect(deviceIP, sleep=False):
    if sleep:
        log.info("We're going to sleep for 90 seconds")
        time.sleep(90)

    log.info("Connecting to: " + deviceIP)
    return devicemanager.DeviceManagerSUT(deviceIP)


def getMaster(hostname):
    # remove all the datacenter cruft from the hostname
    # because we can never really know if a buildbot.tac
    # hostname entry is FQDN or not
    host = hostname.strip().split('.')[0]
    result = None
    for o in masters:
        if 'hostname' in o:
            if o['hostname'].startswith(host):
                result = o
                break
    return result


def dumpException(msg):
    """Gather information on the current exception stack and log it
    """
    t, v, tb = sys.exc_info()
    log.debug(msg)
    for s in traceback.format_exception(t, v, tb):
        if '\n' in s:
            for t in s.split('\n'):
                log.debug(t)
        else:
            log.debug(s[:-1])
    log.debug('Traceback End')

# copied runCommand to tools/buildfarm/utils/run_jetpack.py


def runCommand(cmd, env=None, logEcho=True):
    """Execute the given command.
    Sends to the logger all stdout and stderr output.
    """
    log.debug('calling [%s]' % ' '.join(cmd))

    o = []
    p = subprocess.Popen(
        cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    try:
        for item in p.stdout:
            o.append(item[:-1])
            if logEcho:
                log.debug(item[:-1])
        p.wait()
    except KeyboardInterrupt:
        p.kill()
        p.wait()

    return p, o


def pingDevice(device):
    # bash-3.2$ ping -c 2 -o tegra-056
    # PING tegra-056.build.mtv1.mozilla.com (10.250.49.43): 56 data bytes
    # 64 bytes from 10.250.49.43: icmp_seq=0 ttl=64 time=1.119 ms
    #
    # --- tegra-056.build.mtv1.mozilla.com ping statistics ---
    # 1 packets transmitted, 1 packets received, 0.0% packet loss
    # round-trip min/avg/max/stddev = 1.119/1.119/1.119/0.000 ms

    out = []
    result = False
    p, o = runCommand(['ping', '-c 5', device], logEcho=False)
    for s in o:
        out.append(s)
        if ('5 packets transmitted, 5 packets received' in s) or \
           ('5 packets transmitted, 5 received' in s):
            result = True
            break
    return result, out


def getOurIP(hostname=None):
    """Open a socket against a known server to discover our IP address
    """
    if hostname is None:
        testname = socket.gethostname()
    else:
        testname = hostname
    if 'CP_IP' in os.environ:
        result = os.environ['CP_IP']
    else:
        result = None
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect((testname, 22))
            result = s.getsockname()[0]
            s.close()
        except:
            dumpException('unable to determine our IP address')
    return result


def getIPAddress(hostname):
    """Parse the output of nslookup to determine what is the
    IP address for the device ID that is to be monitored.
    """
    ipAddress = None
    f = False
    p, o = runCommand(['nslookup', hostname], logEcho=False)
    for s in o:
        if '**' in s:
            break
        if f:
            if s.startswith('Address:'):
                ipAddress = s.split()[1]
        else:
            if s.startswith('Name:'):
                f = True

    return ipAddress


def getChildPIDs(pid):
    # ps -U cltbld -O ppid,pgid,command
    # pid   ppid  pgid
    # 18456     1 18455 /opt/local/Libra   ??  S      0:00.88 /opt/local/Library/Frameworks/Python.framework/Versions/2.6/Resources/Python.app/Contents/MacOS/Python /opt/local/Library/Frameworks/Python.framework/Versions/2.6/bin/twistd$
    # 18575 18456 18455 /opt/local/Libra   ??  S      0:00.52
    # /opt/local/Library/Frameworks/Python.framework/Versions/2.6/Resources/Python.app/Contents/MacOS/Python
    # ../../sut_tools/installApp.py 10.250.49.8 ../talos-data/fennec-4.1a1pr$
    p, lines = runCommand(['ps', 'xU', 'cltbld', '-O', 'ppid,pgid,command'])
    pids = []
    for line in lines:
        item = line.split()
        if len(item) > 1:
            try:
                p = int(item[1])
                if p == pid:
                    pids.append(int(item[0]))
            except ValueError:
                pass
    return pids


def killPID(pid, sig=signal.SIGTERM, includeChildren=False):
    """Attempt to kill a process.
    After sending signal, confirm if process is
    """
    log.info('calling kill for pid %s with signal %s' % (pid, sig))
    if includeChildren:
        childList = getChildPIDs(pid)
        for childPID in childList:
            killPID(childPID, sig, True)
            try:
                os.kill(childPID, 0)
                # exception raised if pid NOT found
                # so we know at this point it resisted
                # normal killPID() call
                killPID(childPID, signal.SIGKILL, True)
            except OSError:
                log.debug('%s not found' % childPID)

    try:
        os.kill(pid, sig)
        n = 0
        while n < 30:
            n += 1
            try:
                # verify process is gone
                os.kill(pid, 0)
                time.sleep(1)
            except OSError:
                return True
        return False
    except OSError:
        # if pid doesn't exist, then it worked ;)
        return True


def getLastLine(filename):
    """Run the tail command against the given file and return
    the last non-empty line.
    The content of twistd.log often has output from the slaves
    so we can't assume that it will always be non-empty line.
    """
    result = ''
    fileTail = []

    if os.path.isfile(filename):
        p = subprocess.Popen(['tail', filename],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            for item in p.stdout:
                fileTail.append(item)
            p.wait()
        except KeyboardInterrupt:
            p.kill()
            p.wait()

    if len(fileTail) > 0:
        n = len(fileTail) - 1
        while n >= 0:
            s = fileTail[n].strip()
            if len(s) > 4 and len(s[:4].strip()) > 0:
                result = s
                break
            n -= 1

    return result


def stopProcess(pidFile, label):
    log.debug('%s: looking for %s' % (label, pidFile))
    if os.path.isfile(pidFile):
        try:
            pid = int(open(pidFile, 'r').read().strip())
            log.debug('%s: first attempt to kill %s' % (label, pid))
            if not killPID(pid, includeChildren=True):
                log.debug('%s: second attempt to kill %s' % (label, pid))
                killPID(pid, sig=signal.SIGKILL, includeChildren=True)
            try:
                log.debug('verifying %s is gone' % label)
                try:
                    os.kill(pid, 0)
                except OSError:
                    log.info('%s stopped' % label)
                    os.remove(pidFile)
            except:
                dumpException('verify step of stopProcess')
                log.error('%s: pid %s not found' % (label, pid))
        except ValueError:
            log.error('%s: unable to read %s' % (label, pidFile))


def checkStalled(device):
    """Returns the following based on if any stalled pids were found:
            1 = clean, nothing was stalled
            2 = dirty, but pids were killed
            3 = dirty, but pids remain
    """
    pids = []
    deviceIP = getIPAddress(device)
    devicePath = os.path.join('/builds', device)
    p, lines = runCommand(['ps', 'xU', 'cltbld'])

    this_device_lines = []
    for line in lines:
        if deviceIP in line or device in line:
            this_device_lines.append(line)

    for line in this_device_lines:
        if ('bcontroller' in line) or ('server.js' in line) or \
           ('logcat.py' in line) or ('pywebsocket_wrapper.py' in line):
            item = line.split()
            if len(item) > 1:
                try:
                    pids.append(int(item[0]))
                except ValueError:
                    pass

    if len(pids) == 0:
        result = 1
    else:
        result = 2

    for pid in pids:
        if not killPID(pid, sig=signal.SIGKILL, includeChildren=True):
            result = 3

    for f in ('runtestsremote', 'remotereftest', 'remotereftest.pid.xpcshell'):
        pidFile = os.path.join(devicePath, '%s.pid' % f)
        if os.path.exists(pidFile):
            stopProcess(pidFile, f)
            if os.path.exists(pidFile):
                result = 3

    return result


def stopSlave(pidFile):
    """Try to terminate the buildslave
    """
    stopProcess(pidFile, 'buildslave')
    return os.path.exists(pidFile)


def checkSlaveAlive(bbClient):
    """Check if the buildslave process is alive.
    """
    pidFile = os.path.join(bbClient, 'twistd.pid')
    log.debug('checking if slave is alive [%s]' % pidFile)
    if os.path.isfile(pidFile):
        try:
            pid = int(file(pidFile, 'r').read().strip())
            try:
                os.kill(pid, 0)
                return True
            except OSError:
                dumpException('check to see if pid %s is active failed' % pid)
        except:
            dumpException('unable to check slave - pidfile [%s] was not readable' % pidFile)
    return False


def checkSlaveActive(bbClient):
    """Check to determine what was the date/time
    of the last line of it's log output.
    """
    logFile = os.path.join(bbClient, 'twistd.log')
    lastline = getLastLine(logFile)
    if len(lastline) > 0:
        logTS = datetime.datetime.strptime(lastline[:19], '%Y-%m-%d %H:%M:%S')
        logTD = datetime.datetime.now() - logTS
        return logTD
    return None


def checkCPAlive(bbClient):
    """Check if the clientproxy process is alive.
    """
    pidFile = os.path.join(bbClient, 'clientproxy.pid')
    log.debug('checking if clientproxy is alive [%s]' % pidFile)
    if os.path.isfile(pidFile):
        try:
            pid = int(file(pidFile, 'r').read().strip())
            try:
                os.kill(pid, 0)
                return True
            except OSError:
                dumpException('check to see if pid %s is active failed' % pid)
        except:
            dumpException('unable to check clientproxy - pidfile [%s] was not readable' % pidFile)
    return False


def checkCPActive(bbClient):
    """Check to determine what was the date/time
    of the last line of it's log output.
    """
    logFile = os.path.join(bbClient, 'clientproxy.log')
    lastline = getLastLine(logFile)
    if len(lastline) > 0:
        logTS = datetime.datetime.strptime(lastline[:19], '%Y-%m-%d %H:%M:%S')
        logTD = datetime.datetime.now() - logTS
        return logTD
    return None

# helper routines for interacting with devicemanager
# and buildslave steps


def setFlag(flagfile, contents=None, silent=False):
    log.info(flagfile)
    h = open(flagfile, 'a+')
    if contents is not None:
        # TODO: log.error output wasn't appearing in the logs, so using print
        # for now
        if not silent:
            print contents
        h.write('%s\n' % contents)
    h.close()
    time.sleep(30)


def clearFlag(flagfile, dump=False):
    if os.path.exists(flagfile):
        if dump:
            # Py 2.6 syntax
            log.info("Contents of %s follow:" % flagfile)
            with open(flagfile, "r") as f:
                log.info(f.read())
        os.remove(flagfile)


def calculatePort():
    s = os.environ['SUT_NAME']
    try:
        n = 50000 + int(s.split('-')[1])
    except:
        n = random.randint(40000, 50000)
    return n


def getResolution(dm):
    s = dm.getInfo('screen')['screen'][0]

    if 'X:' in s and 'Y:' in s:
        parts = s.split()
        width = int(parts[0].split(':')[1])
        height = int(parts[1].split(':')[1])
        return width, height
    else:
        return 0, 0


def getDeviceTimestamp(dm):
    ts = int(dm.getCurrentTime())  # epoch time in milliseconds
    dt = datetime.datetime.utcfromtimestamp(ts / 1000)
    log.info("Current device time is %s" % dt.strftime('%Y/%m/%d %H:%M:%S'))
    return dt


def setDeviceTimestamp(dm):
    s = datetime.datetime.now().strftime('%Y/%m/%d %H:%M:%S')
    log.info("Setting device time to %s" % s)
    try:
        dm._runCmds([{'cmd': 'settime %s' % s}])
        return True
    except devicemanager.AgentError, e:
        log.warn("Exception while setting device time: %s" % str(e))
        return False


def checkDeviceRoot(dm):
    dr = dm.getDeviceRoot()
    # checking for /mnt/sdcard/...
    log.info("devroot %s" % str(dr))
    if not dr or dr == '/tests':
        return None
    return dr


def waitForDevice(dm, waitTime=120, silent=False):
    retVal = _waitForDevice(dm, waitTime, silent)
    if not retVal:
        if not silent:
            log.error("Remote Device Error: waiting for device timed out.")
        sys.exit(1)
    return retVal


def _waitForDevice(dm, waitTime=120, silent=False):
    log.info("Waiting for device to come back...")
    time.sleep(waitTime)
    deviceIsBack = False
    tries = 0
    maxTries = 3
    while tries < maxTries:
        tries += 1
        log.info("Try %d" % tries)
        if dm._sock:
            dm._sock.close()
            dm._sock = None
        dm.deviceRoot = None
        dm.retries = 0
        if checkDeviceRoot(dm) is not None:
            deviceIsBack = True
            break
        time.sleep(waitTime)
    if not deviceIsBack:
        if not silent:
            log.error("Remote Device Error: waiting for device timed out.")
        return False
    return True


def stopStalled(device):
    deviceIP = getIPAddress(device)
    devicePath = os.path.join('/builds', device)
    pids = []

    # look for any process that is associated with the device
    # PID TTY           TIME CMD
    # 212 ??         0:17.99
    # /opt/local/Library/Frameworks/Python.framework/Versions/2.6/Resources/Python.app/Contents/MacOS/Python
    # clientproxy.py -b --tegra=tegra-032
    p, lines = runCommand(['ps', 'xU', 'cltbld'])
    this_device_lines = []
    for line in lines:
        if deviceIP in line or device in line:
            this_device_lines.append(line)

    for line in this_device_lines:
        if ('bcontroller' in line) or ('server.js' in line) or \
           ('logcat.py' in line) or ('pywebsocket_wrapper.py' in line):
            item = line.split()
            if len(item) > 1:
                try:
                    pids.append(int(item[0]))
                except ValueError:
                    pass
    for pid in pids:
        killPID(pid, sig=signal.SIGKILL, includeChildren=True)

    result = False
    for f in ('runtestsremote', 'remotereftest', 'remotereftest.pid.xpcshell'):
        pidFile = os.path.join(devicePath, '%s.pid' % f)
        log.info("checking for previous test processes ... %s" % pidFile)
        if os.path.exists(pidFile):
            log.info("pidfile from prior test run found, trying to kill")
            stopProcess(pidFile, f)
            if os.path.exists(pidFile):
                result = True

    return result


def loadOptions(defaults=None):
    """Parse command line parameters and populate the options object.
    """
    parser = OptionParser()

    if defaults is not None:
        for key in defaults:
            items = defaults[key]

            if len(items) == 4:
                (shortCmd, longCmd, defaultValue, helpText) = items
                optionType = 's'
            else:
                (shortCmd, longCmd, defaultValue, helpText, optionType) = items

            if optionType == 'b':
                parser.add_option(shortCmd, longCmd, dest=key, action='store_true', default=defaultValue, help=helpText)
            else:
                parser.add_option(shortCmd, longCmd, dest=key,
                                  default=defaultValue, help=helpText)

    (options, args) = parser.parse_args()
    options.args = args

    return options


def stopDevice(device):
    deviceIP = getIPAddress(device)
    devicePath = os.path.join('/builds', device)
    errorFile = os.path.join(devicePath, 'error.flg')

    log.info('%s: %s - stopping all processes' % (device, deviceIP))

    stopStalled(device)
    stopProcess(os.path.join(devicePath, 'clientproxy.pid'), 'clientproxy')
    stopProcess(os.path.join(devicePath, 'twistd.pid'), 'buildslave')

    log.debug('  clearing flag files')

    if os.path.isfile(errorFile):
        log.info('  error.flg cleared')
        os.remove(errorFile)

    powermanagement.reboot_device(device, debug=True)
