#!/usr/bin/env python

import os
import sys
import glob
import shutil
import zipfile

import site
site.addsitedir(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../lib/python"))

from mozdevice import devicemanagerSUT as devicemanager
from sut_lib import getOurIP, calculatePort, setFlag, checkDeviceRoot, \
    getDeviceTimestamp, setDeviceTimestamp, \
    getResolution, waitForDevice, runCommand, log, powermanagement


# hwine: minor ugg - the flag files need to be global. Refactoring into
#        a class would solve, but this module is scheduled for rewrite


def installOneApp(dm, devRoot, app_file_local_path, errorFile, logcat=True):

    source = app_file_local_path
    filename = os.path.basename(source)
    target = os.path.join(devRoot, filename)

    log.info("Installing %s" % target)
    if dm.pushFile(source, target):
        status = dm.installApp(target)
        if status is None:
            log.info('-' * 42)
            log.info('installApp() done - gathering debug info')
            dm.getInfo('process')
            dm.getInfo('memory')
            dm.getInfo('uptime')
            if logcat:
                try:
                    print dm._runCmds([{'cmd': 'exec su -c "logcat -d -v time *:W"'}])
                except devicemanager.DMError, e:
                    setFlag(errorFile, "Remote Device Error: Exception hit while trying to run logcat: %s" % str(e))
                    return 1
        else:
            setFlag(errorFile,
                    "Remote Device Error: updateApp() call failed - exiting")
            return 1
    else:
        setFlag(errorFile, "Remote Device Error: unable to push %s" % target)
        return 1
    return 0


def find_robocop():
    # we hardcode the relative path to robocop.apk for bug 715215
    # but it may not be unpacked at the time this runs, so be prepared
    # to extract if needed (but use the extracted one if there)
    extracted_location = 'build/tests/bin/robocop.apk'
    self_extracted_location = 'build/robocop.apk'
    actual_location = None

    # for better error reporting
    global errorFile

    if os.path.exists(extracted_location):
        actual_location = extracted_location
    elif os.path.exists(self_extracted_location):
        # already grabbed it in prior step
        actual_location = self_extracted_location
    else:
        expected_zip_location = 'build/fennec*tests.zip'
        matches = glob.glob(expected_zip_location)
        if len(matches) == 1:
            # try to grab the file we need from there, giving up if any
            # assumption doesn't match
            try:
                archive = zipfile.ZipFile(matches[0], 'r')
                apk_info = archive.getinfo('bin/robocop.apk')
                # it's in the archive, extract to tmp dir - will be created
                archive.extract(apk_info, 'build/tmp')
                shutil.move(
                    'build/tmp/bin/robocop.apk', self_extracted_location)
                actual_location = self_extracted_location
                # got it
            except Exception as e:
                log.warning(
                    "(robocop): zip file not as expected: %s (%s)" % (e.message,
                                                                      str(e.args)))
                log.warning("(robocop): robocop.apk will not be installed")
        else:
            log.warning(
                "(robocop): Didn't find just one %s; found '%s'" % (expected_zip_location,
                                                                    str(matches)))
            log.warning("(robocop): robocop.apk will not be installed")

    return actual_location


def one_time_setup(ip_addr, major_source):
    ''' One time setup of state

    ip_addr - of the device we want to install app at
    major_source - we've hacked this script to install
            may-also-be-needed tools, but the source we're asked to
            install has the meta data we need

    Side Effects:
        global, needed for error reporting:
            errorFile
    '''

    # set up the flag files, used throughout
    cwd = os.getcwd()
    global errorFile
    errorFile = os.path.join(cwd, '..', 'error.flg')
    deviceName = os.path.basename(cwd)

    proxyIP = getOurIP()
    proxyPort = calculatePort()

    workdir = os.path.dirname(major_source)
    inifile = os.path.join(workdir, 'fennec', 'application.ini')
    remoteappini = os.path.join(workdir, 'talos', 'remoteapp.ini')
    log.info('copying %s to %s' % (inifile, remoteappini))
    runCommand(['cp', inifile, remoteappini])

    log.info("connecting to: %s" % ip_addr)
    dm = devicemanager.DeviceManagerSUT(ip_addr)
# Moar data!
    dm.debug = 3

    devRoot = checkDeviceRoot(dm)

    if devRoot is None or devRoot == '/tests':
        setFlag(errorFile, "Remote Device Error: devRoot from devicemanager [%s] is not correct - exiting" % devRoot)
        return None, None

    try:
        log.info("%s, %s" % (proxyIP, proxyPort))
        getDeviceTimestamp(dm)
        setDeviceTimestamp(dm)
        getDeviceTimestamp(dm)
        dm.getInfo('process')
        dm.getInfo('memory')
        dm.getInfo('uptime')

        width, height = getResolution(dm)
        # adjust resolution down to allow fennec to install without memory
        # issues
        if (width == 1600 or height == 1200):
            dm.adjustResolution(1024, 768, 'crt')
            log.info('forcing device reboot')
            if not powermanagement.soft_reboot_and_verify(device=deviceName, dm=dm, ipAddr=proxyIP, port=proxyPort):
                return None, None

            width, height = getResolution(dm)
            if width != 1024 and height != 768:
                setFlag(errorFile, "Remote Device Error: Resolution change failed.  Should be %d/%d but is %d/%d" % (1024, 768, width, height))
                return None, None

    except devicemanager.AgentError, err:
        log.error("remoteDeviceError: while doing one time setup for installation: %s" % err)
        return None, None

    return dm, devRoot


def main(argv):
    if (len(argv) < 3):
        print "usage: installApp.py <ip address> <localfilename> [<processName>]"
        sys.exit(1)

    # N.B. 3rd arg not used anywhere

    ip_addr = argv[1]
    path_to_main_apk = argv[2]
    dm, devRoot = one_time_setup(ip_addr, path_to_main_apk)
    if not dm:
        return 1

    # errorFile is already created globally in one_time_setup call above
    if installOneApp(dm, devRoot, path_to_main_apk, errorFile):
        return 1

    # also install robocop if it's available
    robocop_to_use = find_robocop()
    if robocop_to_use is not None:
        waitForDevice(dm)
        # errorFile is already created globally in one_time_setup call above
        if installOneApp(dm, devRoot, robocop_to_use, errorFile):
            return 1

    # make sure we're all the way back up before we finish
    waitForDevice(dm)
    return 0

if __name__ == '__main__':
    # Stop buffering! (but not while testing)
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
    sys.exit(main(sys.argv))
