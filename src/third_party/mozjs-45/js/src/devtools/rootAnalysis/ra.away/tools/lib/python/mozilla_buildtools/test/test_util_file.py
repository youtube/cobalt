import unittest
import tempfile
import os
import hashlib
import shutil
from util.file import compare, sha1sum, copyfile


class TestFileOps(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def testCompareEqFiles(self):
        tmpdir = tempfile.mkdtemp()
        file1 = os.path.join(tmpdir, "foo")
        file2 = os.path.join(tmpdir, "bar")
        open(file1, "w").write("hello")
        open(file2, "w").write("hello")
        self.failUnless(compare(file1, file2))

    def testCompareDiffFiles(self):
        tmpdir = tempfile.mkdtemp()
        file1 = os.path.join(tmpdir, "foo")
        file2 = os.path.join(tmpdir, "bar")
        open(file1, "w").write("hello")
        open(file2, "w").write("goodbye")
        self.failUnless(not compare(file1, file2))

    def testSha1sum(self):
        h = hashlib.new('sha1')
        h.update(open(__file__, 'rb').read())
        self.assertEquals(sha1sum(__file__), h.hexdigest())

    def testCopyFile(self):
        tmp = os.path.join(self.tmpdir, "t")
        copyfile(__file__, tmp)
        self.assertEquals(sha1sum(__file__), sha1sum(tmp))
        self.assertEquals(os.stat(__file__).st_mode, os.stat(tmp).st_mode)
        self.assertEquals(
            int(os.stat(__file__).st_mtime), int(os.stat(tmp).st_mtime))
