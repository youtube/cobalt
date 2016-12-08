#!/usr/bin/env python

import logging
import os
from os import path
import sys
from tempfile import mkstemp

sys.path.append(path.join(path.dirname(__file__), "../../../lib/python"))
logging.basicConfig(
    stream=sys.stdout, level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)

from release.info import readReleaseConfig
from release.updates.verify import UpdateVerifyConfig
from util.commands import run_cmd
from util.hg import mercurial, update, make_hg_url

HG = "hg.mozilla.org"
DEFAULT_BUILDBOT_CONFIGS_REPO = make_hg_url(HG, "build/buildbot-configs")
UPDATE_VERIFY_COMMAND = ["bash", "verify.sh", "-c"]
UPDATE_VERIFY_DIR = path.join(
    path.dirname(__file__), "../../../release/updates")


def validate(options, args):
    assert options.chunks and options.thisChunk, \
        "chunks and this-chunk are required"

    releaseConfigFile = path.join("buildbot-configs", options.releaseConfig)
    releaseConfig = readReleaseConfig(releaseConfigFile,
                                      required=(options.configDict,))
    uvConfig = path.join(UPDATE_VERIFY_DIR,
                         releaseConfig[options.configDict][options.release_channel]["verifyConfigs"][options.platform])
    assert path.isfile(uvConfig), "Update verify config must exist!"
    return releaseConfig

if __name__ == "__main__":
    from optparse import OptionParser
    parser = OptionParser("")

    parser.set_defaults(
        buildbotConfigs=os.environ.get("BUILDBOT_CONFIGS",
                                       DEFAULT_BUILDBOT_CONFIGS_REPO),
        configDict="updateChannels",
        chunks=None,
        thisChunk=None,
    )
    parser.add_option("--config-dict", dest="configDict")
    parser.add_option("-t", "--release-tag", dest="releaseTag")
    parser.add_option("-r", "--release-config", dest="releaseConfig")
    parser.add_option("-b", "--buildbot-configs", dest="buildbotConfigs")
    parser.add_option("-p", "--platform", dest="platform")
    parser.add_option("-C", "--release-channel", dest="release_channel")
    parser.add_option("--chunks", dest="chunks", type="int")
    parser.add_option("--this-chunk", dest="thisChunk", type="int")

    options, args = parser.parse_args()
    mercurial(options.buildbotConfigs, "buildbot-configs")
    update("buildbot-configs", revision=options.releaseTag)
    releaseConfig = validate(options, args)
    verifyConfigFile = releaseConfig[options.configDict][options.release_channel]["verifyConfigs"][options.platform]

    fd, configFile = mkstemp()
    fh = os.fdopen(fd, "w")
    try:
        verifyConfig = UpdateVerifyConfig()
        verifyConfig.read(path.join(UPDATE_VERIFY_DIR, verifyConfigFile))
        myVerifyConfig = verifyConfig.getChunk(
            options.chunks, options.thisChunk)
        myVerifyConfig.write(fh)
        fh.close()
        run_cmd(["cat", configFile])
        run_cmd(UPDATE_VERIFY_COMMAND + [configFile], cwd=UPDATE_VERIFY_DIR)
    finally:
        if path.exists(configFile):
            os.unlink(configFile)
