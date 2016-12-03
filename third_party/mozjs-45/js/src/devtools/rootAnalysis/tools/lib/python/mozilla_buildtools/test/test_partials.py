import unittest
from release.partials import Partial
from release.platforms import buildbot2ftp

PRODUCTS = ('firefox', 'thunderbird')
PLATFORMS = ('linux', 'linux64', 'win32', 'macosx64')
VERSION = '99.0b1'
PROTOCOL = 'http'
SERVER = 'ftp.mozilla.org'


class TestPartial(unittest.TestCase):

    def testPartialShortName(self):
        for product in PRODUCTS:
            buildNumber = 1
            p = Partial(product, VERSION, buildNumber, PROTOCOL, SERVER)
            self.assertEqual(p.short_name(), '%s %s build %s' % (product, VERSION, buildNumber))
            buildNumber = None
            p = Partial(product, VERSION, buildNumber, PROTOCOL, SERVER)
            self.assertEqual(p.short_name(), '%s %s' % (product, VERSION))

    def testPartialIsFromCandidates_dir(self):
        for product in PRODUCTS:
            buildNumber = 1
            p = Partial(product, VERSION, buildNumber, PROTOCOL, SERVER)
            self.assertTrue(p._is_from_candidates_dir())
            buildNumber = None
            p = Partial(product, VERSION, buildNumber, PROTOCOL, SERVER)
            self.assertFalse(p._is_from_candidates_dir())

    def testPartialCompleteMarName(self):
        for product in PRODUCTS:
            buildNumber = 1
            p = Partial(product, VERSION, buildNumber, PROTOCOL, SERVER)
            complete_mar_ = '%s-%s.complete.mar' % (product, VERSION)
            self.assertEqual(complete_mar_, p.complete_mar_name())

    def testPartialCompleteMarUrl(self):
        for product in PRODUCTS:
            for platform in PLATFORMS:
                buildNumber = 1
                p = Partial(product, VERSION, buildNumber, PROTOCOL, SERVER)
                complete_mar_name = p.complete_mar_name()
                url = '%s://%s/pub/mozilla.org/%s/candidates/%s-candidates/build%s/update/%s/en-US/%s' % (
                    PROTOCOL, SERVER, product, VERSION, buildNumber,
                    buildbot2ftp(platform), complete_mar_name)

                self.assertEqual(p.complete_mar_url(platform), url)
                buildNumber = None
                p = Partial(product, VERSION, buildNumber, PROTOCOL, SERVER)
                url = '%s://%s/pub/mozilla.org/%s/releases/%s/update/%s/en-US/%s' % (
                    PROTOCOL, SERVER, product, VERSION,
                    buildbot2ftp(platform), complete_mar_name)
                self.assertEqual(p.complete_mar_url(platform), url)


if __name__ == '__main__':
    unittest.main()
