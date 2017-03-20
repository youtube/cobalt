from unittest import TestCase
import subprocess
import shutil
import tempfile

from util.archives import bzip2, bunzip2


class TestSigningUtils(TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def testBzip2(self):
        fn = "%s/foo" % self.tmpdir
        open(fn, "w").write("hello")
        bzip2(fn)
        proc = subprocess.Popen(["bzcat", fn], stdout=subprocess.PIPE)
        self.assertEquals("hello", proc.stdout.read())

        bunzip2(fn)
        self.assertEquals("hello", open(fn, 'rb').read())
