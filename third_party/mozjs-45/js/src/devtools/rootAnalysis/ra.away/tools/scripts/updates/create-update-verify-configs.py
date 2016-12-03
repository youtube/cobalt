#!/usr/bin/env python

import site
import os
from os import path
import logging

site.addsitedir(path.join(path.dirname(__file__), "../../lib/python"))
site.addsitedir(path.join(path.dirname(__file__), "../../lib/python/vendor"))

from distutils.version import LooseVersion
from release.updates.patcher import PatcherConfig
from release.l10n import makeReleaseRepackUrls
from release.platforms import buildbot2updatePlatforms, buildbot2ftp
from release.paths import makeReleasesDir, makeCandidatesDir
from release.info import readReleaseConfig
from util.retry import retry
from util.hg import mercurial, make_hg_url, update
from release.updates.verify import UpdateVerifyConfig


HG = "hg.mozilla.org"
DEFAULT_BUILDBOT_CONFIGS_REPO = make_hg_url(HG, 'build/buildbot-configs')
DEFAULT_MAX_PUSH_ATTEMPTS = 10
REQUIRED_CONFIG = ('productName', 'buildNumber', 'ausServerUrl',
                   'stagingServer')
FTP_SERVER_TEMPLATE = 'http://%s/pub/mozilla.org'

logging.basicConfig(format="%(asctime)s - %(levelname)s - %(message)s")
log = logging.getLogger()


def validate(options):
    err = False
    config = {}

    if not path.exists(path.join('buildbot-configs', options.release_config)):
        log.error("%s does not exist!" % options.release_config)
        exit(1)

    config = readReleaseConfig(path.join('buildbot-configs',
                                         options.release_config))
    for key in REQUIRED_CONFIG:
        if key not in config:
            err = True
            log.error("Required item missing in config: %s" % key)

    if err:
        exit(1)

    return config


if __name__ == "__main__":
    from optparse import OptionParser
    parser = OptionParser("")

    parser.add_option("-c", "--config", dest="config")
    parser.add_option("--platform", dest="platform")
    parser.add_option("-r", "--release-config-file", dest="release_config",
                      help="The release config file to use.")
    parser.add_option("-b", "--buildbot-configs", dest="buildbot_configs",
                      help="The place to clone buildbot-configs from",
                      default=os.environ.get('BUILDBOT_CONFIGS_REPO',
                                             DEFAULT_BUILDBOT_CONFIGS_REPO))
    parser.add_option("-t", "--release-tag", dest="release_tag",
                      help="Release tag to update buildbot-configs to")
    parser.add_option("--channel", dest="channel", default="betatest")
    parser.add_option("--full-check-locale", dest="full_check_locales",
                      action="append", default=['de', 'en-US', 'ru'])
    parser.add_option("--output", dest="output")
    parser.add_option("-v", "--verbose", dest="verbose", default=False,
                      action="store_true")

    options, args = parser.parse_args()

    required_options = ['config', 'platform', 'release_config',
                        'buildbot_configs', 'release_tag']
    options_dict = vars(options)

    for opt in required_options:
        if not options_dict[opt]:
            parser.error("Required option %s not present" % opt)

    if options.verbose:
        log.setLevel(logging.DEBUG)
    else:
        log.setLevel(logging.INFO)

    ftp_platform = buildbot2ftp(options.platform)
    full_check_locales = options.full_check_locales

    # Variables from release config
    retry(mercurial, args=(options.buildbot_configs, 'buildbot-configs'))
    update('buildbot-configs', revision=options.release_tag)
    release_config = validate(options)
    product_name = release_config['productName']
    staging_server = FTP_SERVER_TEMPLATE % release_config['stagingServer']
    aus_server_url = release_config['ausServerUrl']
    build_number = release_config['buildNumber']
    previous_releases_staging_server = FTP_SERVER_TEMPLATE % \
        release_config.get('previousReleasesStagingServer',
                           release_config['stagingServer'])

    # Current version data
    pc = PatcherConfig(open(options.config).read())
    partials = pc['current-update']['partials'].keys()
    app_name = pc['appName']
    to_version = pc['current-update']['to']
    to_ = makeReleaseRepackUrls(
        product_name, app_name, to_version, options.platform,
        locale='%locale%', signed=True, exclude_secondary=True
    ).values()[0]
    candidates_dir = makeCandidatesDir(
        product_name, to_version, build_number, ftp_root='/')
    to_path = "%s%s" % (candidates_dir, to_)

    uvc = UpdateVerifyConfig(product=app_name, channel=options.channel,
                             aus_server=aus_server_url, to=to_path)

    # getUpdatePaths yields all of the update paths, but we need to know
    # everything about a fromVersion before we can add it to the update
    # verify config, so we need to collect everything it yields before
    # acting on it.
    updatePaths = {}
    for fromVersion, platform, locale, _, _ in pc.getUpdatePaths():
        # Skip paths from platforms we don't care about.
        if platform != ftp_platform:
            continue
        if fromVersion not in updatePaths:
            updatePaths[fromVersion] = []
        updatePaths[fromVersion].append(locale)

    for fromVersion in reversed(sorted(updatePaths, key=LooseVersion)):
        locales = updatePaths[fromVersion]
        from_ = pc["release"][fromVersion]
        appVersion = from_["extension-version"]
        build_id = from_["platforms"][ftp_platform]
        mar_channel_IDs = from_.get('mar-channel-ids')

        # Use new build targets for Windows, but only on compatible versions (42+)
        # See bug 1185456 for additional context.
        if options.platform not in ("win32", "win64") or LooseVersion(fromVersion) < LooseVersion("42.0"):
            update_platform = buildbot2updatePlatforms(options.platform)[0]
        else:
            update_platform = buildbot2updatePlatforms(options.platform)[1]

        path_ = makeReleaseRepackUrls(
            product_name, app_name, fromVersion, options.platform,
            locale='%locale%', signed=True, exclude_secondary=True
        ).values()[0]
        release_dir = makeReleasesDir(product_name, fromVersion, ftp_root='/')
        from_path = "%s%s" % (release_dir, path_)

        # Exclude locales being full checked
        quick_check_locales = [l for l in locales
                               if l not in full_check_locales]
        # Get the intersection of from and to full_check_locales
        this_full_check_locales = [l for l in full_check_locales
                                   if l in locales]

        if fromVersion in partials:
            log.info("Generating configs for partial update checks for %s" % fromVersion)
            uvc.addRelease(release=appVersion, build_id=build_id,
                           locales=locales,
                           patch_types=["complete", "partial"],
                           from_path=from_path, ftp_server_from=staging_server,
                           ftp_server_to=staging_server,
                           mar_channel_IDs=mar_channel_IDs,
                           platform=update_platform)
        else:
            if len(this_full_check_locales) > 0:
                log.info("Generating full check configs for %s" % fromVersion)
                uvc.addRelease(release=appVersion, build_id=build_id,
                               locales=this_full_check_locales, from_path=from_path,
                               ftp_server_from=previous_releases_staging_server,
                               ftp_server_to=staging_server,
                               mar_channel_IDs=mar_channel_IDs,
                               platform=update_platform)
            # Quick test for other locales, no download
            if len(quick_check_locales) > 0:
                log.info("Generating quick check configs for %s" % fromVersion)
                uvc.addRelease(release=appVersion, build_id=build_id,
                               locales=quick_check_locales,
                               platform=update_platform)

    f = open(options.output, 'w')
    uvc.write(f)
