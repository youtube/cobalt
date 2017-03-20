import unittest

from release.info import getBaseTag, getReleaseConfigName


class TestGetBaseTag(unittest.TestCase):
    def testRelease(self):
        self.assertEquals('FIREFOX_16_0_2', getBaseTag('firefox', '16.0.2'))

    def testBeta(self):
        self.assertEquals('FIREFOX_17_0b3', getBaseTag('firefox', '17.0b3'))

    def testEsr(self):
        self.assertEquals(
            'FIREFOX_10_0_9esr', getBaseTag('firefox', '10.0.9esr'))

    def testFennec(self):
        self.assertEquals('FENNEC_17_0', getBaseTag('fennec', '17.0'))

    def testThunderbird(self):
        self.assertEquals(
            'THUNDERBIRD_18_0b1', getBaseTag('thunderbird', '18.0b1'))


class TestGetReleaseConfigName(unittest.TestCase):
    def testFennecBeta(self):
        got = getReleaseConfigName('fennec', 'mozilla-beta', '35.0b1')
        self.assertEquals('release-fennec-mozilla-beta.py', got)

    def testFennecBetaHackery(self):
        got = getReleaseConfigName('fennec', 'mozilla-release', '35.0b1')
        self.assertEquals('release-fennec-mozilla-beta.py', got)

    def testFennecRelease(self):
        got = getReleaseConfigName('fennec', 'mozilla-release', '35.0')
        self.assertEquals('release-fennec-mozilla-release.py', got)

    def testFirefoxBeta(self):
        got = getReleaseConfigName('firefox', 'mozilla-beta', '38.0b7')
        self.assertEquals('release-firefox-mozilla-beta.py', got)

    def testFirefoxBetaHackery(self):
        got = getReleaseConfigName('firefox', 'mozilla-release', '38.0b7')
        self.assertEquals('release-firefox-mozilla-beta.py', got)

    def testFirefoxRelase(self):
        got = getReleaseConfigName('firefox', 'mozilla-release', '38.0')
        self.assertEquals('release-firefox-mozilla-release.py', got)

    def testThunderbird(self):
        got = getReleaseConfigName('thunderbird', 'comm-esr38', '38.0esr')
        self.assertEquals('release-thunderbird-comm-esr38.py', got)

    def testStaging(self):
        got = getReleaseConfigName('fennec', 'mozilla-release', '38.0', staging=True)
        self.assertEquals('staging_release-fennec-mozilla-release.py', got)
