import unittest

from jinja2 import UndefinedError

from release.config import substituteReleaseConfig


class TestSubstituteReleaseConfig(unittest.TestCase):
    def testSimple(self):
        config = '{{ product }} {{ version }} {{ branch }}'
        got = substituteReleaseConfig(config, 'foo', '1.0', branch='blah')
        self.assertEquals(got, 'foo 1.0 blah')

    def testUndefined(self):
        config = '{{ blech }}'
        self.assertRaises(
            UndefinedError, substituteReleaseConfig, config, '1', '2.0')
