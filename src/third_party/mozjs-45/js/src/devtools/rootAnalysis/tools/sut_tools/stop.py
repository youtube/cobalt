#!/usr/bin/env python

#
# Assumes Python 2.6
#

import os
import sys
import socket
import logging

import site
site.addsitedir(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../lib/python"))

from sut_lib import loadOptions, getIPAddress, stopProcess


options = None
log = logging.getLogger()
defaultOptions = {
    'debug': ('-d', '--debug', False, 'Enable debug output', 'b'),
    'bbpath': ('-p', '--bbpath', '/builds', 'Path where the Device buildbot slave clients can be found'),
    'device': ('', '--device', None, 'Device to check, if not given all Devices will be checked'),
}


def initLogs(options):
    echoHandler = logging.StreamHandler()
    echoFormatter = logging.Formatter(
        '%(asctime)s %(message)s')  # not the normal one

    echoHandler.setFormatter(echoFormatter)
    log.addHandler(echoHandler)

    if options.debug:
        log.setLevel(logging.DEBUG)
        log.info('debug level is on')
    else:
        log.setLevel(logging.INFO)


def stopDevice(device):
    deviceIP = getIPAddress(device)
    devicePath = os.path.join(options.bbpath, device)
    errorFile = os.path.join(devicePath, 'error.flg')

    log.info('%s: %s - stopping all processes' % (device, deviceIP))

    stopProcess(os.path.join(devicePath, 'remotereftest.pid'), 'remotereftest')
    stopProcess(
        os.path.join(devicePath, 'runtestsremote.pid'), 'runtestsremote')
    stopProcess(os.path.join(
        devicePath, 'remotereftest.pid.xpcshell.pid'), 'xpcshell')
    stopProcess(os.path.join(devicePath, 'clientproxy.pid'), 'clientproxy')
    stopProcess(os.path.join(devicePath, 'twistd.pid'), 'buildslave')

    log.debug('  clearing flag files')

    if os.path.isfile(errorFile):
        log.info('  error.flg cleared')
        os.remove(errorFile)

    log.debug('  sending rebt to device')

    try:
        hbSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        hbSocket.settimeout(float(120))
        hbSocket.connect((deviceIP, 20700))
        hbSocket.send('rebt\n')
        hbSocket.close()
    except:
        log.error('  device socket error')


if __name__ == '__main__':
    options = loadOptions(defaultOptions)
    initLogs(options)

    options.bbpath = os.path.abspath(options.bbpath)

    if options.device is None:
        log.error('you must specify a single Device')
        sys.exit(2)

    stopDevice(options.device)
