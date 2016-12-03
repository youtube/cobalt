from urllib2 import urlopen
from urlparse import urljoin
try:
    import simplejson as json
except ImportError:
    import json

from build.l10n import getLocalesForChunk
from release.platforms import buildbot2ftp, getPlatformLocales, \
    getPlatformLocalesFromJson
from release.versions import getPrettyVersion

import logging
log = logging.getLogger(__name__)


def getShippedLocales(product, appName, version, buildNumber, sourceRepo,
                      hg='https://hg.mozilla.org', revision=None):
    if revision is not None:
        tag = revision
    else:
        tag = '%s_%s_BUILD%s' % (product.upper(), version.replace('.', '_'),
                                 str(buildNumber))
    file = '%s/raw-file/%s/%s/locales/shipped-locales' % \
        (sourceRepo, tag, appName)
    url = urljoin(hg, file)
    try:
        sl = urlopen(url).read()
    except:
        log.error("Failed to retrieve %s", url)
        raise
    return sl


def getCommonLocales(a, b):
    return [locale for locale in a if locale in b]


def parsePlainL10nChangesets(changesets):
    ret = {}
    for line in changesets.splitlines():
        locale, revision = line.rstrip().split()
        ret[locale] = revision
    return ret


def getL10nRepositories(changesets, l10nRepoPath, relbranch=None):
    """Parses a list of locale names and revisions for their associated
       repository from the 'changesets' string passed in."""
    # urljoin() will strip the last part of l10nRepoPath it doesn't end with
    # "/"
    if not l10nRepoPath.endswith('/'):
        l10nRepoPath = l10nRepoPath + '/'
    repositories = {}
    try:
        for locale, data in json.loads(changesets).iteritems():
            locale = urljoin(l10nRepoPath, locale)
            repositories[locale] = {
                'revision': data['revision'],
                'relbranchOverride': relbranch,
                'bumpFiles': []
            }
    except (TypeError, ValueError):
        for locale, revision in parsePlainL10nChangesets(changesets).iteritems():
            if revision == 'FIXME':
                raise Exception('Found FIXME in changesets for locale "%s"' % locale)
            locale = urljoin(l10nRepoPath, locale)
            repositories[locale] = {
                'revision': revision,
                'relbranchOverride': relbranch,
                'bumpFiles': []
            }

    return repositories


def makeReleaseRepackUrls(productName, brandName, version, platform,
                          locale='en-US', signed=False,
                          exclude_secondary=False):
    longVersion = version
    builds = {}
    platformDir = buildbot2ftp(platform)
    if productName not in ('fennec',):
        if platform.startswith('linux'):
            filename = '%s.tar.bz2' % productName
            builds[filename] = '/'.join([p.strip('/') for p in [
                platformDir, locale, '%s-%s.tar.bz2' % (productName, version)]])
        elif platform.startswith('macosx'):
            filename = '%s.dmg' % productName
            builds[filename] = '/'.join([p.strip('/') for p in [
                platformDir, locale, '%s %s.dmg' % (brandName, longVersion)]])
        elif platform.startswith('win'):
            filename = '%s.zip' % productName
            instname = '%s.exe' % productName
            prefix = []
            if not signed:
                prefix.append('unsigned')
            prefix.extend([platformDir, locale])
            if not exclude_secondary:
                builds[filename] = '/'.join(
                    [p.strip('/') for p in
                     prefix + ['%s-%s.zip' % (productName, version)]]
                )
            builds[instname] = '/'.join(
                [p.strip('/') for p in
                 prefix + ['%s Setup %s.exe' % (brandName, longVersion)]]
            )
        else:
            raise "Unsupported platform"
    else:
        if platform.startswith('android'):
            filename = '%s-%s.%s.android-arm.apk' % (
                productName, version, locale)
            prefix = []
            if not signed:
                prefix.append('unsigned')
            prefix.extend([platformDir, locale])
            builds[filename] = '/'.join(
                [p.strip('/') for p in
                 prefix + [filename]]
            )
        elif platform == 'linux':
            filename = '%s.tar.bz2' % productName
            builds[filename] = '/'.join([p.strip('/') for p in [
                platform, locale, '%s-%s.%s.linux-i686.tar.bz2' % (productName, version, locale)]])
        elif platform == 'macosx':
            filename = '%s.dmg' % productName
            builds[filename] = '/'.join([p.strip('/') for p in [
                platform, locale, '%s-%s.%s.mac.dmg' % (brandName, version, locale)]])
        elif platform == 'win32':
            filename = '%s.zip' % productName
            builds[filename] = '/'.join([p.strip('/') for p in [
                platform, locale,
                '%s-%s.%s.win32.zip' % (productName, version, locale)]])
        else:
            raise "Unsupported platform"

    return builds


def getReleaseLocalesForChunk(productName, appName, version, buildNumber,
                              sourceRepo, platform, chunks, thisChunk,
                              hg='https://hg.mozilla.org'):
    possibleLocales = getPlatformLocales(
        getShippedLocales(productName, appName, version, buildNumber,
                          sourceRepo, hg),
        (platform,)
    )[platform]
    return getLocalesForChunk(possibleLocales, chunks, thisChunk)


def getReleaseLocalesFromJsonForChunk(stage_platform, chunks, thisChunk, jsonFile):
    possibleLocales = getPlatformLocalesFromJson(
        jsonFile, (stage_platform,))[stage_platform]
    return getLocalesForChunk(possibleLocales, chunks, thisChunk)
