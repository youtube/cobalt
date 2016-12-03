#!/usr/bin/env python

import logging
import os
from os import path
from traceback import format_exc, print_exc
import site
import sys
from distutils.version import LooseVersion

site.addsitedir(path.join(path.dirname(__file__), "../../lib/python"))
site.addsitedir(path.join(path.dirname(__file__), "../../lib/python/vendor"))

from balrog.submitter.cli import ReleaseSubmitterV3
from build.checksums import parseChecksumsFile
from build.l10n import repackLocale, l10nRepackPrep
from build.paths import get_repo_dirname
import build.misc
from build.upload import postUploadCmdPrefix
from release.download import downloadReleaseBuilds, downloadUpdateIgnore404
from release.info import readReleaseConfig, readConfig, fileInfo
from release.l10n import getReleaseLocalesForChunk
from util.hg import mercurial, update, make_hg_url
from util.retry import retry
from release.info import getBuildID

logging.basicConfig(
    stream=sys.stdout, level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)

HG = "hg.mozilla.org"
DEFAULT_BUILDBOT_CONFIGS_REPO = make_hg_url(HG, "build/buildbot-configs")


class RepackError(Exception):
    pass


def createRepacks(sourceRepo, revision, l10nRepoDir, l10nBaseRepo,
                  mozconfigPath, srcMozconfigPath, objdir, makeDirs, appName,
                  locales, product, version, buildNumber,
                  stageServer, stageUsername, stageSshKey,
                  compareLocalesRepo, merge, platform, brand, appVersion,
                  generatePartials=False, partialUpdates=None,
                  usePymake=False, tooltoolManifest=None,
                  tooltool_script=None, tooltool_urls=None,
                  balrog_submitter=None, balrog_hash="sha512",
                  mozillaDir=None, mozillaSrcDir=None):
    buildid = retry(getBuildID, args=(platform, product, version,
                                      buildNumber, 'candidates', stageServer))
    log.info('Got buildid: %s' % buildid)
    sourceRepoName = path.split(sourceRepo)[-1]
    absObjdir = path.abspath(path.join(sourceRepoName, objdir))
    localeSrcDir = path.join(absObjdir, appName, "locales")
    # Even on Windows we need to use "/" as a separator for this because
    # compare-locales doesn"t work any other way
    l10nIni = "/".join([sourceRepoName, appName, "locales", "l10n.ini"])
    env = {
        "MOZ_OBJDIR": objdir,
        "MOZ_MAKE_COMPLETE_MAR": "1",
        "UPLOAD_HOST": stageServer,
        "UPLOAD_USER": stageUsername,
        "UPLOAD_SSH_KEY": stageSshKey,
        "UPLOAD_TO_TEMP": "1",
        "MOZ_PKG_PRETTYNAMES": "1",
        "MOZILLA_REV": os.getenv('MOZILLA_REV', ''),
        "COMM_REV": os.getenv('COMM_REV', ''),
        "LD_LIBRARY_PATH": os.getenv("LD_LIBRARY_PATH", ""),
        "MBSDIFF_HOOK": os.getenv("MBSDIFF_HOOK", ""),
    }
    if appVersion is None or version != appVersion:
        env["MOZ_PKG_VERSION"] = version
    signed = False
    if os.environ.get('MOZ_SIGN_CMD'):
        env['MOZ_SIGN_CMD'] = os.environ['MOZ_SIGN_CMD']
        signed = True
    env['POST_UPLOAD_CMD'] = postUploadCmdPrefix(
        to_candidates=True,
        product=product,
        version=version,
        buildNumber=buildNumber,
        signed=signed,
    )
    if usePymake:
        env['USE_PYMAKE'] = "1"
        env['MOZILLA_OFFICIAL'] = "1"
        env["MOZ_SIGN_CMD"] = "python " + \
            path.join(os.getcwd(), "scripts", "release", "signing", "signtool.py").replace('\\', '\\\\\\\\') + \
            " --cachedir " + \
            path.join(os.getcwd(), "signing_cache").replace('\\', '\\\\\\\\') + \
            " -t " + \
            path.join(os.getcwd(), "token").replace('\\', '\\\\\\\\') + \
            " -n " + \
            path.join(os.getcwd(), "nonce").replace('\\', '\\\\\\\\') + \
            " -c " + \
            path.join(os.getcwd(), "scripts", "release", "signing", "host.cert").replace('\\', '\\\\\\\\')
        signingServers = os.environ["MOZ_SIGN_CMD"].split("-H", 1)[1].split("-H")
        for s in signingServers:
            env["MOZ_SIGN_CMD"] += " -H %s" % s.strip()
    build.misc.cleanupObjdir(sourceRepoName, objdir, appName)
    mercurial(sourceRepo, sourceRepoName)
    update(sourceRepoName, revision=revision)
    l10nRepackPrep(
        sourceRepoName, objdir, mozconfigPath, srcMozconfigPath, l10nRepoDir,
        makeDirs, env, tooltoolManifest, tooltool_script, tooltool_urls)
    input_env = retry(downloadReleaseBuilds,
                      args=(stageServer, product, brand, version, buildNumber,
                            platform),
                      kwargs={'signed': signed,
                              'usePymake': usePymake})
    env.update(input_env)

    failed = []
    for l in locales:
        try:
            if generatePartials:
                for oldVersion in partialUpdates:
                    oldBuildNumber = partialUpdates[oldVersion]['buildNumber']
                    partialUpdates[oldVersion]['mar'] = retry(
                        downloadUpdateIgnore404,
                        args=(stageServer, product, oldVersion, oldBuildNumber,
                              platform, l)
                    )
            checksums_file = repackLocale(locale=l, l10nRepoDir=l10nRepoDir,
                                          l10nBaseRepo=l10nBaseRepo, revision=revision,
                                          localeSrcDir=localeSrcDir, l10nIni=l10nIni,
                                          compareLocalesRepo=compareLocalesRepo, env=env,
                                          absObjdir=absObjdir, merge=merge,
                                          productName=product, platform=platform,
                                          version=version, partialUpdates=partialUpdates,
                                          buildNumber=buildNumber, stageServer=stageServer,
                                          mozillaDir=mozillaDir, mozillaSrcDir=mozillaSrcDir)

            if balrog_submitter:
                # TODO: partials, after bug 797033 is fixed
                checksums = parseChecksumsFile(open(checksums_file).read())
                completeInfo = []
                partialInfo = []
                for f, info in checksums.iteritems():
                    if f.endswith('.complete.mar'):
                        completeInfo.append({
                            "size": info["size"],
                            "hash": info["hashes"][balrog_hash],
                        })
                    if f.endswith('.partial.mar'):
                        pathInfo = fileInfo(f, product.lower())
                        previousVersion = pathInfo["previousVersion"]
                        partialInfo.append({
                            "previousVersion": previousVersion,
                            "previousBuildNumber": partialUpdates[previousVersion]['buildNumber'],
                            "size": info["size"],
                            "hash": info["hashes"][balrog_hash],
                        })
                if not completeInfo:
                    raise Exception("Couldn't find complete mar info")
                retry(balrog_submitter.run,
                    kwargs={
                        'platform': platform,
                        'productName': product.capitalize(),
                        'appVersion': appVersion,
                        'version': version,
                        'build_number': buildNumber,
                        'locale': l,
                        'hashFunction': balrog_hash,
                        'extVersion': appVersion,
                        'buildID': buildid,
                        'completeInfo': completeInfo,
                        'partialInfo': partialInfo,
                    }
                )
        except Exception, e:
            print_exc()
            failed.append((l, format_exc()))

    if len(failed) > 0:
        log.error("The following tracebacks were detected during repacks:")
        for l, e in failed:
            log.error("%s:" % l)
            log.error("%s\n" % e)
        raise Exception(
            "Failed locales: %s" % " ".join([x for x, _ in failed]))

REQUIRED_BRANCH_CONFIG = ("stage_server", "stage_username", "stage_ssh_key",
                          "compare_locales_repo_path", "hghost")
REQUIRED_RELEASE_CONFIG = ("sourceRepositories", "l10nRepoPath", "appName",
                           "productName", "version", "buildNumber", "appVersion")


def validate(options, args):
    if not options.configfile:
        log.info("Must pass --configfile")
        sys.exit(1)
    releaseConfigFile = "/".join(["buildbot-configs", options.releaseConfig])

    if options.chunks or options.thisChunk:
        assert options.chunks and options.thisChunk, \
            "chunks and this-chunk are required when one is passed"
        assert not options.locales, \
            "locale option cannot be used when chunking"
    else:
        if len(options.locales) < 1:
            raise Exception('Need at least one locale to repack')

    if options.balrog_api_root:
        if not options.credentials_file or not options.balrog_username:
            raise Exception("--credentials-file and --balrog-username must be set when --balrog-api-root is set.")

    releaseConfig = readReleaseConfig(releaseConfigFile,
                                      required=REQUIRED_RELEASE_CONFIG)
    branchConfig = {
        'stage_ssh_key': options.stage_ssh_key,
        'hghost': options.hghost,
        'stage_server': options.stage_server,
        'stage_username': options.stage_username,
        'compare_locales_repo_path': options.compare_locales_repo_path,
    }
    return branchConfig, releaseConfig

if __name__ == "__main__":
    from optparse import OptionParser
    parser = OptionParser("")

    makeDirs = ["config"]

    parser.set_defaults(
        buildbotConfigs=os.environ.get("BUILDBOT_CONFIGS",
                                       DEFAULT_BUILDBOT_CONFIGS_REPO),
        locales=[],
        chunks=None,
        thisChunk=None,
        objdir="obj-l10n",
        source_repo_key="mozilla"
    )
    parser.add_option("-c", "--configfile", dest="configfile")
    parser.add_option("-r", "--release-config", dest="releaseConfig")
    parser.add_option("-b", "--buildbot-configs", dest="buildbotConfigs")
    parser.add_option("-t", "--release-tag", dest="releaseTag")
    parser.add_option("-p", "--platform", dest="platform")
    parser.add_option("-o", "--objdir", dest="objdir")
    parser.add_option("-l", "--locale", dest="locales", action="append")
    parser.add_option("--source-repo-key", dest="source_repo_key")
    parser.add_option("--chunks", dest="chunks", type="int")
    parser.add_option("--this-chunk", dest="thisChunk", type="int")
    parser.add_option("--generate-partials", dest="generatePartials",
                      action='store_true', default=False)
    parser.add_option("--stage-ssh-key", dest="stage_ssh_key")
    parser.add_option("--hghost", dest="hghost")
    parser.add_option("--stage-server", dest="stage_server")
    parser.add_option("--stage-username", dest="stage_username")
    parser.add_option(
        "--compare-locales-repo-path", dest="compare_locales_repo_path")
    parser.add_option("--properties-dir", dest="properties_dir")
    parser.add_option("--tooltool-manifest", dest="tooltool_manifest")
    parser.add_option("--tooltool-script", dest="tooltool_script",
                      default=[], action="append")
    parser.add_option("--tooltool-url", dest="tooltool_urls", action="append")
    parser.add_option("--use-pymake", dest="use_pymake",
                      action="store_true", default=False)
    # todo: maybe read these from branch/release config? is that even possible for credentials file?
    parser.add_option("--balrog-api-root", dest="balrog_api_root")
    parser.add_option("--credentials-file", dest="credentials_file")
    parser.add_option("--balrog-username", dest="balrog_username")

    options, args = parser.parse_args()
    mercurial(options.buildbotConfigs, "buildbot-configs")
    update("buildbot-configs", revision=options.releaseTag)
    sys.path.append(os.getcwd())
    branchConfig, releaseConfig = validate(options, args)
    sourceRepoInfo = releaseConfig["sourceRepositories"][
        options.source_repo_key]

    try:
        brandName = releaseConfig["brandName"]
    except KeyError:
        brandName = releaseConfig["productName"].title()

    platform = options.platform
    if platform == "linux":
        platform = "linux32"
    mozconfig = path.join(get_repo_dirname(sourceRepoInfo["path"]),
                          releaseConfig["appName"], "config", "mozconfigs",
                          platform, "l10n-mozconfig")

    if options.chunks:
        locales = retry(getReleaseLocalesForChunk,
                        args=(
                        releaseConfig[
                        "productName"], releaseConfig["appName"],
                        releaseConfig[
                        "version"], int(releaseConfig["buildNumber"]),
                        sourceRepoInfo["path"], options.platform,
                        options.chunks, options.thisChunk)
                        )
    else:
        locales = options.locales

    if options.properties_dir:
        # Output a list of the locales into the properties directory. This will
        # allow consumers of the Buildbot JSON to know which locales were built
        # in a particular repack chunk.
        localeProps = path.normpath(path.join(options.properties_dir, 'locales'))
        f = open(localeProps, 'w+')
        f.write('locales:%s' % ','.join(locales))
        f.close()

    l10nRepoDir = 'l10n'

    stageSshKey = path.join("~", ".ssh", branchConfig["stage_ssh_key"])

    mozillaDir = None
    mozillaSrcDir = None
    # If mozilla_dir is defined, extend the paths in makeDirs with the prefix
    # of the mozilla_dir
    if 'mozilla_dir' in releaseConfig:
        for i in range(0, len(makeDirs)):
            makeDirs[i] = path.join(releaseConfig['mozilla_dir'], makeDirs[i])
        mozillaDir = releaseConfig['mozilla_dir']
        mozillaSrcDir = releaseConfig['mozilla_dir']
    elif 'mozilla_srcdir' in releaseConfig:
        mozillaSrcDir = releaseConfig['mozilla_srcdir']

    if not options.tooltool_script:
        options.tooltool_script = ['/tools/tooltool.py']

    if options.balrog_api_root:
        credentials = readConfig(options.credentials_file,
            required=['balrog_credentials']
        )
        auth = (options.balrog_username, credentials['balrog_credentials'][options.balrog_username])
        balrog_submitter = ReleaseSubmitterV3(options.balrog_api_root, auth)
    else:
        balrog_submitter = None

    partialUpdates = releaseConfig.get('partialUpdates', {}).copy()
    # FIXME: the follwong hack can be removed when win64 has the same list of
    # partial update as other platforms. Check mozilla-esr38 to be sure.
    if platform in releaseConfig.get('HACK_first_released_version', {}):
        partialUpdates_copy = {}
        for k, v in partialUpdates.iteritems():
            if LooseVersion(k) >= LooseVersion(releaseConfig['HACK_first_released_version'][platform]):
                partialUpdates_copy[k] = v
        partialUpdates = partialUpdates_copy
    # FIXME: end of hack

    createRepacks(
        sourceRepo=make_hg_url(branchConfig["hghost"], sourceRepoInfo["path"]),
        revision=options.releaseTag,
        l10nRepoDir=l10nRepoDir,
        l10nBaseRepo=make_hg_url(branchConfig["hghost"],
                                 releaseConfig["l10nRepoPath"]),
        mozconfigPath=mozconfig,
        srcMozconfigPath=releaseConfig.get('l10n_mozconfigs', {}).get(options.platform),
        objdir=options.objdir,
        makeDirs=makeDirs,
        appName=releaseConfig["appName"],
        locales=locales,
        product=releaseConfig["productName"],
        version=releaseConfig["version"],
        appVersion=releaseConfig["appVersion"],
        buildNumber=int(releaseConfig["buildNumber"]),
        stageServer=branchConfig["stage_server"],
        stageUsername=branchConfig["stage_username"],
        stageSshKey=stageSshKey,
        compareLocalesRepo=make_hg_url(branchConfig["hghost"],
                                       branchConfig[
                                           "compare_locales_repo_path"]),
        merge=releaseConfig["mergeLocales"],
        platform=options.platform,
        brand=brandName,
        generatePartials=options.generatePartials,
        partialUpdates=partialUpdates,
        usePymake=options.use_pymake,
        tooltoolManifest=options.tooltool_manifest,
        tooltool_script=options.tooltool_script,
        tooltool_urls=options.tooltool_urls,
        balrog_submitter=balrog_submitter,
        mozillaDir=mozillaDir,
        mozillaSrcDir=mozillaSrcDir
    )
