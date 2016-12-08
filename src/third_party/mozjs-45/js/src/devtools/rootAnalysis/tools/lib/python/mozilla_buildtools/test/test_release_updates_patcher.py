from os import path
import unittest

import mock

from apache_conf_parser import ApacheConfParser

from release.updates.patcher import substitutePath, PatcherConfig, PatcherConfigError

samplePatcherConfigObj = PatcherConfig()
samplePatcherConfigObj['appName'] = 'Firefox'
samplePatcherConfigObj['current-update'] = {
    'channel': ['release'],
    'testchannel': ['betatest', 'releasetest'],
    'details': 'https://details',
    'from': '13.0',
    'to': '13.0.1',
    'actions': ['silent'],
    'action-locales': ['en-US'],
    'complete': {
        'betatest-url': 'http://%platform%.%locale%.complete.mar',
        'path': '%platform%.%locale%.complete.mar',
        'url': 'http://%bouncer-platform%.%locale%.complete',
    },
    'partials': {
        '12.0': {
            'betatest-url': 'http://%platform%.%locale%.12.0.partial.mar',
            'path': '%platform%.%locale%.12.0.partial.mar',
            'url': 'http://%bouncer-platform%.%locale%.12.0.partial',
        },
        '13.0': {
            'betatest-url': 'http://%platform%.%locale%.13.0.partial.mar',
            'path': '%platform%.%locale%.13.0.partial.mar',
            'url': 'http://%bouncer-platform%.%locale%.13.0.partial',
        }
    },
}
samplePatcherConfigObj['past-update'] = [
    ['11.0', '12.0', ['betatest', 'releasetest', 'release']],
    ['12.0', '13.0', ['betatest', 'releasetest', 'release']]
]
samplePatcherConfigObj['release'] = {
    '11.0': {
        'checksumsurl': 'http://%platform%.%locale%.11.0.checksum',
        'completemarurl': 'http://%platform%.%locale%.11.0.complete.mar',
        'extension-version': '11.0',
        'locales': ['de', 'en-US', 'ja', 'ja-JP-mac', 'zu'],
        'prettyVersion': '11.0',
        'schema': 1,
        'version': '11.0',
        'exceptions': {
            'ja': ['win32'],
            'ja-JP-mac': ['mac'],
        },
        'platforms': {
            'mac': 12345,
            'win32': 12345,
        },
    },
    '12.0': {
        'checksumsurl': 'http://%platform%.%locale%.12.0.checksum',
        'completemarurl': 'http://%platform%.%locale%.12.0.complete.mar',
        'extension-version': '12.0',
        'locales': ['de', 'en-US', 'ja', 'ja-JP-mac', 'zu'],
        'prettyVersion': '12.0',
        'schema': 2,
        'version': '12.0',
        'exceptions': {
            'ja': ['win32'],
            'ja-JP-mac': ['mac'],
        },
        'platforms': {
            'mac': 22345,
            'win32': 22345,
        },
    },
    '13.0': {
        'checksumsurl': 'http://%platform%.%locale%.13.0.checksum',
        'completemarurl': 'http://%platform%.%locale%.13.0.complete.mar',
        'extension-version': '13.0',
        # Drop "zu" here to make sure that dropping and re-adding a locale
        # works
        'locales': ['de', 'en-US', 'ja', 'ja-JP-mac'],
        'prettyVersion': '13.0',
        'schema': 2,
        'version': '13.0',
        'exceptions': {
            'ja': ['win32', 'win64'],
            'ja-JP-mac': ['mac'],
        },
        # Add win64 here (and in 13.0.1) to make sure platform additions work.
        'platforms': {
            'mac': 32345,
            'win32': 32345,
            'win64': 32345,
        },
    },
    '13.0.1': {
        'checksumsurl': 'http://%platform%.%locale%.13.0.1.checksum',
        'completemarurl': 'http://%platform%.%locale%.13.0.1.complete.mar',
        'extension-version': '13.0.1',
        # Drop "de" here to make sure that fully dropping a locale works
        'locales': ['en-US', 'ja', 'ja-JP-mac', 'zu'],
        'mar-channel-ids': 'firefox-mozilla-beta',
        'prettyVersion': '13.0.1',
        'schema': 2,
        'version': '13.0.1',
        'exceptions': {
            'ja': ['win32', 'win64'],
            'ja-JP-mac': ['mac'],
        },
        'platforms': {
            'mac': 42345,
            'win32': 42345,
            'win64': 42345,
        },
    },
}


class TestPatcherConfig(unittest.TestCase):
    config = path.join(path.dirname(__file__), "sample-patcher-config.cfg")
    badConfig = path.join(path.dirname(__file__), "bad-patcher-config.cfg")

    def _parse(self, section):
        return ApacheConfParser(section, infile=False).nodes[0].body.nodes

    def setUp(self):
        self.pc = PatcherConfig()

    def testGetUpdatePaths(self):
        expected = (
            ('11.0', 'mac', 'en-US', ('betatest',
             'releasetest', 'release'), ('complete',)),
            ('11.0', 'mac', 'ja-JP-mac', ('betatest',
             'releasetest', 'release'), ('complete',)),
            ('11.0', 'mac', 'zu', ('betatest', 'releasetest',
             'release'), ('complete',)),
            ('11.0', 'win32', 'en-US', ('betatest',
             'releasetest', 'release'), ('complete',)),
            ('11.0', 'win32', 'ja', ('betatest', 'releasetest',
             'release'), ('complete',)),
            ('11.0', 'win32', 'zu', ('betatest', 'releasetest',
             'release'), ('complete',)),
            ('12.0', 'mac', 'en-US', ('betatest',
             'releasetest', 'release'), ('complete', 'partial')),
            ('12.0', 'mac', 'ja-JP-mac', ('betatest',
             'releasetest', 'release'), ('complete', 'partial')),
            ('12.0', 'mac', 'zu', ('betatest', 'releasetest',
             'release'), ('complete', 'partial')),
            ('12.0', 'win32', 'en-US', ('betatest',
             'releasetest', 'release'), ('complete', 'partial')),
            ('12.0', 'win32', 'ja', ('betatest', 'releasetest',
             'release'), ('complete', 'partial')),
            ('12.0', 'win32', 'zu', ('betatest', 'releasetest',
             'release'), ('complete', 'partial')),
            ('13.0', 'mac', 'en-US', ('betatest',
             'releasetest', 'release'), ('complete', 'partial')),
            ('13.0', 'mac', 'ja-JP-mac', ('betatest',
             'releasetest', 'release'), ('complete', 'partial')),
            ('13.0', 'win32', 'en-US', ('betatest',
             'releasetest', 'release'), ('complete', 'partial')),
            ('13.0', 'win32', 'ja', ('betatest', 'releasetest',
             'release'), ('complete', 'partial')),
            ('13.0', 'win64', 'en-US', ('betatest',
             'releasetest', 'release'), ('complete', 'partial')),
            ('13.0', 'win64', 'ja', ('betatest', 'releasetest',
             'release'), ('complete', 'partial')),
        )
        got = []
        # getUpdatePaths is a generator, so we need to store the results before
        # comparison
        for path in samplePatcherConfigObj.getUpdatePaths():
            got.append(path)
        self.assertEquals(sorted(got), sorted(expected))

    def testGetUpdatePathsNone(self):
        got = []
        for path in self.pc.getUpdatePaths():
            got.append(path)
        self.assertEquals(got, [])

    def testReadXml(self):
        self.pc.readXml(open(self.config).read())
        self.assertEquals(self.pc, samplePatcherConfigObj)

    def testReadXmlMultipleCurrentUpdate(self):
        bad = open(self.badConfig).read()
        self.assertRaises(PatcherConfigError, self.pc.readXml, bad)

    def testParsePastUpdate(self):
        expected = ['11.0', '12.0', ['betatest', 'releasetest']]
        got = self.pc.parsePastUpdate(
            ['11.0', '12.0', 'betatest', 'releasetest'])
        self.assertEquals(got, expected)

    def testParsePastUpdateOneChannel(self):
        expected = ['11.0', '12.0', ['betatest']]
        got = self.pc.parsePastUpdate(['11.0', '12.0', 'betatest'])
        self.assertEquals(got, expected)

    def testParsePastUpdateNoChannels(self):
        self.assertRaises(
            PatcherConfigError, self.pc.parsePastUpdate, ['11.0', '12.0'])

    def testParseCurrentUpdate(self):
        dom = self._parse("""\
<current-update>
    channel   release
    <complete>
        path    %platform%.%locale%.complete.mar
    </complete>
    details   https://details
    from   13.0
    <partials>
        <12.0>
            betatest-url http://%platform%.%locale%.12.0.partial.mar
            path %platform%.%locale%.12.0.partial.mar
            url http://%bouncer-platform%.%locale%.12.0.partial
        </12.0>
        <13.0>
            betatest-url http://%platform%.%locale%.13.0.partial.mar
            path %platform%.%locale%.13.0.partial.mar
            url http://%bouncer-platform%.%locale%.13.0.partial
        </13.0>
    </partials>
    testchannel   betatest releasetest
    to   13.0.1
</current-update>
""")
        expected = {
            'channel': ['release'],
            'details': 'https://details',
            'from': '13.0',
            'testchannel': ['betatest', 'releasetest'],
            'to': '13.0.1',
            'complete': {
                'path': '%platform%.%locale%.complete.mar',
            },
            'partials': {
                '12.0': {
                    'betatest-url': 'http://%platform%.%locale%.12.0.partial.mar',
                    'path': '%platform%.%locale%.12.0.partial.mar',
                    'url': 'http://%bouncer-platform%.%locale%.12.0.partial',
                },
                '13.0': {
                    'betatest-url': 'http://%platform%.%locale%.13.0.partial.mar',
                    'path': '%platform%.%locale%.13.0.partial.mar',
                    'url': 'http://%bouncer-platform%.%locale%.13.0.partial',
                }
            },
        }
        got = self.pc.parseCurrentUpdate(dom)
        self.assertEquals(got, expected)

    def testParseCurrentUpdateWithForce(self):
        dom = self._parse("""\
<current-update>
    channel   release
    <complete>
        path    %platform%.%locale%.complete.mar
    </complete>
    details   https://details
    from   13.0
    testchannel   betatest releasetest
    to   13.0.1
    force foo
    force bar
</current-update>
""")
        expected = {
            'channel': ['release'],
            'details': 'https://details',
            'from': '13.0',
            'testchannel': ['betatest', 'releasetest'],
            'to': '13.0.1',
            'complete': {
                'path': '%platform%.%locale%.complete.mar',
            },
            'force': ['foo', 'bar'],
        }
        got = self.pc.parseCurrentUpdate(dom)
        self.assertEquals(got, expected)

    def testParseCurrentUpdateMultipleComplete(self):
        dom = self._parse("""\
<current-update>
    channel   release
    <complete>
        path    %platform%.%locale%.complete.mar
    </complete>
    details   https://details
    from   13.0
    <complete>
        path    %platform%.%locale%.partial.mar
    </complete>
    testchannel   betatest releasetest
    to   13.0.1
</current-update>
""")
        self.assertRaises(PatcherConfigError, self.pc.parseCurrentUpdate, dom)

    def testParseRelease(self):
        dom = self._parse("""\
<11.0>
    checksumsurl http://%platform%.%locale%.checksum
    completemarurl  http://%platform%.%locale%.complete
    <exceptions>
        ja   win32, linux-i686
        ja-JP-mac   mac
    </exceptions>
    extension-version   11.0
    locales   de en-US ja ja-JP-mac zu
    mar-channel-ids   firefox-mozilla-beta,firefox-mozilla-release
    <platforms>
        linux-i686 12345
        mac   12345
        win32   12345
    </platforms>
    prettyVersion   11.0
    schema   2
    version   11.0
</11.0>
""")
        expected = {
            'checksumsurl': 'http://%platform%.%locale%.checksum',
            'completemarurl': 'http://%platform%.%locale%.complete',
            'extension-version': '11.0',
            'locales': ['de', 'en-US', 'ja', 'ja-JP-mac', 'zu'],
            'mar-channel-ids': 'firefox-mozilla-beta,firefox-mozilla-release',
            'prettyVersion': '11.0',
            'schema': 2,
            'version': '11.0',
            'exceptions': {
                'ja': ['win32', 'linux-i686'],
                'ja-JP-mac': ['mac'],
            },
            'platforms': {
                'linux-i686': 12345,
                'mac': 12345,
                'win32': 12345,
            },
        }
        got = self.pc.parseRelease(dom)
        self.assertEquals(got, expected)

    def testParseReleaseMultipleNodesSameVersion(self):
        dom = self._parse("""\
<11.0>
version   11.0
version   12.0
</11.0>
""")
        self.assertRaises(PatcherConfigError, self.pc.parseRelease, dom)

    def testAddPastUpdate(self):
        self.pc.addPastUpdate(['1', '2', ['test']])
        self.assertEquals(self.pc['past-update'], [['1', '2', ['test']]])

    def testAddDuplicatePastUpdate(self):
        self.pc['past-update'] = [['1', '2', ['abc']]]
        self.assertRaises(
            PatcherConfigError, self.pc.addPastUpdate, ['1', '2', ['test']])

    def testAddDuplicatePastUpdate2(self):
        self.pc['past-update'] = [['1', '2', ['abc']]]
        self.assertRaises(
            PatcherConfigError, self.pc.addPastUpdate, ['1', '3', ['test']])

    def testAddRelease(self):
        self.pc.addRelease('3.0', {
            'schema': 2,
            'version': '3.0',
        })
        self.assertEquals(
            self.pc['release'], {'3.0': {'schema': 2, 'version': '3.0'}})

    def testAddDuplicateRelease(self):
        self.pc['release']['2.0'] = {}
        self.assertRaises(PatcherConfigError, self.pc.addRelease, '2.0', {})

    def testGetUrlComplete(self):
        pc = samplePatcherConfigObj
        with mock.patch.dict('release.platforms.ftp_bouncer_platform_map', {'p': 'pp'}):
            got = pc.getUrl('12.0', 'p', 'l', 'complete', 'release')
            self.assertEquals(got, 'http://pp.l.complete')

    def testGetUrlCompleteOverride(self):
        pc = samplePatcherConfigObj
        got = pc.getUrl('13.0', 'p', 'l', 'complete', 'betatest')
        self.assertEquals(got, 'http://p.l.complete.mar')

    def testGetUrlPartial(self):
        pc = samplePatcherConfigObj
        got = pc.getUrl('12.0', 'p', 'l', 'partial', 'releasetest')
        self.assertEquals(got, 'http://p.l.12.0.partial')

    def testGetUrlNonExistentPartial(self):
        pc = samplePatcherConfigObj
        self.assertRaises(
            PatcherConfigError, pc.getUrl, '15.0', 'p', 'l', 'partial', 'a')

    def testGetPathComplete(self):
        pc = samplePatcherConfigObj
        got = pc.getPath('11.0', 'p', 'l', 'complete')
        self.assertEquals(got, 'p.l.complete.mar')

    def testGetPathPartial(self):
        pc = samplePatcherConfigObj
        got = pc.getPath('12.0', 'p', 'l', 'partial')
        self.assertEquals(got, 'p.l.12.0.partial.mar')

    def testGetOptionalAttrsSchema1(self):
        pc = samplePatcherConfigObj
        self.assertEquals(pc.getOptionalAttrs('11.0', 'en-US'), {})

    def testGetOptionalAttrsSchema2(self):
        pc = samplePatcherConfigObj
        self.assertEquals(pc.getOptionalAttrs('12.0', 'en-US'), {'actions': ['silent']})

    def testGetOptionalAttrsSchema2WrongLocale(self):
        pc = samplePatcherConfigObj
        self.assertEquals(pc.getOptionalAttrs('12.0', 'de'), {})

    def testGetFromVersions(self):
        pc = samplePatcherConfigObj
        self.assertEquals(
            sorted(pc.getFromVersions()), ['11.0', '12.0', '13.0'])


class TestSubstitutePath(unittest.TestCase):
    def testNoSubstitutes(self):
        self.assertEquals(
            substitutePath('/foo/bar/blah', 'aaa', 'bbb'), '/foo/bar/blah')

    def testSubstituteLocale(self):
        self.assertEquals(substitutePath(
            '/foo/%locale%/blah', 'aaa', 'bbb'), '/foo/bbb/blah')

    def testSubstituteBouncerPlatform(self):
        with mock.patch.dict('release.platforms.ftp_bouncer_platform_map', {'abc': 'xxx'}):
            self.assertEquals(substitutePath(
                '/%bouncer-platform%/boo', platform='abc'), '/xxx/boo')

    def testSubstituteVersion(self):
        self.assertEquals(substitutePath(
            '/abc/def/%version%', version='123'), '/abc/def/123')

    def testNoSubFound(self):
        self.assertRaises(TypeError, substitutePath, '%platform%')
