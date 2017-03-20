import unittest
from build.paths import getRealpath, get_repo_dirname
import os
from os import path
from tempfile import mkdtemp
import shutil


class TestgetRealpath(unittest.TestCase):

    def setUp(self):
        self.tempdir = mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tempdir)

    def _createRegularFile(self, fileName):
        full_path = path.join(self.tempdir, fileName)
        dirname = path.dirname(full_path)
        os.makedirs(dirname)
        f = open(full_path, 'w')
        f.close()

    def testRegularFile(self):
        fname = 'dir/file1.txt'
        self._createRegularFile(fname)
        self.assertEqual(fname, getRealpath(fname))

    def testAbsSymlink(self):
        fname = 'dir/file2.txt'
        symlink = 'localconfig.py'
        self._createRegularFile(fname)
        os.symlink(path.join(self.tempdir, fname),
                   path.join(self.tempdir, symlink))
        self.assertEqual(fname, getRealpath(symlink, cwd=self.tempdir))

    def testRelativeSymlink(self):
        fname = 'dir/file3.txt'
        symlink = 'localconfig.py'
        self._createRegularFile(fname)
        os.symlink(fname, path.join(self.tempdir, symlink))
        self.assertEqual(fname, getRealpath(symlink, cwd=self.tempdir))

    def testAbsSymlinkToAbsSymlink(self):
        fname = 'dir/file4.txt'
        symlink1 = 'production.py'
        symlink2 = 'localconfig.py'
        self._createRegularFile(fname)
        os.symlink(path.join(self.tempdir, fname),
                   path.join(self.tempdir, symlink1))
        os.symlink(path.join(self.tempdir, symlink1),
                   path.join(self.tempdir, symlink2))
        self.assertEqual(fname, getRealpath(symlink2, cwd=self.tempdir))

    def testAbsSymlinkToRelativeSymlink(self):
        fname = 'dir/file5.txt'
        symlink1 = 'production.py'
        symlink2 = 'localconfig.py'
        self._createRegularFile(fname)
        os.symlink(fname, path.join(self.tempdir, symlink1))
        os.symlink(path.join(self.tempdir, symlink1),
                   path.join(self.tempdir, symlink2))
        self.assertEqual(fname, getRealpath(symlink2, cwd=self.tempdir))

    def testRelativeSymlinkToAbsSymlink(self):
        fname = 'dir/file6.txt'
        symlink1 = 'production.py'
        symlink2 = 'localconfig.py'
        self._createRegularFile(fname)
        os.symlink(path.join(self.tempdir, fname),
                   path.join(self.tempdir, symlink1))
        os.symlink(symlink1, path.join(self.tempdir, symlink2))
        self.assertEqual(fname, getRealpath(symlink2, cwd=self.tempdir))

    def testRelativeSymlinkToRelativeSymlink(self):
        fname = 'dir/file7.txt'
        symlink1 = 'production.py'
        symlink2 = 'localconfig.py'
        self._createRegularFile(fname)
        os.symlink(fname, path.join(self.tempdir, symlink1))
        os.symlink(symlink1, path.join(self.tempdir, symlink2))
        self.assertEqual(fname, getRealpath(symlink2, cwd=self.tempdir))

    def test3Levels(self):
        fname = 'dir/file8.txt'
        symlink1 = 'symlink1'
        symlink2 = 'symlink2'
        symlink3 = 'symlink3'
        self._createRegularFile(fname)
        os.symlink(fname, path.join(self.tempdir, symlink1))
        os.symlink(symlink1, path.join(self.tempdir, symlink2))
        os.symlink(symlink2, path.join(self.tempdir, symlink3))
        self.assertEqual(fname, getRealpath(symlink3, cwd=self.tempdir))

    def testDepth(self):
        fname = 'dir1/dir2/dir3/file9.txt'
        symlink1 = 'production.py'
        symlink2 = 'localconfig.py'
        self._createRegularFile(fname)
        os.symlink(fname, path.join(self.tempdir, symlink1))
        os.symlink(symlink1, path.join(self.tempdir, symlink2))
        self.assertEqual(fname, getRealpath(symlink2, cwd=self.tempdir,
                                            depth=3))


class TestGetRepoDirname(unittest.TestCase):

    def test_regular(self):
        self.assertEqual("repo", get_repo_dirname("path/repo"))

    def test_trailing_slash(self):
        self.assertEqual("repo", get_repo_dirname("path/repo/"))

    def test_one_level(self):
        self.assertEqual("repo", get_repo_dirname("repo"))

    def test_one_level_with_slashes(self):
        self.assertEqual("repo", get_repo_dirname("/repo/"))

    def test_multiple_levels(self):
        self.assertEqual("repo", get_repo_dirname("path//path2/repo"))
