"""
Some automated tests for signing code
"""
import subprocess
from unittest import TestCase

import tempfile
import shutil
import os

from release.info import fileInfo

from signing.utils import shouldSign, filterFiles, sortFiles, sums_are_equal


class TestFileParsing(TestCase):
    product = 'firefox'
    firstLocale = 'en-US'

    def _doFileInfoTest(self, path, product, info):
        self.assertEqual(fileInfo(path, product), info)

    def testShortPathenUSMar(self):
        self._doFileInfoTest(
            'foo/bar/firefox-3.0.12.en-US.win32.complete.mar',
            'firefox',
            dict(product='firefox', version='3.0.12', locale='en-US',
                 platform='win32', contents='complete', format='mar',
                 pathstyle='short', leading_path='', previousVersion=None))

    def testShortPathLocaleMar(self):
        self._doFileInfoTest(
            'foo/bar/firefox-3.0.12.es.win32.complete.mar',
            'firefox',
            dict(product='firefox', version='3.0.12', locale='es',
                 platform='win32', contents='complete', format='mar',
                 pathstyle='short', leading_path='', previousVersion=None))

    def testShortPathLocaleExe(self):
        self._doFileInfoTest(
            'foo/bar/firefox-3.0.12.es.win32.installer.exe',
            'firefox',
            dict(product='firefox', version='3.0.12', locale='es',
                 platform='win32', contents='installer', format='exe',
                 pathstyle='short', leading_path=''))

    def testLongPathenUSMarRC(self):
        self._doFileInfoTest(
            'unsigned/update/win32/en-US/firefox-3.5rc3.complete.mar',
            'firefox',
            dict(product='firefox', version='3.5rc3', locale='en-US',
                 platform='win32', contents='complete', format='mar',
                 pathstyle='long', leading_path='', previousVersion=None))

    def testLongPathenUSMarFinal(self):
        self._doFileInfoTest(
            'unsigned/update/win32/en-US/firefox-3.5.complete.mar',
            'firefox',
            dict(product='firefox', version='3.5', locale='en-US',
                 platform='win32', contents='complete', format='mar',
                 pathstyle='long', leading_path='', previousVersion=None))

    def testLongPathenUSMarPointRelease(self):
        self._doFileInfoTest(
            'unsigned/update/win32/en-US/firefox-3.5.1.complete.mar',
            'firefox',
            dict(product='firefox', version='3.5.1', locale='en-US',
                 platform='win32', contents='complete', format='mar',
                 pathstyle='long', leading_path='', previousVersion=None))

    def testLongPathenUSMarPointRelease2(self):
        self._doFileInfoTest(
            'unsigned/update/win32/en-US/firefox-3.5.12.complete.mar',
            'firefox',
            dict(product='firefox', version='3.5.12', locale='en-US',
                 platform='win32', contents='complete', format='mar',
                 pathstyle='long', leading_path='', previousVersion=None))

    def testLongPathenUSMarProjectBranch(self):
        self._doFileInfoTest(
            'unsigned/update/win32/en-US/firefox-3.6.3plugin2.complete.mar',
            'firefox',
            dict(
                product='firefox', version='3.6.3plugin2', locale='en-US',
                platform='win32', contents='complete', format='mar',
                pathstyle='long', leading_path='', previousVersion=None))

    def testLongPathLocaleMarPointRelease(self):
        self._doFileInfoTest(
            'unsigned/update/win32/fr/firefox-3.5.1.complete.mar',
            'firefox',
            dict(product='firefox', version='3.5.1', locale='fr',
                 platform='win32', contents='complete', format='mar',
                 pathstyle='long', leading_path='', previousVersion=None))

    def testLongPathLocaleExePointRelease(self):
        self._doFileInfoTest(
            'unsigned/win32/fr/Firefox Setup 3.5.1.exe',
            'firefox',
            dict(product='firefox', version='3.5.1', locale='fr',
                 platform='win32', contents='installer', format='exe',
                 pathstyle='long', leading_path=''))

    def testLongPathenUSExePointRelease(self):
        self._doFileInfoTest(
            'unsigned/win32/en-US/Firefox Setup 3.5.12.exe',
            'firefox',
            dict(product='firefox', version='3.5.12', locale='en-US',
                 platform='win32', contents='installer', format='exe',
                 pathstyle='long', leading_path=''))

    def testLongPathenUSExeFinal(self):
        self._doFileInfoTest(
            'unsigned/win32/en-US/Firefox Setup 3.5.exe',
            'firefox',
            dict(product='firefox', version='3.5', locale='en-US',
                 platform='win32', contents='installer', format='exe',
                 pathstyle='long', leading_path=''))

    def testLongPathenUSExeRC(self):
        self._doFileInfoTest(
            'unsigned/win32/en-US/Firefox Setup 3.5 RC 3.exe',
            'firefox',
            dict(product='firefox', version='3.5 RC 3', locale='en-US',
                 platform='win32', contents='installer', format='exe',
                 pathstyle='long', leading_path=''))

    def testLongPathenUSExeBeta(self):
        self._doFileInfoTest(
            'unsigned/win32/en-US/Firefox Setup 3.5 Beta 99.exe',
            'firefox',
            dict(product='firefox', version='3.5 Beta 99', locale='en-US',
                 platform='win32', contents='installer', format='exe',
                 pathstyle='long', leading_path=''))

    def testLongPathenUSExeProjectBranch(self):
        self._doFileInfoTest(
            'unsigned/win32/en-US/Firefox Setup 3.6.3plugin2.exe',
            'firefox',
            dict(
                product='firefox', version='3.6.3plugin2', locale='en-US',
                platform='win32', contents='installer', format='exe',
                pathstyle='long', leading_path=''))

    def testLongPathPartnerRepack(self):
        self._doFileInfoTest(
            'unsigned/partner-repacks/chinapack-win32/win32/zh-CN/Firefox Setup 3.6.14.exe',
            'firefox',
            dict(product='firefox', version='3.6.14', locale='zh-CN',
                 platform='win32', contents='installer', format='exe',
                 pathstyle='long', leading_path='partner-repacks/chinapack-win32/'))

    def testShouldSign(self):
        self.assert_(shouldSign('setup.exe'))
        self.assert_(shouldSign('freebl3.dll'))
        self.assert_(not shouldSign('application.ini'))
        self.assert_(not shouldSign('D3DCompiler_42.dll'))

    def testFiltering(self):
        """ Test that only files of the expected platform and type are passed
        through  to be signed """
        files = [
            'unsigned/linux-i686/en-US/Firefox Setup 3.6.13.exe',
            'unsigned/linux-i686/en-US/firefox-3.6.13.tar.bz2',
            'unsigned/win32/en-US/Firefox Setup 3.6.13.exe',
            'unsigned/win32/en-US/firefox-3.6.13.zip',
            'unsigned/partner-repacks/chinapack-win32/win32/en-US/Firefox Setup 3.6.13.exe',
            'unsigned/partner-repacks/google/win32/ar/Firefox Setup 3.6.13.exe',
        ]
        expected = [
            'unsigned/win32/en-US/Firefox Setup 3.6.13.exe',
            'unsigned/partner-repacks/chinapack-win32/win32/en-US/Firefox Setup 3.6.13.exe',
            'unsigned/partner-repacks/google/win32/ar/Firefox Setup 3.6.13.exe',
        ]
        self.assertEquals(filterFiles(files, self.product), expected)

    def testSorting(self):
        """ Test that the expected sort order is maintained with plain
           exes, followed by partner exes, followed by MARs"""
        files = [
            'unsigned/partner-repacks/chinapack-win32/win32/en-US/Firefox Setup 3.6.13.exe',
            'unsigned/update/win32/en-US/firefox-3.6.13.complete.mar',
            'unsigned/win32/en-US/Firefox Setup 3.6.13.exe',
            'unsigned/partner-repacks/google/win32/ar/Firefox Setup 3.6.13.exe',
            'unsigned/update/win32/ar/firefox-3.6.13.complete.mar',
            'unsigned/win32/ar/Firefox Setup 3.6.13.exe',
        ]
        sorted_files = [
            'unsigned/win32/en-US/Firefox Setup 3.6.13.exe',
            'unsigned/update/win32/en-US/firefox-3.6.13.complete.mar',
            'unsigned/win32/ar/Firefox Setup 3.6.13.exe',
            'unsigned/update/win32/ar/firefox-3.6.13.complete.mar',
            'unsigned/partner-repacks/chinapack-win32/win32/en-US/Firefox Setup 3.6.13.exe',
            'unsigned/partner-repacks/google/win32/ar/Firefox Setup 3.6.13.exe',
        ]

        results = sortFiles(files, self.product, self.firstLocale)
        self.assertEquals(results, sorted_files)

    def testChecksumVerify(self):
        """ Test that the checksum verification code is correct """
        test_invalid_checksums = [
            {'firefox.exe': '1', 'libnss3.dll': '2'},
            {'firefox.exe': '3', 'libnss3.dll': '2'},
            {'firefox.exe': '1', 'libnss3.dll': '2'},
        ]
        test_valid_checksums = [
            {'firefox.exe': '1', 'libnss3.dll': '2'},
            {'firefox.exe': '1', 'libnss3.dll': '2'},
            {'firefox.exe': '1', 'libnss3.dll': '2'},
        ]
        self.failUnless(not sums_are_equal(
            test_invalid_checksums[0], test_invalid_checksums))
        self.failUnless(
            sums_are_equal(test_valid_checksums[0], test_valid_checksums))

    def testWin32DownloadRules(self):
        """ Test that the rsync logic works as expected:
              - No hidden files/dirs
              - No previously signed binaries
              - No partial mars
              - No checksums
              - No detached sigs
        """
        try:
            tmpdir = tempfile.mkdtemp()
            os.makedirs("%s/test-download/update/win32" % tmpdir)
            os.makedirs("%s/test-download/win32" % tmpdir)
            os.makedirs("%s/test-download/unsigned/win32/.foo" % tmpdir)
            os.makedirs("%s/test-download/unsigned/win32/foo" % tmpdir)
            open("%s/test-download/unsigned/win32/.foo/hello.complete.mar" %
                 tmpdir, "w").write("Hello\n")
            open("%s/test-download/unsigned/win32/foo/hello.complete.mar" %
                 tmpdir, "w").write("Hello\n")
            open("%s/test-download/win32/hello.complete.mar" %
                 tmpdir, "w").write("Hello\n")
            open("%s/test-download/unsigned/win32/hello.partial.mar" %
                 tmpdir, "w").write("Hello\n")
            open("%s/test-download/unsigned/win32/hello.checksums" %
                 tmpdir, "w").write("Hello\n")
            open("%s/test-download/unsigned/win32/hello.asc" %
                 tmpdir, "w").write("Hello\n")
            download_exclude = os.path.join(
                os.path.dirname(os.path.abspath(__file__)),
                "../../../../release/signing/download-exclude.list",
            )
            subprocess.check_call([
                'rsync',
                '-av',
                '--exclude-from',
                download_exclude,
                '%s/test-download/' % tmpdir,
                '%s/test-dest/' % tmpdir])
            self.failUnless(not os.path.isdir("%s/test-dest/win32" % tmpdir))
            self.failUnless(
                not os.path.isdir("%s/test-dest/update/win32" % tmpdir))
            self.failUnless(not os.path.isdir(
                "%s/test-dest/unsigned/win32/.foo/" % tmpdir))
            self.failUnless(not os.path.exists("%s/test-dest/unsigned/win32/.foo/hello.complete.mar" % tmpdir))
            self.failUnless(not os.path.exists(
                "%s/test-dest/unsigned/win32/hello.partial.mar" % tmpdir))
            self.failUnless(not os.path.exists(
                "%s/test-dest/unsigned/win32/hello.checksums" % tmpdir))
            self.failUnless(not os.path.exists(
                "%s/test-dest/unsigned/win32/hello.asc" % tmpdir))
            self.failUnless(os.path.exists("%s/test-dest/unsigned/win32/foo/hello.complete.mar" % tmpdir))
        finally:
            # clean up
            shutil.rmtree(tmpdir)


if __name__ == '__main__':
    from unittest import main
    main()
