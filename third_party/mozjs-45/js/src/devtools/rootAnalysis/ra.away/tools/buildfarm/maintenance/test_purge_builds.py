from unittest import TestCase
from purge_builds import isImportant


class TestPurge(TestCase):

    importantDirsPattern = ['release-*', '*-nightly', 'info']
    importantDirs = ['mozilla-central-linux64-nightly',
                     'mozilla-central-linux64-l10n-nightly',
                     'release-mozilla-1.9.2-linux64_build',
                     'release-mozilla-central-win32-opt-unittest-crashtest',
                     'release-mozilla-1.9.1-linux-opt-unittest-mochitests-3',
                     'release-mozilla-central-macosx64_repack_3',
                     'release-mozilla-central-android_update_verify',
                     'info',
                     ]
    notImportantDirs = ['mozilla-central-bundle',
                        'mozilla-1.9.2-linux-l10n-dep',
                        'mozilla-central-win32-xulrunner',
                        'nanojit-macosx64',
                        'fuzzer-linux',
                        'mozilla-central-linux64-debug',
                        ]

    def testImportantDirs(self):
        for d in self.importantDirs:
            self.assertTrue(isImportant(d, self.importantDirsPattern))

    def testNotImportantDirs(self):
        for d in self.notImportantDirs:
            self.assertFalse(isImportant(d, self.importantDirsPattern))
