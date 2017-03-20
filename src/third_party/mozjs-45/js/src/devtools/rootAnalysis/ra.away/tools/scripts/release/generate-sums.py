#!/usr/bin/env python


import logging
import os
from os import path
import sys

sys.path.append(path.join(path.dirname(__file__), "../../lib/python"))
logging.basicConfig(
    stream=sys.stdout, level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)

from release.info import readReleaseConfig, readBranchConfig
from util.hg import update, mercurial, make_hg_url
from util.commands import run_remote_cmd

from release.download import rsyncFilesByPattern, rsyncFiles
from release.signing import generateChecksums, signFiles
from release.paths import makeCandidatesDir

DEFAULT_BUILDBOT_CONFIGS_REPO = make_hg_url('hg.mozilla.org',
                                            'build/buildbot-configs')

REQUIRED_BRANCH_CONFIG = ("stage_server", "stage_username", "stage_ssh_key")
REQUIRED_RELEASE_CONFIG = ("productName", "version", "buildNumber")


def validate(options, args):
    if not options.configfile:
        log.info("Must pass --configfile")
        sys.exit(1)
    releaseConfigFile = path.join("buildbot-configs", options.releaseConfig)
    branchConfigFile = path.join("buildbot-configs", options.configfile)
    branchConfigDir = path.dirname(branchConfigFile)

    if not path.exists(branchConfigFile):
        log.info("%s does not exist!" % branchConfigFile)
        sys.exit(1)

    releaseConfig = readReleaseConfig(releaseConfigFile,
                                      required=REQUIRED_RELEASE_CONFIG)
    sourceRepoName = releaseConfig['sourceRepositories'][
        options.sourceRepoKey]['name']
    branchConfig = readBranchConfig(branchConfigDir, branchConfigFile,
                                    sourceRepoName,
                                    required=REQUIRED_BRANCH_CONFIG)
    return branchConfig, releaseConfig


if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser("")

    parser.set_defaults(
        buildbotConfigs=os.environ.get("BUILDBOT_CONFIGS",
                                       DEFAULT_BUILDBOT_CONFIGS_REPO),
        sourceRepoKey="mozilla",
    )
    parser.add_option("-c", "--configfile", dest="configfile")
    parser.add_option("-r", "--release-config", dest="releaseConfig")
    parser.add_option("-b", "--buildbot-configs", dest="buildbotConfigs")
    parser.add_option("-t", "--release-tag", dest="releaseTag")
    parser.add_option("--source-repo-key", dest="sourceRepoKey")
    parser.add_option("--product", dest="product")
    parser.add_option("--ssh-user", dest="ssh_username")
    parser.add_option("--ssh-key", dest="ssh_key")
    parser.add_option("--create-contrib-dirs", dest="create_contrib_dirs",
                      action="store_true")

    options, args = parser.parse_args()
    mercurial(options.buildbotConfigs, "buildbot-configs")
    update("buildbot-configs", revision=options.releaseTag)

    branchConfig, releaseConfig = validate(options, args)

    productName = options.product or releaseConfig['productName']
    version = releaseConfig['version']
    buildNumber = releaseConfig['buildNumber']
    stageServer = branchConfig['stage_server']
    stageUsername = options.ssh_username or branchConfig['stage_username']
    stageSshKey = options.ssh_key or branchConfig["stage_ssh_key"]
    stageSshKey = path.join(os.path.expanduser("~"), ".ssh", stageSshKey)

    candidatesDir = makeCandidatesDir(productName, version, buildNumber)
    rsyncFilesByPattern(server=stageServer, userName=stageUsername,
                        sshKey=stageSshKey, source_dir=candidatesDir,
                        target_dir='temp/', pattern='*.checksums')
    types = {'sha1': 'SHA1SUMS', 'md5': 'MD5SUMS', 'sha512': 'SHA512SUMS'}
    generateChecksums('temp', types)
    files = types.values()
    signFiles(files)
    upload_files = files + ['%s.asc' % x for x in files] + \
        [path.join(path.dirname(__file__), 'KEY')]
    log.info("Fixing permissions...")
    for f in upload_files:
        log.info("chmod 644 %s" % f)
        os.chmod(f, 0644)
    rsyncFiles(files=upload_files, server=stageServer, userName=stageUsername,
               sshKey=stageSshKey, target_dir=candidatesDir)
    if options.create_contrib_dirs:
        cmd = 'mkdir -v -m 2775 %s/contrib %s/contrib-localized' % \
            (candidatesDir, candidatesDir)
        run_remote_cmd(cmd, server=stageServer, username=stageUsername,
                       sshKey=stageSshKey)
