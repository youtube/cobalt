import tempfile
import shutil
import os
from unittest import TestCase
from util.paths import findfiles, finddirs, convertPath


class TestPaths(TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def testFindFiles(self):
        tmpdir = self.tmpdir
        os.makedirs("%s/d1" % tmpdir)
        open("%s/foo" % tmpdir, 'w').write("hello")
        open("%s/d1/bar" % tmpdir, 'w').write("world")

        self.assertEquals(findfiles(tmpdir),
                          ["%s/foo" % tmpdir,
                           "%s/d1/bar" % tmpdir, ])

        self.assertEquals(finddirs(tmpdir), ["%s/d1" % tmpdir])

    def testConvertPath(self):
        tests = [
            ('unsigned-build1/unsigned/update/win32/foo/bar',
                'signed-build1/update/win32/foo/bar'),
            ('unsigned-build1/unsigned/win32/foo/bar',
                'signed-build1/win32/foo/bar'),
            ('unsigned-build1/win32/foo/bar',
                'signed-build1/win32/foo/bar'),
        ]
        for a, b in tests:
            self.assertEqual(convertPath(a, 'signed-build1'), b)
