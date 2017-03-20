#!/usr/bin/python
""" Usage: %prog platform [sdk_location] [tarball_url] [ftp_url] [extension]

Script to pull down the latest jetpack sdk tarball, unpack it, and run its tests against the
executable of whatever valid platform is passed.
"""
import os
import sys
import urllib
import shutil
import re
import traceback
import time
import logging
import subprocess
from optparse import OptionParser

SDK_TARBALL = "addonsdk.tar.bz2"
POLLER_DIR = "addonsdk-poller"
SDK_DIR = "jetpack"

PLATFORMS = {
    'snowleopard': 'macosx64',
    'lion': 'macosx64',
    'yosemite': 'macosx64',
    'xp-ix': 'win32',
    'win7-ix': 'win32',
    'win8': 'win32',
    'w764': 'win64',
    'ubuntu32': 'linux',
    'ubuntu64': 'linux64',
    'ubuntu32_vm': 'linux',
    'ubuntu64_vm': 'linux64',
    'ubuntu64-asan_vm': 'linux64-asan',
}

log = logging.getLogger()


# copied runCommand from tools/sut_tools/sut_lib.py
def runCommand(cmd, env=None, logEcho=True):
    """Execute the given command.
    Sends to the logger all stdout and stderr output.
    """
    log.debug('calling [%s]' % ' '.join(cmd))

    o = []
    p = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)

    try:
        for item in p.stdout:
            o.append(item[:-1])
            print item[:-1]
        p.wait()
    except KeyboardInterrupt:
        p.kill()
        p.wait()

    return p, o


def emphasizeFailureText(text):
    return '<em class="testfail">%s</em>' % text


def summarizeJetpackTestLog(name, log):
    infoRe = re.compile(r"(\d+) of (\d+) tests passed")
    successCount = 0
    failCount = 0
    totalCount = 0
    summary = ""
    for line in log:
        m = infoRe.match(line)
        if m:
            successCount += int(m.group(1))
            totalCount += int(m.group(2))
    failCount = int(totalCount - successCount)
    # Handle failCount.
    failCountStr = str(failCount)
    if failCount > 0:
        failCountStr = emphasizeFailureText(failCountStr)
    # Format the counts
    summary = "%d/%d" % (totalCount, failCount)
    # Return the summary.
    return "TinderboxPrint:%s<br/>%s\n" % (name, summary)

if __name__ == '__main__':
    is_poller = False
    untar_args = ''
    parser = OptionParser(usage=__doc__)
    parser.add_option("-e", "--extension", dest="ext", default="",
                      help="Extension to match in the builds directory for downloading the build")
    parser.add_option("-f", "--ftp-url", dest="ftp_url", default="",
                      help="Url for ftp latest-builds directory where the build to download lives")
    parser.add_option(
        "-s", "--sdk-location", dest="sdk_location", default='jetpack-location.txt',
        help="Text file or url to use to download the current addonsdk tarball")
    parser.add_option("-p", "--platform", dest="platform",
                      help="Platform of the build to download and to test")
    parser.add_option("-t", "--tarball-url", dest="tarball_url", default="",
                      help="Url to download the jetpack tarball from")
    parser.add_option("-b", "--branch", dest="branch", default="",
                      help="The branch this test is pulling an installer to run against")

    (options, args) = parser.parse_args()

    # Handling method of running tests (branch checkin vs. poller on addon-sdk
    # repo)
    if options.ftp_url == "":
        # Branch change triggered test suite
        # 'jetpack' dir must exist the build's tests package
        if os.path.exists("./%s" % SDK_DIR):
            os.chdir(SDK_DIR)
            if ".txt" in options.sdk_location:
                f = open(options.sdk_location, 'r')
                sdk_url = f.readline()
                f.close()
            else:
                sdk_url = options.sdk_location
            os.chdir('..')
            basepath = os.getcwd()
        else:
            print("No jetpack directory present!  Cannot run test suite.")
            sys.exit(2)
    elif options.ftp_url != "" and options.ext != "" and options.tarball_url != "":
        # Addonsdk checkin triggered
        is_poller = True

        # Clobber previous run
        # First try to delete n-2 run which failed to be removed
        if os.path.exists("./" + POLLER_DIR + ".deleteme"):
            try:
                shutil.rmtree(POLLER_DIR + ".deleteme")
                time.sleep(1)
            except EnvironmentError:
                print("Unable to delete n-2 run folder")
        # Then try to delete n-1 test run
        if os.path.exists("./%s" % POLLER_DIR):
            try:
                shutil.rmtree(POLLER_DIR)
            except OSError, e:
                print("Unable to delete n-1 run folder")
                time.sleep(1)
                # Rename it and try to delete it on next run
                os.rename(POLLER_DIR, POLLER_DIR + ".deleteme")

        # Make a new workdir
        os.mkdir(POLLER_DIR)
        os.chdir(POLLER_DIR)
        basepath = os.getcwd()
        sdk_url = options.tarball_url
        platform = PLATFORMS[options.platform]
        branch = options.branch
        ftp_url = options.ftp_url % locals()
        print "FTP_URL: %s" % ftp_url
        pat = re.compile('firefox.*%s$' % options.ext)
        urls = urllib.urlopen("%s" % ftp_url)
        lines = urls.read().splitlines()
        directory = None

        # Find the largest-numbered numeric directory or symlink
        for line in lines:
            if line.startswith('d') or line.startswith('l'):
                parts = line.split(" ")
                # For a symlink, the last three parts are '1332439584', '->',
                # '../old/etc.'
                if line.startswith('l'):
                    parts[-1] = parts[-3]
                if not parts[-1].isdigit():
                    continue
                if directory is None or int(parts[-1]) > int(directory):
                    directory = parts[-1]

        # Now get the executable for this platform
        if directory is None:
            print "Error, no directory found to check for executables"
            sys.exit(4)
        print "Using directory %s" % directory
        urls = urllib.urlopen("%s/%s" % (ftp_url, directory))
        filenames = urls.read().splitlines()
        executables = []
        for filename in filenames:
            f = filename.split(" ")[-1]
            if pat.match(f):
                executables.append(f)

        # Only grab the most recent build (in case there's more than one in the
        # dir)
        if len(executables) > 0:
            exe = sorted(executables, reverse=True)[0]
        else:
            print "Error: missing Firefox executable"
            sys.exit(4)

        info_file = exe.replace(
            options.ext, "%s.txt" % options.ext.split('.')[0])

        # Now get the branch revision
        for filename in filenames:
            if info_file in filename:
                info = filename.split(" ")[-1]
                urllib.urlretrieve(
                    "%s/%s/%s" % (ftp_url, directory, info), info)
                f = open(info, 'r')
                for line in f.readlines():
                    if "hg.mozilla.org" in line:
                        branch_rev = line.split('/')[-1].strip()
                        branch = line.split('/')[-3].strip()
                        print "TinderboxPrint: <a href=\"https://hg.mozilla.org/%(branch)s/rev/%(branch_rev)s\">%(branch)s-rev:%(branch_rev)s</a>\n" % locals()
                f.close()
        print "EXE_URL: %s/%s/%s" % (ftp_url, directory, exe)

        # Download the build
        urllib.urlretrieve("%s/%s/%s" % (ftp_url, directory, exe), exe)
    else:
        parser.error("Incorrect number of arguments")
        sys.exit(4)

    # Custom paths/args for each platform's executable
    if options.platform in ('linux', 'linux64', 'ubuntu32', 'ubuntu64',
                            'ubuntu32_vm', 'ubuntu64_vm', 'ubuntu64-asan_vm'):
        app_path = "%s/firefox/firefox" % basepath
        poller_cmd = 'tar -xjvf *%s' % options.ext
    elif options.platform in ('macosx', 'macosx64', 'snowleopard', 'yosemite'):
        poller_cmd = '../scripts/buildfarm/utils/installdmg.sh *.dmg'
    elif options.platform in ('win32', 'win7-ix', 'win8', 'win64', 'xp-ix'):
        app_path = "%s/firefox/firefox.exe" % basepath
        # The --exclude=*.app is here to avoid extracting a symlink on win32 that is only
        # relevant to OS X. It would be nice if we could just tell tar to
        # ignore symlinks...
        untar_args = "--exclude=*.app"
        poller_cmd = 'unzip -o *%s' % options.ext
    else:
        print "%s is not a valid platform." % options.platform
        sys.exit(4)

    # Download/untar sdk tarball as SDK_TARBALL
    print "SDK_URL: %s" % sdk_url
    try:
        urllib.urlretrieve(sdk_url, SDK_TARBALL)
    except EnvironmentError:
        traceback.print_exc(file=sys.stdout)
        sys.exit(4)
    os.system('tar -xvf %s %s' % (SDK_TARBALL, untar_args))

    # Unpack/mount/unzip the executables in addonsdk checkin triggered runs
    if is_poller:
        os.system(poller_cmd)

    # Find the sdk dir and Mac .app file to run tests with
    # Must happen after poller_cmd is run or Mac has no executable yet in
    # addonsdk checkin runs
    dirs = os.listdir('.')
    for d in dirs:
        if 'addon-sdk' in d:
            sdk_rev = d.split('-')[2]
            print "TinderboxPrint: <a href=\"https://hg.mozilla.org/projects/addon-sdk/rev/%(sdk_rev)s\">sdk-rev:%(sdk_rev)s</a>\n" % locals()
            sdkdir = os.path.abspath(d)
            print "SDKDIR: %s" % sdkdir
        if options.platform in ('macosx', 'macosx64', 'snowleopard', 'lion', 'yosemite'):
            if '.app' in d:
                app_path = os.path.abspath(d)
                print "APP_PATH: %s" % app_path

    if not os.path.exists(app_path):
        print "The APP_PATH \"%s\" does not exist" % app_path
        sys.exit(4)

    # Run it!
    if sdkdir:
        os.chdir(sdkdir)
        args = ['python', 'bin/cfx', '--parseable', '--overload-modules',
                'testall', '-a', 'firefox', '-b', app_path]
        process, output = runCommand(args)
        if is_poller:
            print summarizeJetpackTestLog("Jetpack", output)
        sys.exit(process.returncode)
    else:
        print "SDK_DIR is either missing or invalid."
        sys.exit(4)
