#!/usr/bin/python

from optparse import OptionParser
from urllib2 import urlopen
from urlparse import urljoin
from time import sleep

from buildbotcustom.l10n import ParseLocalesFile
from buildbotcustom.common import getSupportedPlatforms

from force_build import Forcer

ALL_PLATFORMS = getSupportedPlatforms()


class L10nForcer:
    ignoredLocales = ('en-US')  # locales that are _always_ to be skipped

    def __init__(self, masterUrl, toForce, releaseTag, name="Unknown",
                 comments="Unknown", delay=2, loud=True):
        """toForce is a dict whose keys are platform names and whose values
           are a tuple of locales to force for that platform
        """
        self.forcers = {}
        assert isinstance(toForce, dict), "toForce must be a dict!"
        for value in toForce.itervalues():
            assert isinstance(value, (tuple, list)), \
                "toForce values must be a list or tuple"
        for platform in toForce.iterkeys():
            forcer = Forcer(masterUrl, '%s_repack' % platform, loud)
            self.forcers[platform] = forcer
        self.toForce = toForce
        self.name = name
        self.comments = comments
        self.delay = delay
        self.loud = loud
        self.releaseTag = releaseTag

    def getProperties(self, locale):
        return {'en_revision': self.releaseTag,
                'l10n_revision': self.releaseTag,
                'locale': locale}

    def forceBuilds(self):
        errors = []
        for platform, locales in self.toForce.iteritems():
            forcer = self.forcers[platform]
            for locale in sorted(locales):
                if locale in self.ignoredLocales:
                    continue
                properties = self.getProperties(locale)
                try:
                    # HACK HACK HACK HACK ALERT
                    # The Force Build form has on way to prevent builds from
                    # getting merged together. By putting each of them on a
                    # different branch, we can prevent this. It doesn't affect
                    # the builds in any way, but it could be confusing.
                    # Once we have a Scheduler we can use 'sendchange' with
                    # we should be able to stop doing this.
                    forcer.forceBuild(name=self.name, comments=self.comments,
                                      branch=locale, properties=properties)
                    sleep(self.delay)
                except:
                    errors.append((platform, locale))
                    if self.loud:
                        "*** Error when forcing %s %s" % (platform, locale)
                    # Don't raise the exception, keep going
        if self.loud:
            if len(errors) > 0:
                print "ERROR SUMMARY:"
            for platform, locale in map(lambda x: (x[0], x[1]), errors):
                print "  %s - %s" % (platform, locale)


def getShippedLocales(shippedLocales):
    parsed = ParseLocalesFile(shippedLocales)
    # Now we have to reverse it, because we want it a different way
    properlyParsed = {}
    for p in ALL_PLATFORMS:
        properlyParsed[p] = []
    for locale, platforms in parsed.iteritems():
        if platforms:
            for p in platforms:
                # map the bouncer/shipped-locales platforms to automation ones
                if p == 'osx':
                    p = 'macosx'
                # skip platforms we don't know about
                if p not in properlyParsed.keys():
                    continue
                properlyParsed[p].append(locale)
        else:
            for p in properlyParsed.iterkeys():
                properlyParsed[p].append(locale)
    return properlyParsed


def filterPlatforms(shippedLocales, includedPlatforms):
    # includedPlatforms is a list of platforms that we want to do builds for,
    # therefore, we remove any platform NOT in that list from shippedLocales
    for platform in shippedLocales.keys():
        if platform not in includedPlatforms:
            del shippedLocales[platform]
    return shippedLocales


if __name__ == '__main__':
    usage = """usage: %prog [options]
You must have buildbotcustom and buildbot in your PYTHONPATH to use this script.

Examples:
To retrigger all l10n builds for all platforms for Firefox 3.5.4:
%prog -m http://production-master02.build.mozilla.org:8010 -t FIREFOX_3_5_4_RELEASE -v -b releases/mozilla-1.9.1 -n [your name]

To retrigger all l10n builds for linux for Firefox 3.5.4:
%prog -m http://production-master02.build.mozilla.org:8010 -t FIREFOX_3_5_4_RELEASE -v -b releases/mozilla-1.9.1 -p linux -n [your name]

To retrigger all l10n builds for linux and macosx for Firefox 3.5.4:
%prog -m http://production-master02.build.mozilla.org:8010 -t FIREFOX_3_5_4_RELEASE -v -b releases/mozilla-1.9.1 -p linux -p macosx -n [your name]

To retrigger 'de' builds for every platform for Firefox 3.6b1:
echo "de" > de-only
%prog -m http://production-master.build.mozilla.org:8010 -t FIREFOX_3_6b1_RELEASE -v -s de-only -n [your name]

To retrigger 'af' on linux and 'zh-TW' on win32 and mac for Firefox 3.6b1:
echo "af linux" > my-locales
echo "zh-TW win32 osx" >> my-locales
%prog -m http://production-master.build.mozilla.org:8010 -t FIREFOX_3_6b1_RELEASE -v -s my-locales -n [your name]

"""
    parser = OptionParser(usage=usage)
    parser.add_option("-n", "--name", action="store", dest="name",
                      default="Unknown",
                      help="Name to use when submitting.")
    parser.add_option("-m", "--master", action="store", dest="master",
                      help="The base url of the master to submit to. " +
                           "Eg, http://localhost:8010")
    parser.add_option("-t", "--release-tag", action="store", dest="releaseTag",
                      help="The tag to build with. Eg, FIREFOX_3_5_3_RELEASE")
    parser.add_option("-c", "--comments", action="store", dest="comments",
                      default="Unknown")
    parser.add_option("-d", "--delay", action="store", dest="delay",
                      default=2,
                      help="Amount of time (in seconds) to wait between each" +
                           "POST. Defaults to 2 seconds")
    parser.add_option("-v", "--verbose", action="store_true", dest="loud",
                      default=False)
    parser.add_option("-g", "--hg", action="store", dest="hg",
                      default="https://hg.mozilla.org",
                      help="Root of the HG server. Defaults to " +
                           "https://hg.mozilla.org. Only used when -s " +
                           "isn't specified.")
    parser.add_option("-b", "--branch", action="store", dest="branch",
                      help="The branch, relative to the HG server, to " +
                           "locate shipped locales on. " +
                           "Eg, releases/mozilla-1.9.1. Only used when " +
                           "-s isn't specified.")
    parser.add_option("-s", "--shipped-locales-file", action="store",
                      dest="shippedLocales", default=None,
                      help="When specified, this script will read in the "
                           "contents of the given file and force the "
                           "appropriate builds based on the contents. It is "
                           "assumed that the file is in shipped-locales format."
                           " If en-US is listed as a locale it will be ignored."
                           " If this option is absent shipped-locales will be "
                           "retrieved based on the -t, -g, and -b arguments "
                           "given. This will cause every locale for every "
                           "platform to be rebuilt. Use with caution.")
    parser.add_option("-p", "--platform", action="append",
                      dest="platforms", default=None,
                      help="When specified, only platforms passed with -p will "
                           "have builds forced. By default, builds will be "
                           "forced for all platforms %s for " %
                      str(ALL_PLATFORMS) +
                      "locales without exceptions. Platform exceptions "
                      "will be obeyed regardless of -p options used. See "
                      "example usage for further details.")

    (options, args) = parser.parse_args()

    shippedLocales = None
    if options.shippedLocales:
        f = open(options.shippedLocales)
        shippedLocales = f.read()
        f.close()
    else:
        file = '%s/raw-file/%s/browser/locales/shipped-locales' % \
            (options.branch, options.releaseTag)
        url = urljoin(options.hg, file)
        shippedLocales = urlopen(url).read()

    platforms = options.platforms or ALL_PLATFORMS
    for p in platforms:
        assert p in ALL_PLATFORMS

    toForce = filterPlatforms(getShippedLocales(shippedLocales), platforms)
    forcer = L10nForcer(masterUrl=options.master, toForce=toForce,
                        releaseTag=options.releaseTag, name=options.name,
                        comments=options.comments, delay=options.delay,
                        loud=options.loud)
    forcer.forceBuilds()
