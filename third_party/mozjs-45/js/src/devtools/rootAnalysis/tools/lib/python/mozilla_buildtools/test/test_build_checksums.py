import unittest

from build.checksums import parseChecksumsFile


class TestParseChecksumFile(unittest.TestCase):
    def testSimple(self):
        expected = {'xxxxx': {'size': 1, 'hashes': {'sha512': 'aaa'}}}
        got = parseChecksumsFile("aaa sha512 1 xxxxx")
        self.assertEquals(got, expected)

    def testMultipleHashes(self):
        expected = {'yyy': {'size': 5, 'hashes': {'sha512': 'aaa',
                                                  'md5': 'bbb'}}}
        got = parseChecksumsFile("""\
aaa sha512 5 yyy
bbb md5 5 yyy
""")
        self.assertEquals(got, expected)

    def testMultipleFiles(self):
        expected = {'a': {'size': 1, 'hashes': {'sha512': 'd'}},
                    'b': {'size': 2, 'hashes': {'sha512': 'e'}}}
        got = parseChecksumsFile("""\
d sha512 1 a
e sha512 2 b
""")
        self.assertEquals(got, expected)

    def testSpacesInFilename(self):
        expected = {'b d': {'size': 2, 'hashes': {'sha512': 'g'}}}
        got = parseChecksumsFile("""\
g sha512 2 b d
""")
        self.assertEquals(got, expected)

    def testFilesizeMismatch(self):
        self.assertRaises(
            ValueError, parseChecksumsFile, """a a 1 a\nb b 2 a""")

    def testHashMismatch(self):
        self.assertRaises(
            ValueError, parseChecksumsFile, """c c 3 c\nd c 3 c""")

    def testInvalidSize(self):
        self.assertRaises(ValueError, parseChecksumsFile, "b c -2 d")
