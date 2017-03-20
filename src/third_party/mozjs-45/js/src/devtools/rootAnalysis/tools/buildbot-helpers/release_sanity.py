#!/usr/bin/env python
"""%prog [-d|--dryrun] [-u|--username `username`] [-b|--bypass-check]
        [-l| --bypass-l10n-check] [-m|--bypass-mozconfig-check]
        [-V| --version `version`] [-B --branch `branchname`]
        [-N|--build-number `buildnumber`]
        [-c| --release-config `releaseConfigFile`]
        [-w| --whitelist `mozconfig_whitelist`]
        [--l10n-dashboard-version version]
        master:port

    Wrapper script to sanity-check a release. Default behaviour is to check
    the branch and revision specific in the release_configs, check if the
    milestone and version# in the source repo match the
    expected values in the release_configs, check the l10n repos & dashboard,
    compare the nightly and release mozconfigs for a release branch against
    a whitelist of known differences between the two. If all tests pass then
    the master is reconfiged and then a senchange is generated to kick off
    the release automation.
"""
try:
    import simplejson as json
except ImportError:
    import json

import logging
import site
import urllib2

from optparse import OptionParser
from os import path
from tempfile import mkdtemp
from shutil import rmtree

site.addsitedir(path.join(path.dirname(__file__), "../lib/python"))

from util.file import compare
from util.hg import make_hg_url, mercurial, update
from release.info import readReleaseConfig, getRepoMatchingBranch
from release.versions import getL10nDashboardVersion
from release.l10n import getShippedLocales
from release.platforms import getLocaleListFromShippedLocales
from release.sanity import check_buildbot, locale_diff, \
    sendchange, verify_mozconfigs
from release.partials import Partial
from util.retry import retry

log = logging.getLogger(__name__)
error_tally = set()
HG = 'hg.mozilla.org'


def verify_repo(branch, revision, hghost):
    """Poll the hgweb interface for a given branch and revision to
       make sure it exists"""
    repo_url = make_hg_url(hghost, branch, revision=revision)
    log.info("Checking for existence of %s..." % repo_url)
    success = True
    try:
        repo_page = urllib2.urlopen(repo_url)
        log.info("Got: %s !" % repo_page.geturl())
    except urllib2.HTTPError:
        log.error("Repo does not exist with required revision."
                  " Check again, or use -b to bypass")
        success = False
        error_tally.add('verify_repo')
    return success


def verify_configs(configs_dir, revision, hghost, configs_repo, changesets,
                   filename):
    """Check the release_configs and l10n-changesets against tagged
    revisions"""

    release_config_file = path.join(configs_dir, 'mozilla', filename)
    l10n_changesets_file = path.join(configs_dir, 'mozilla', changesets)
    configs_url = make_hg_url(hghost, configs_repo, revision=revision,
                              filename=path.join('mozilla', filename))
    l10n_url = make_hg_url(hghost, configs_repo, revision=revision,
                           filename=path.join('mozilla', changesets))

    success = True
    try:
        official_configs = urllib2.urlopen(configs_url, timeout=10)
        log.info("Comparing tagged revision %s to on-disk %s ..." % (
            configs_url, filename))
        if not compare(official_configs, release_config_file):
            log.error("local configs do not match tagged revisions in repo")
            success = False
            error_tally.add('verify_configs')
        l10n_changesets = urllib2.urlopen(l10n_url, timeout=10)
        log.info("Comparing tagged revision %s to on-disk %s ..." % (
            l10n_url, changesets))
        if not compare(l10n_changesets, l10n_changesets_file):
            log.error("local l10n-changesets do not match tagged revisions"
                      " in repo")
            success = False
            error_tally.add('verify_configs')
    except (urllib2.HTTPError, urllib2.URLError):
        log.error("cannot find configs in repo %s" % configs_url)
        log.error("cannot find configs in repo %s" % l10n_url)
        success = False
        error_tally.add('verify_configs')
    return success


def query_locale_revisions(l10n_changesets):
    locales = {}
    if l10n_changesets.endswith('.json'):
        fh = open(l10n_changesets, 'r')
        locales_json = json.load(fh)
        fh.close()
        for locale in locales_json:
            locales[locale] = locales_json[locale]["revision"]
    else:
        for line in open(l10n_changesets, 'r'):
            locale, revision = line.split()
            locales[locale] = revision
    return locales


def get_l10n_changesets(locale_url):
    try:
        urllib2.urlopen(locale_url, timeout=10)
        return True
    except urllib2.HTTPError, e:
        reason = ""
        if hasattr(e, 'reason'):
            # Python 2.6 does not have reason
            reason = e.reason
        log.error("error checking l10n changeset %s: %d %s" % (locale_url, e.code, reason))
        raise e
    except urllib2.URLError:
        log.error("timeout checking l10n changeset %s" % locale_url)
        raise


def verify_l10n_changesets(hgHost, l10n_changesets):
    """Checks for the existance of all l10n changesets"""
    success = True
    locales = query_locale_revisions(l10n_changesets)
    for locale in sorted(locales.keys()):
        revision = locales[locale]
        localePath = '%(repoPath)s/%(locale)s/file/%(revision)s' % {
            'repoPath': releaseConfig['l10nRepoPath'].strip('/'),
            'locale': locale,
            'revision': revision,
        }
        locale_url = make_hg_url(hgHost, localePath, protocol='https')
        log.info("Checking for existence l10n changeset %s %s in repo %s ..."
                 % (locale, revision, locale_url))

        success = retry(get_l10n_changesets,
                        kwargs=dict(locale_url=locale_url), attempts=3,
                        sleeptime=1, retry_exceptions=(urllib2.HTTPError, urllib2.URLError))
        if not success:
            error_tally.add('verify_l10n')
    return success


def verify_l10n_dashboard(l10n_changesets, l10n_dashboard_version=None):
    """Checks the l10n-changesets against the l10n dashboard"""
    success = True
    locales = query_locale_revisions(l10n_changesets)
    if l10n_dashboard_version:
        l10n_dashboard_version = getL10nDashboardVersion(
            l10n_dashboard_version, releaseConfig['productName'],
            parse_version=False)
    else:
        l10n_dashboard_version = getL10nDashboardVersion(
            releaseConfig['version'], releaseConfig['productName'])
    dash_url = 'https://l10n.mozilla.org/shipping/l10n-changesets?ms=%s' % \
        l10n_dashboard_version
    log.info("Comparing l10n changesets on dashboard %s to on-disk %s ..."
             % (dash_url, l10n_changesets))
    try:
        dash_changesets = {}
        for line in urllib2.urlopen(dash_url, timeout=10):
            locale, revision = line.split()
            dash_changesets[locale] = revision
        for locale in locales:
            revision = locales[locale]
            dash_revision = dash_changesets.pop(locale, None)
            if not dash_revision:
                log.error("\tlocale %s missing on dashboard" % locale)
                success = False
                error_tally.add('verify_l10n_dashboard')
            elif revision != dash_revision:
                log.error("\tlocale %s revisions not matching: %s (config)"
                          " vs. %s (dashboard)"
                          % (locale, revision, dash_revision))
                success = False
                error_tally.add('verify_l10n_dashboard')
        for locale in dash_changesets:
            log.error("\tlocale %s missing in config" % locale)
            success = False
            error_tally.add('verify_l10n_dashboard')
    except (urllib2.HTTPError, urllib2.URLError):
        log.error("cannot find l10n dashboard at %s" % dash_url)
        success = False
        error_tally.add('verify_l10n_dashboard')
    return success


def verify_l10n_shipped_locales(l10n_changesets, shipped_locales):
    """Ensure that our l10n-changesets on the master match the repo's shipped
    locales list"""
    success = True
    locales = query_locale_revisions(l10n_changesets)
    log.info("Comparing l10n changesets to shipped locales ...")
    diff_list = locale_diff(locales, shipped_locales)
    if len(diff_list) > 0:
        log.error("l10n_changesets and shipped_locales differ on locales:"
                  " %s" % diff_list)
        success = False
        error_tally.add('verify_l10n_shipped_locales')
    return success


def verify_options(cmd_options, config):
    """Check release_configs against command-line opts"""
    success = True
    if cmd_options.version and cmd_options.version != config['version']:
        log.error("version passed in does not match release_configs")
        success = False
        error_tally.add('verify_options')
    if cmd_options.buildNumber and \
            int(cmd_options.buildNumber) != int(config['buildNumber']):
        log.error("buildNumber passed in does not match release_configs")
        success = False
        error_tally.add('verify_options')
    if not getRepoMatchingBranch(cmd_options.branch,
                                 config['sourceRepositories']):
        log.error("branch passed in does not exist in release config")
        success = False
        error_tally.add('verify_options')
    if not cmd_options.skip_reconfig:
        if not cmd_options.masters_json_file:
            log.error("masters json file is required when not skipping reconfig")
            success = False
            error_tally.add('masters_json_file')
    return success


def verify_partial(platforms, product, version, build_number,
                   HACK_first_released_versions=None, protocol='http',
                   server='ftp.mozilla.org'):

    from distutils.version import LooseVersion
    partial = Partial(product, version, build_number, protocol, server)
    log.info("Checking for existence of %s complete mar file..." % partial)
    complete_mar_name = partial.complete_mar_name()
    for platform in platforms:
        if HACK_first_released_versions and platform in HACK_first_released_versions:
            if LooseVersion(version) < LooseVersion(HACK_first_released_versions[platform]):
                # No partial for this!
                continue
        log.info("Platform: %s" % platform)
        complete_mar_url = partial.complete_mar_url(platform=platform)
        if partial.exists(platform=platform):
            log.info("complete mar: %s exists, url: %s" % (complete_mar_name,
                                                           complete_mar_url))
        else:
            log.error("Requested file, %s, does not exist on %s"
                      " Check again, or use -b to bypass" % (complete_mar_name,
                                                             complete_mar_url))
            error_tally.add('verify_partial')
            return False

    return True


if __name__ == '__main__':
    parser = OptionParser(__doc__)
    parser.set_defaults(
        check=True,
        checkL10n=True,
        checkL10nDashboard=True,
        checkMozconfigs=True,
        dryrun=False,
        username="cltbld",
        loglevel=logging.INFO,
        version=None,
        buildNumber=None,
        branch=None,
        whitelist=path.abspath(path.join(path.dirname(__file__),
                                         "mozconfig_whitelist")),
        skip_reconfig=False,
        configs_repo_url='build/buildbot-configs',
        configs_branch='production',
        concurrency=8,
        skip_verify_configs=False,
        checkMultiLocale=True,
    )
    parser.add_option(
        "-b", "--bypass-check", dest="check", action="store_false",
        help="don't bother verifying release repo's on this master")
    parser.add_option(
        "-l", "--bypass-l10n-check", dest="checkL10n", action="store_false",
        help="don't bother verifying l10n milestones")
    parser.add_option(
        "--bypass-l10n-dashboard-check", dest="checkL10nDashboard",
        action="store_false", help="don't verify l10n changesets against the dashboard (implied when --bypass-l10n-check is passed)")
    parser.add_option(
        "-m", "--bypass-mozconfig-check", dest="checkMozconfigs",
        action="store_false", help="don't verify mozconfigs")
    parser.add_option(
        "-d", "--dryrun", "--dry-run", dest="dryrun", action="store_true",
        help="just do the reconfig/checks, without starting anything")
    parser.add_option(
        "-u", "--username", dest="username",
        help="specify a specific username to attach to the sendchange")
    parser.add_option(
        "-V", "--version", dest="version",
        help="version string for release in format: x.x.x")
    parser.add_option("-N", "--build-number", dest="buildNumber", type="int",
                      help="build number for this release, "
                      "uses release_config otherwise")
    parser.add_option(
        "-B", "--branch", dest="branch",
        help="branch name for this release, uses release_config otherwise")
    parser.add_option(
        "-c", "--release-config", dest="releaseConfigFiles", action="append",
        help="specify the release-config files (the first is primary)")
    parser.add_option("-w", "--whitelist", dest="whitelist",
                      help="whitelist for known mozconfig differences")
    parser.add_option(
        "--l10n-dashboard-version", dest="l10n_dashboard_version",
        help="Override L10N dashboard version")
    parser.add_option("--skip-reconfig", dest="skip_reconfig",
                      action="store_true", help="Do not run reconfig")
    parser.add_option("--configs-dir", dest="configs_dir",
                      help="buildbot-configs directory")
    parser.add_option("--configs-repo-url", dest="configs_repo_url",
                      help="buildbot-configs repo URL")
    parser.add_option("--configs-branch", dest="configs_branch",
                      help="buildbot-configs branch")
    parser.add_option("--masters-json-file", dest="masters_json_file",
                      help="Path to production-masters.json file.")
    parser.add_option('-j', dest='concurrency', type='int',
                      help='Fabric concurrency level')
    parser.add_option("--skip-verify-configs", dest="skip_verify_configs",
                      action="store_true",
                      help="Do not verify configs agains remote repos")
    parser.add_option("--bypass-multilocale-check", dest="checkMultiLocale",
                      action="store_false",
                      help="Do not verify that multilocale is enabled for Fennec")

    options, args = parser.parse_args()
    if not options.dryrun and not args:
        parser.error("Need to provide a master to sendchange to,"
                     " or -d for a dryrun")
    elif not options.branch:
        parser.error("Need to provide a branch to release")
    elif not options.releaseConfigFiles:
        parser.error("Need to provide a release config file")

    logging.basicConfig(level=options.loglevel,
                        format="%(asctime)s : %(levelname)s : %(message)s")

    releaseConfig = None
    test_success = True
    buildNumber = options.buildNumber
    products = []

    check_buildbot()
    if not options.dryrun and not options.skip_reconfig:
        from util.fabric.common import check_fabric, FabricHelper
        check_fabric()

    if options.configs_dir:
        configs_dir = options.configs_dir
        cleanup_configs = False
    else:
        cleanup_configs = True
        configs_dir = mkdtemp()
        remote = make_hg_url(HG, options.configs_repo_url)
        retry(mercurial, args=(remote, configs_dir),
              kwargs={'branch': options.configs_branch})
        update(configs_dir, options.configs_branch)

    # https://bugzilla.mozilla.org/show_bug.cgi?id=678103#c5
    # This goes through the list of config files in reverse order, which is a
    # hacky way of making sure that the config file that's listed first is the
    # one that's loaded in releaseConfig for the sendchange.
    for releaseConfigFile in list(reversed(options.releaseConfigFiles)):
        abs_release_config_file = path.join(configs_dir, 'mozilla',
                                            releaseConfigFile)
        releaseConfig = readReleaseConfig(abs_release_config_file)
        products.append(releaseConfig['productName'])

        if not options.buildNumber:
            log.warn("No buildNumber specified, using buildNumber in"
                     " release_config, which may be out of date!")
            options.buildNumber = releaseConfig['buildNumber']

        if options.check:
            site.addsitedir(path.join(configs_dir, 'mozilla'))
            from config import BRANCHES
            source_repo = 'mozilla'
            try:
                branchConfig = BRANCHES[options.branch]
            except KeyError:
                from thunderbird_config import BRANCHES
                branchConfig = BRANCHES[options.branch]
                source_repo = 'comm'

            # Match command line options to defaults in release_configs
            if not verify_options(options, releaseConfig):
                test_success = False
                log.error("Error verifying command-line options,"
                          " attempting checking repo")

            # verify that mozconfigs for this release pass diff with nightly,
            # compared to a whitelist
            try:
                repo_path = \
                    releaseConfig['sourceRepositories'][source_repo]['path']
                revision = \
                    releaseConfig[
                        'sourceRepositories'][source_repo]['revision']
            except KeyError:
                try:
                    repo_path = \
                        releaseConfig['sourceRepositories']['mobile']['path']
                    revision = \
                        releaseConfig[
                            'sourceRepositories']['mobile']['revision']
                except KeyError:
                    log.error("Can't determine sourceRepo for mozconfigs")
            nightly_mozconfigs = {}
            for p in releaseConfig['mozconfigs']:
                nightly_mozconfigs[p] = branchConfig['platforms'][p]['src_mozconfig']
            if options.checkMozconfigs and \
                    not verify_mozconfigs(
                        repo_path,
                        revision,
                        branchConfig['hghost'],
                        releaseConfig['productName'],
                        releaseConfig['mozconfigs'],
                        nightly_mozconfigs,
                        options.whitelist):
                test_success = False
                error_tally.add('verify_mozconfig')
                log.error("Error verifying mozconfigs")

            # verify that the release_configs on-disk match the tagged
            # revisions in hg
            l10nRevisionFile = path.join(configs_dir, 'mozilla',
                                         releaseConfig['l10nRevisionFile'])
            if not options.skip_verify_configs and \
                    not verify_configs(
                        configs_dir,
                        "%s_BUILD%s" % (releaseConfig['baseTag'], buildNumber),
                        branchConfig['hghost'],
                        options.configs_repo_url,
                        releaseConfig['l10nRevisionFile'],
                        releaseConfigFile):
                test_success = False
                log.error("Error verifying configs")

            if options.checkL10n:
                # verify that l10n changesets exist
                if not verify_l10n_changesets(branchConfig['hghost'],
                                              l10nRevisionFile):
                    test_success = False
                    log.error("Error verifying l10n changesets")

                if options.checkL10nDashboard:
                    # verify that l10n changesets match the dashboard
                    if not verify_l10n_dashboard(
                        l10nRevisionFile,
                            options.l10n_dashboard_version):
                        test_success = False
                        log.error("Error verifying l10n dashboard changesets")

                if options.checkMultiLocale:
                    if releaseConfig.get('enableMultiLocale'):
                        f = open(l10nRevisionFile)
                        if 'multilocale' not in f.read():
                            test_success = False
                            log.error("MultiLocale enabled but not present in l10n changesets")
                        f.close()

                # verify that l10n changesets match the shipped locales
                if releaseConfig.get('shippedLocalesPath'):
                    sr = releaseConfig['sourceRepositories'][source_repo]
                    sourceRepoPath = sr['path']
                    shippedLocales = getLocaleListFromShippedLocales(
                        getShippedLocales(
                            releaseConfig['productName'],
                            releaseConfig['appName'],
                            releaseConfig['version'],
                            releaseConfig['buildNumber'],
                            sourceRepoPath,
                            'https://hg.mozilla.org',
                            sr['revision'],
                        ))
                    # l10n_changesets do not have an entry for en-US
                    if 'en-US' in shippedLocales:
                        shippedLocales.remove('en-US')
                    if not verify_l10n_shipped_locales(l10nRevisionFile,
                                                       shippedLocales):
                        test_success = False
                        log.error("Error verifying l10n_changesets matches"
                                  " shipped_locales")

            # verify that the relBranch + revision in the release_configs
            # exists in hg
            for sr in releaseConfig['sourceRepositories'].values():
                sourceRepoPath = sr['path']
                if not verify_repo(sourceRepoPath, sr['revision'],
                                   branchConfig['hghost']):
                    test_success = False
                    log.error("Error verifying repos")

            # check partial updates
            partials = releaseConfig.get('partialUpdates')
            if 'extraUpdates' in releaseConfig:
                partials.extend(releaseConfig['extraUpdated'])
            product = releaseConfig['productName']
            platforms = releaseConfig['enUSPlatforms']
            if partials:
                for partial in partials:
                    build_number = partials[partial]['buildNumber']
                    # when bug 839926 lands, buildNumber must be None for releases
                    # but it might have a value for betas (beta *might* use
                    # unreleased builds see bug 1091694 c2)
                    if not verify_partial(platforms, product, partial,
                                          build_number,
                                          releaseConfig.get("HACK_first_released_version"),
                                          server=releaseConfig['ftpServer']):
                        test_success = False
                        log.error("Error verifying partials")

    if test_success:
        if not options.dryrun:
            if not options.skip_reconfig:
                fabric_helper = FabricHelper(
                    masters_json_file=options.masters_json_file,
                    concurrency=options.concurrency,
                    roles=['build', 'scheduler'])
                fabric_helper.update_and_reconfig()
            sourceRepoPath = getRepoMatchingBranch(
                options.branch, releaseConfig['sourceRepositories'])['path']
            sendchange(
                sourceRepoPath,
                "%s_RELEASE" % releaseConfig['baseTag'],
                options.username,
                args[0],
                products,
            )
        else:
            log.info("Tests Passed! Did not run reconfig/sendchange."
                     " Rerun without `-d`")
            if cleanup_configs:
                log.info("Removing temporary directory: %s" % configs_dir)
                rmtree(configs_dir)
    else:
        log.fatal("Tests Failed! Not running sendchange!")
        log.fatal("Failed tests (run with -b to skip) :")
        for error in error_tally:
            log.fatal(error)
        if cleanup_configs:
            log.info("Not removing temporary directory: %s" % configs_dir)
        exit(1)
