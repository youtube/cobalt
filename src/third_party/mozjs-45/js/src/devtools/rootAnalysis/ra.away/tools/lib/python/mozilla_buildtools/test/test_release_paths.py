import unittest

from release.paths import makeReleasesDir, makeCandidatesDir


class TestReleasesDir(unittest.TestCase):

    def testBaseReleases(self):
        got = makeReleasesDir('bbb')
        self.assertEquals('/pub/mozilla.org/bbb/releases/', got)

    def testVersioned(self):
        got = makeReleasesDir('aa', '15.1')
        self.assertEquals('/pub/mozilla.org/aa/releases/15.1/', got)

    def testRemote(self):
        got = makeReleasesDir('yy', protocol='http', server='foo.bar')
        self.assertEquals('http://foo.bar/pub/mozilla.org/yy/releases/', got)

    def testRemoteAndVersioned(self):
        got = makeReleasesDir('yx', '1.0', protocol='https', server='cee.dee')
        self.assertEquals(
            'https://cee.dee/pub/mozilla.org/yx/releases/1.0/', got)


class TestCandidatesDir(unittest.TestCase):

    def test_base(self):
        expected = "/pub/mozilla.org/bbb/candidates/1.0-candidates/build2/"
        got = makeCandidatesDir('bbb', '1.0', 2)
        self.assertEquals(expected, got)

    def test_fennec(self):
        expected = "/pub/mozilla.org/mobile/candidates/15.1-candidates/build3/"
        got = makeCandidatesDir('fennec', '15.1', 3)
        self.assertEquals(expected, got)

    def test_remote(self):
        expected = "http://foo.bar/pub/mozilla.org/bbb/candidates/1.0-candidates/build5/"
        got = makeCandidatesDir('bbb', '1.0', 5, protocol="http",
                                server='foo.bar')
        self.assertEquals(expected, got)

    def test_ftp_root(self):
        expected = "pub/bbb/candidates/1.0-candidates/build5/"
        got = makeCandidatesDir('bbb', '1.0', 5, ftp_root="pub/")
        self.assertEquals(expected, got)
