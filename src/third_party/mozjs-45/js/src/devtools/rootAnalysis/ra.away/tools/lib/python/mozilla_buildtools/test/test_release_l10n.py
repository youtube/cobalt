import unittest

from release.l10n import parsePlainL10nChangesets, getL10nRepositories


class TestParsePlainL10nChangesets(unittest.TestCase):
    def testSimple(self):
        contents = """\
af abc123
zh-TW blah"""
        expected = {
            'af': 'abc123',
            'zh-TW': 'blah',
        }
        got = parsePlainL10nChangesets(contents)
        self.assertEquals(got, expected)

    def testMissingRevision(self):
        contents = """\
af abc123
zh-TW"""
        self.assertRaises(ValueError, parsePlainL10nChangesets, contents)


class TestGetL10nRepositories(unittest.TestCase):
    def testSimple(self):
        contents = """\
af abc123
zh-TW blah"""
        expected = {
            'l10n/af': {
                'revision': 'abc123',
                'relbranchOverride': None,
                'bumpFiles': [],
            },
            'l10n/zh-TW': {
                'revision': 'blah',
                'relbranchOverride': None,
                'bumpFiles': [],
            }
        }
        got = getL10nRepositories(contents, 'l10n')
        self.assertEquals(got, expected)
