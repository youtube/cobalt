import unittest

from release.versions import getL10nDashboardVersion, getAppVersion


class TestBuildVersions(unittest.TestCase):
    def _doTest(self, expected, version):
        self.assertEquals(expected,
                          getL10nDashboardVersion(version, "firefox"))

    def testPointRelease(self):
        self._doTest("fx4.0.1", "4.0.1")

    def testBeta(self):
        self._doTest("fx5_beta_b3", "5.0b3")


class TestGetAppVersion(unittest.TestCase):
    def testFinal(self):
        self.assertEquals('17.0', getAppVersion('17.0'))

    def testPoint(self):
        self.assertEquals('18.0.3', getAppVersion('18.0.3'))

    def testBeta(self):
        self.assertEquals('17.0', getAppVersion('17.0b2'))

    def testEsr(self):
        self.assertEquals('10.0.9', getAppVersion('10.0.9esr'))
