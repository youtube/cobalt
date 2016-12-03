#!/usr/bin/env python

import os
from os import path
import sys
import logging
from subprocess import CalledProcessError
from tempfile import NamedTemporaryFile
import argparse
import site

logging.basicConfig(
    stream=sys.stdout, level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)

site.addsitedir(path.join(path.dirname(__file__), "../../lib/python"))
site.addsitedir(path.join(path.dirname(__file__), "../../lib/python/vendor"))
from release.info import readReleaseConfig, readConfig
from release.paths import makeCandidatesDir, makeReleasesDir
from util.hg import update, make_hg_url, mercurial
from util.commands import run_remote_cmd
from util.transfer import scp
from util.retry import retry
from util.svn import checkoutSVN, exportJSON, commitSVN, getSVNrev, updateRev

import requests

from release.product_details_update import updateProductDetailFiles


DEFAULT_BUILDBOT_CONFIGS_REPO = make_hg_url('hg.mozilla.org',
                                            'build/buildbot-configs')

REQUIRED_RELEASE_CONFIG = ("productName", "version", "buildNumber",
                           "stage_product")

DEFAULT_RSYNC_EXCLUDES = ['--exclude=*tests*',
                          '--exclude=*crashreporter*',
                          '--exclude=*.log',
                          '--exclude=*.txt',
                          '--exclude=*unsigned*',
                          '--exclude=*update-backup*',
                          '--exclude=*partner-repacks*',
                          '--exclude=*.checksums',
                          '--exclude=*.checksums.asc',
                          '--exclude=logs',
                          '--exclude=jsshell*',
                          '--exclude=host',
                          '--exclude=*.json',
                          '--exclude=*mar-tools*',
                          '--exclude=gecko-unsigned-unaligned.apk',
                          '--exclude=robocop.apk',
                          ]

VIRUS_SCAN_CMD = ['nice', 'ionice', '-c2', '-n7',
                  'extract_and_run_command.py', '-j2', 'clamdscan', '-m',
                  '--no-summary', '--']

PARTNER_BUNDLE_DIR = '/mnt/netapp/stage/releases.mozilla.com/bundles'
# Left side is destination relative to PARTNER_BUNDLE_DIR.
# Right side is source, relative to partner-repacks in the candidates dir.
PARTNER_BUNDLE_MAPPINGS = {
    r'bing/mac/en-US/Firefox-Bing.dmg': r'bing/mac/en-US/Firefox\ %(version)s.dmg',
    r'bing/win32/en-US/Firefox-Bing\ Setup.exe': r'bing/win32/en-US/Firefox\ Setup\ %(version)s.exe',
}


def checkStagePermissions(productName, version, buildNumber, stageServer,
                          stageUsername, stageSshKey):
    # The following commands should return 0 lines output and exit code 0
    tests = ["find %%s ! -user %s ! -path '*/contrib*'" % stageUsername,
             "find %%s ! -group `id -g -n %s` ! -path '*/contrib*'" % stageUsername,
             "find %s -type f ! -perm 644",
             "find %s -mindepth 1 -type d ! -perm 755 ! -path '*/contrib*' ! -path '*/partner-repacks*'",
             "find %s -maxdepth 1 -type d ! -perm 2775 -path '*/contrib*'",
             ]
    candidates_dir = makeCandidatesDir(productName, version, buildNumber)

    errors = False
    for test_template in tests:
        test = test_template % candidates_dir
        cmd = 'test "0" = "$(%s | wc -l)"' % test
        try:
            run_remote_cmd(cmd, server=stageServer,
                           username=stageUsername, sshKey=stageSshKey)
        except CalledProcessError:
            errors = True

    if errors:
        raise


def runAntivirusCheck(productName, version, buildNumber, stageServer,
                      stageUsername=None, stageSshKey=None):
    candidates_dir = makeCandidatesDir(productName, version, buildNumber)
    cmd = VIRUS_SCAN_CMD + [candidates_dir]
    run_remote_cmd(cmd, server=stageServer, username=stageUsername,
                   sshKey=stageSshKey)


def pushToMirrors(productName, version, buildNumber, stageServer,
                  stageUsername=None, stageSshKey=None, excludes=None,
                  extra_excludes=None, dryRun=False, overwrite=False):
    """ excludes overrides DEFAULT_RSYNC_EXCLUDES, extra_exludes will be
    appended to DEFAULT_RSYNC_EXCLUDES. """

    source_dir = makeCandidatesDir(productName, version, buildNumber)
    target_dir = makeReleasesDir(productName, version)

    if not excludes:
        excludes = DEFAULT_RSYNC_EXCLUDES
    if extra_excludes:
        excludes += ['--exclude=%s' % ex for ex in extra_excludes]

    # fail/warn if target directory exists depending on dry run mode
    try:
        run_remote_cmd(['test', '!', '-d', target_dir], server=stageServer,
                       username=stageUsername, sshKey=stageSshKey)
    except CalledProcessError:
        if overwrite:
            log.info('target directory %s exists, but overwriting files as requested' % target_dir)
        elif dryRun:
            log.warning('WARN: target directory %s exists', target_dir)
        else:
            raise

    if not dryRun:
        run_remote_cmd(['mkdir', '-p', target_dir], server=stageServer,
                       username=stageUsername, sshKey=stageSshKey)
        run_remote_cmd(
            ['chmod', 'u=rwx,g=rxs,o=rx', target_dir], server=stageServer,
            username=stageUsername, sshKey=stageSshKey)
    rsync_cmd = ['rsync', '-av']
    if dryRun:
        rsync_cmd.append('-n')
    # use hardlinks
    rsync_cmd.append('--link-dest=%s' % source_dir)
    run_remote_cmd(rsync_cmd + excludes + [source_dir, target_dir],
                   server=stageServer, username=stageUsername,
                   sshKey=stageSshKey)


indexFileTemplate = """\
<!DOCTYPE html>
<html><head>
<meta http-equiv="content-type" content="text/html; charset=UTF-8">
<style media="all">@import "http://www.mozilla.org/style/firefox/4.0beta/details.css";</style>
<title>Thanks for your interest in Firefox %(version)s</title>
</head>
<body>
<h1>Thanks for your interest in Firefox %(version)s</h1>
<p>We aren't quite finished qualifying Firefox %(version)s yet. You should check out the latest <a href="http://www.mozilla.org/firefox/channel">Beta</a>.</p>
<p>When we're all done with Firefox %(version)s it will show up on <a href="http://firefox.com?ref=ftp">Firefox.com</a>.</p>
</body>
</html>"""


def makeIndexFiles(productName, version, buildNumber, stageServer,
                   stageUsername, stageSshKey):
    candidates_dir = makeCandidatesDir(productName, version, buildNumber)
    indexFile = NamedTemporaryFile()
    indexFile.write(indexFileTemplate % {'version': version})
    indexFile.flush()

    scp(
        indexFile.name, '%s@%s:%s/index.html' % (
            stageUsername, stageServer, candidates_dir),
        sshKey=stageSshKey)
    run_remote_cmd(['chmod', '644', '%s/index.html' % candidates_dir],
                   server=stageServer, username=stageUsername, sshKey=stageSshKey)
    run_remote_cmd(
        ['find', candidates_dir, '-mindepth', '1', '-type', 'd', '-not', '-regex', '.*contrib.*', '-exec', 'cp', '-pv', '%s/index.html' % candidates_dir, '{}', '\\;'],
        server=stageServer, username=stageUsername, sshKey=stageSshKey)


def deleteIndexFiles(cleanup_dir, stageServer, stageUsername,
                     stageSshKey):
    run_remote_cmd(
        ['find', cleanup_dir, '-name', 'index.html', '-exec', 'rm',
            '-v', '{}', '\\;'],
        server=stageServer, username=stageUsername, sshKey=stageSshKey)


def updateSymlink(productName, version, stageServer, stageUsername,
                  stageSshKey, target):
    # step 1 check if we have already pushed to mirrors (bug 1083208)
    push_dir = makeReleasesDir(productName, version)
    try:
        # check if the remote dir exists
        run_remote_cmd(['test', '-d', push_dir],
                       server=stageServer, username=stageUsername,
                       sshKey=stageSshKey)
    except CalledProcessError:
            log.error('ERROR: push to mirrors directory, %s, does not exist on %s'
                      % (push_dir, stageServer))
            log.error('ERROR: Did you push to mirrors before running post release?')
            raise

    releases_dir = makeReleasesDir(productName)
    run_remote_cmd([
        'cd %(rd)s && rm -f %(target)s && ln -s %(version)s %(target)s' %
        dict(rd=releases_dir, version=version, target=target)],
        server=stageServer, username=stageUsername, sshKey=stageSshKey)


def doSyncPartnerBundles(productName, version, buildNumber, stageServer,
                         stageUsername, stageSshKey):
    candidates_dir = makeCandidatesDir(productName, version, buildNumber)

    for dest, src in PARTNER_BUNDLE_MAPPINGS.iteritems():
        full_dest = path.join(PARTNER_BUNDLE_DIR, dest)
        full_src = path.join(candidates_dir, 'partner-repacks', src)
        full_src = full_src % {'version': version}
        run_remote_cmd(
            ['cp', '-f', full_src, full_dest],
            server=stageServer, username=stageUsername, sshKey=stageSshKey
        )

    # And fix the permissions...
    run_remote_cmd(
        ['find', PARTNER_BUNDLE_DIR, '-type', 'd',
         '-exec', 'chmod', '775', '{}', '\\;'],
        server=stageServer, username=stageUsername, sshKey=stageSshKey
    )
    run_remote_cmd(
        ['find', PARTNER_BUNDLE_DIR, '-name', '"*.exe"',
         '-exec', 'chmod', '775', '{}', '\\;'],
        server=stageServer, username=stageUsername, sshKey=stageSshKey
    )
    run_remote_cmd(
        ['find', PARTNER_BUNDLE_DIR, '-name', '"*.dmg"',
         '-exec', 'chmod', '775', '{}', '\\;'],
        server=stageServer, username=stageUsername, sshKey=stageSshKey
    )


def update_bouncer_aliases(tuxedoServerUrl, auth, version, bouncer_aliases):
    for related_product_template, alias in bouncer_aliases.iteritems():
        update_bouncer_alias(tuxedoServerUrl, auth, version,
                             related_product_template, alias)


def update_bouncer_alias(tuxedoServerUrl, auth, version,
                         related_product_template, alias):
    url = "%s/create_update_alias" % tuxedoServerUrl
    related_product = related_product_template % {"version": version}

    data = {"alias": alias, "related_product": related_product}
    log.info("Updating %s to point to %s using %s", alias, related_product,
             url)

    # Wrap the real call to hide credentials from retry's logging
    def do_update_bouncer_alias():
        r = requests.post(url, data=data, auth=auth, verify=False)
        r.raise_for_status()

    retry(do_update_bouncer_alias)


def updateProductDetails(productName, version, productDetailsRepo, mozillaComRepo, svnSshKey, dryRun=False):
    """
    Add a new version to the product details
    """
    os.environ["SVN_SSH"] = "ssh -i %s" % svnSshKey
    cwd = os.getcwd()
    pdDir = path.join(cwd, "product-details.svn")
    mcDir = path.join(cwd, "mozilla.com.svn")
    retry(checkoutSVN, args=(pdDir, productDetailsRepo), attempts=3)
    retry(checkoutSVN, args=(mcDir, mozillaComRepo), attempts=3)
    # Update the PHP files
    updateProductDetailFiles(pdDir, productName, version)
    # Export to json
    exportJSON(pdDir)
    # Commit to svn
    commitSVN(pdDir, productName, version, dryRun)
    # Get the svn revision of the p-d repository
    svnRev = getSVNrev(pdDir)
    # Update Mozilla.com
    updateRev(mcDir, svnRev)
    # commit Mozilla.com
    commitSVN(pdDir, productName, version, dryRun)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.set_defaults(
    )
    parser.add_argument("-r", "--release-config", required=True,
                        help="Release config file location relative to "
                        "buildbot-configs repo root")
    parser.add_argument("-b", "--buildbot-configs",
                        help="buildbot-configs mercurial repo URL",
                        default=os.environ.get("BUILDBOT_CONFIGS",
                                               DEFAULT_BUILDBOT_CONFIGS_REPO))
    parser.add_argument("-t", "--release-tag", required=True)
    parser.add_argument("--product", help="Override for stage_product")
    parser.add_argument("--ssh-user", required=True)
    parser.add_argument("--ssh-key", required=True)
    parser.add_argument("--overwrite", default=False, action="store_true")
    parser.add_argument("--extra-excludes", action="append")
    parser.add_argument("actions", nargs="+", help="Script actions")

    args = parser.parse_args()
    actions = args.actions
    mercurial(args.buildbot_configs, "buildbot-configs")
    update("buildbot-configs", revision=args.release_tag)

    releaseConfigFile = path.join("buildbot-configs", args.release_config)
    releaseConfig = readReleaseConfig(
        releaseConfigFile, required=REQUIRED_RELEASE_CONFIG)

    productName = args.product or releaseConfig['stage_product']
    version = releaseConfig['version']
    buildNumber = releaseConfig['buildNumber']
    stageServer = releaseConfig['stagingServer']
    stageUsername = args.ssh_user
    stageSshKey = args.ssh_key
    stageSshKey = path.join(os.path.expanduser("~"), ".ssh", stageSshKey)
    createIndexFiles = releaseConfig.get('makeIndexFiles', False) \
        and productName != 'xulrunner'
    syncPartnerBundles = releaseConfig.get('syncPartnerBundles', False) \
        and productName != 'xulrunner'
    ftpSymlinkName = releaseConfig.get('ftpSymlinkName')
    bouncer_aliases = releaseConfig.get('bouncer_aliases')
    productDetailsRepo = releaseConfig.get('productDetailsRepo')
    mozillaComRepo = releaseConfig.get('mozillaComRepo')
    svnSshKey = releaseConfig.get("svnSshKey")

    if 'permissions' in actions:
        checkStagePermissions(stageServer=stageServer,
                              stageUsername=stageUsername,
                              stageSshKey=stageSshKey,
                              productName=productName,
                              version=version,
                              buildNumber=buildNumber)

    if 'antivirus' in actions:
        runAntivirusCheck(stageServer=stageServer,
                          stageUsername=stageUsername,
                          stageSshKey=stageSshKey,
                          productName=productName,
                          version=version,
                          buildNumber=buildNumber)

    if 'permissions' in actions or 'antivirus' in actions:
        pushToMirrors(stageServer=stageServer,
                      stageUsername=stageUsername,
                      stageSshKey=stageSshKey,
                      productName=productName,
                      version=version,
                      buildNumber=buildNumber,
                      extra_excludes=args.extra_excludes,
                      dryRun=True)

    if 'push' in actions:
        if createIndexFiles:
            makeIndexFiles(stageServer=stageServer,
                           stageUsername=stageUsername,
                           stageSshKey=stageSshKey,
                           productName=productName,
                           version=version,
                           buildNumber=buildNumber)
        pushToMirrors(stageServer=stageServer,
                      stageUsername=stageUsername,
                      stageSshKey=stageSshKey,
                      productName=productName,
                      version=version,
                      extra_excludes=args.extra_excludes,
                      buildNumber=buildNumber,
                      overwrite=args.overwrite)
        if createIndexFiles:
            deleteIndexFiles(stageServer=stageServer,
                             stageUsername=stageUsername,
                             stageSshKey=stageSshKey,
                             cleanup_dir=makeCandidatesDir(productName, version, buildNumber))

    if 'postrelease' in actions:
        if createIndexFiles:
            deleteIndexFiles(stageServer=stageServer,
                             stageUsername=stageUsername,
                             stageSshKey=stageSshKey,
                             cleanup_dir=makeReleasesDir(productName, version))
        if ftpSymlinkName:
            updateSymlink(stageServer=stageServer,
                          stageUsername=stageUsername,
                          stageSshKey=stageSshKey,
                          productName=productName,
                          version=version,
                          target=ftpSymlinkName)
        if syncPartnerBundles:
            doSyncPartnerBundles(stageServer=stageServer,
                                 stageUsername=stageUsername,
                                 stageSshKey=stageSshKey,
                                 productName=productName,
                                 version=version,
                                 buildNumber=buildNumber)
        if bouncer_aliases and productName != 'xulrunner':
            credentials_file = path.join(os.getcwd(), "oauth.txt")
            credentials = readConfig(
                credentials_file,
                required=["tuxedoUsername", "tuxedoPassword"])
            auth = (credentials["tuxedoUsername"],
                    credentials["tuxedoPassword"])

            update_bouncer_aliases(
                tuxedoServerUrl=releaseConfig["tuxedoServerUrl"],
                auth=auth,
                version=version,
                bouncer_aliases=bouncer_aliases)

    if 'product-details' in args:
        updateProductDetails(
            productName,
            version,
            productDetailsRepo,
            mozillaComRepo,
            svnSshKey,
        )
