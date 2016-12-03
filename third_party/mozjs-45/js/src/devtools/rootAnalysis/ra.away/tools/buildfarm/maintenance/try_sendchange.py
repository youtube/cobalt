#!/usr/bin/env python
# Created by Lukas Blakk on 2010-08-25
"""try_sendchange.py tryDir args

example usage:
    python try_sendchange.py lsblakk@mozilla.com-94624b46ec1b --build o --p all --u all --t none

This script creates and sends sendchanges for each of the
platforms/test/talos requested to each of the TEST_MASTERS"""

import sys
import os
import argparse
import re
from ftplib import FTP

TEST_MASTERS = ['production-master01.build.mozilla.org:9009']
PLATFORMS = ['linux', 'linux64', 'macosx', 'macosx64', 'win32', 'android-r7']
TRY_BASE_PATH = '/pub/mozilla.org/firefox/try-builds/%(email)s-%(changeset)s/'
PLATFORM_BASE_PATH = '/pub/mozilla.org/firefox/try-builds/%(email)s-%(changeset)s/try-%(platform)s/'

if __name__ == "__main__":
    args = sys.argv[1:]

    parser = argparse.ArgumentParser(description='Accepts command line arguments for creating a test/talos try sendchanges', usage='%(prog)s email-changeset [options]')

    parser.add_argument('--dry-run', '-n',
                        action='store_true',
                        dest='dry_run',
                        help='outputs only the text of the sendchanges, without sending them')
    parser.add_argument('--build', '-b',
                        default='do',
                        dest='build',
                        help='accepts the build types requested (default is debug & opt)')
    parser.add_argument('--platforms', '-p',
                        default='all',
                        dest='platforms',
                        help='provide a list of desktop platforms, or specify none (default is all)')
    parser.add_argument('--unittests', '-u',
                        default='none',
                        dest='tests',
                        help='provide a list of unit tests, or specify all (default is none)')
    parser.add_argument('--talos', '-t',
                        default='none',
                        dest='talos',
                        help='provide a list of talos tests, or specify all (default is none)')

    (options, unknown_args) = parser.parse_known_args(args)

    if options.build == 'do' or options.build == 'od':
        options.build = ['opt', 'debug']
    elif options.build == 'd':
        options.build = ['debug']
    elif options.build == 'o':
        options.build = ['opt']
    else:
        options.build = ['opt']

    if options.platforms == 'all':
        platforms = PLATFORMS
    else:
        options.platforms = options.platforms.split(',')
        platforms = options.platforms

    sendchanges = []
    email = None

    # locate an email-changeset in the command line request
    for arg in unknown_args:
        match = re.search('@', arg)
        if match:
            email, dummy, changeset = unknown_args[0].rpartition('-')
            continue

    if (email != None):
        # now create sendchanges for each TEST_MASTER
        for master in TEST_MASTERS:
            ftp = FTP('dm-ftp01.mozilla.org')
            ftp.login()

            tryDirPath = TRY_BASE_PATH % {'email': email,
                                          'changeset': changeset}
            dirlist = ftp.nlst(tryDirPath)
            if dirlist:
                print "Scanning ftp...\n"
            else:
                print "Nothing (or no) FTP dir list, so nothing to work with here."
            for dir in dirlist:
                for platform in platforms:
                    for buildType in options.build:
                        if buildType == 'debug':
                            platform = "%s-%s" % (platform, buildType)
                        if dir.endswith(platform):
                            tryUrlPath = PLATFORM_BASE_PATH % {'email': email,
                                                               'changeset': changeset,
                                                               'platform': platform}
                            filelist = ftp.nlst(tryUrlPath)

                            packagedTests = None
                            for f in filelist:
                                match = re.search('tests.zip', f)
                                if match:
                                    packagedTests = f
                                    print "Found test pkg for %s" % platform

                            path = None
                            for f in filelist:
                                for suffix in ('.tar.bz2', '.win32.zip', '.dmg', 'arm.apk'):
                                    if f.endswith(suffix):
                                        path = f
                                        print "Found build for %s" % platform
                                        if options.talos != 'none' and buildType == 'debug':
                                            print "No talos for debug builds...skipping."

                            if options.talos != 'none' and buildType == 'opt' and path:
                                sendchange = "buildbot sendchange --master %(master)s " \
                                             "--branch try-%(platform)s-talos " \
                                             "--revision %(changeset)s " \
                                             "--comment \"try: --t %(talos)s\" " \
                                             "--user %(email)s http://stage.mozilla.org%(path)s " \
                                             % {'master': master,
                                                'platform': platform,
                                                'changeset': changeset,
                                                'talos': options.talos,
                                                'email': email,
                                                'path': path}
                                if not options.dry_run:
                                    os.system(sendchange)
                                    print "Sent Talos: %s" % sendchange
                                else:
                                    sendchanges.append(sendchange)
                            if options.tests != 'none' and packagedTests and path:
                                # take off the -debug in platform name if exists cause we tack
                                # buildType on in the sendchange
                                platform = platform.split('-')[0]
                                sendchange = "buildbot sendchange --master %(master)s " \
                                             "--branch try-%(platform)s-%(buildType)s-unittest " \
                                             "--revision %(changeset)s " \
                                             "--comment \"try: --u %(tests)s\" " \
                                             "--user %(email)s http://stage.mozilla.org%(path)s " \
                                             "http://stage.mozilla.org%(packagedTests)s " \
                                             % {'master': master,
                                                'platform': platform,
                                                'buildType': buildType,
                                                'changeset': changeset,
                                                'tests': options.tests,
                                                'email': email,
                                                'path': path,
                                                'packagedTests': packagedTests}
                                if not options.dry_run:
                                    os.system(sendchange)
                                    print "Sent Test: %s" % sendchange
                                else:
                                    sendchanges.append(sendchange)
            ftp.quit()
            if options.dry_run and dirlist:
                print "\nWhat will be sent:\n"
                for s in sendchanges:
                    print s

    else:
        print """Couldn't find an email address in the arguments list, please refer to --help\n
                Usage: python try_sendchange.py email-changeset [options]"""
