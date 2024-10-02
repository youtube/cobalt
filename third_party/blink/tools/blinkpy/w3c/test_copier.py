# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
"""Logic for converting and copying files from a W3C repo.

This module is responsible for modifying and copying a subset of the tests from
a local W3C repository source directory into a destination directory.
"""

import logging

from blinkpy.w3c.common import is_basename_skipped
from blinkpy.common import path_finder
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.typ_types import ResultType

_log = logging.getLogger(__name__)

# Directory for imported tests relative to the web tests base directory.
DEST_DIR_NAME = 'external'


class TestCopier(object):
    def __init__(self, host, source_repo_path):
        """Initializes variables to prepare for copying and converting files.

        Args:
            host: An instance of Host.
            source_repo_path: Path to the local checkout of web-platform-tests.
        """
        self.host = host

        assert self.host.filesystem.exists(source_repo_path)
        self.source_repo_path = source_repo_path

        self.filesystem = self.host.filesystem
        self.path_finder = path_finder.PathFinder(self.filesystem)
        self.web_tests_dir = self.path_finder.web_tests_dir()
        self.destination_directory = self.filesystem.normpath(
            self.filesystem.join(
                self.web_tests_dir, DEST_DIR_NAME,
                self.filesystem.basename(self.source_repo_path)))
        self.import_in_place = (
            self.source_repo_path == self.destination_directory)
        self.dir_above_repo = self.filesystem.dirname(self.source_repo_path)

        self.import_list = []

        # This is just a FYI list of CSS properties that still need to be prefixed,
        # which may be output after importing.
        self._prefixed_properties = {}

    def do_import(self):
        _log.info('Importing %s into %s', self.source_repo_path,
                  self.destination_directory)
        self.find_importable_tests()
        self.import_tests()

    def find_importable_tests(self):
        """Walks through the source directory to find what tests should be imported.

        This function sets self.import_list, which contains information about how many
        tests are being imported, and their source and destination paths.
        """
        paths_to_skip = self.find_paths_to_skip()

        for root, dirs, files in self.filesystem.walk(self.source_repo_path):
            cur_dir = root.replace(self.dir_above_repo + '/', '') + '/'
            _log.debug('Scanning %s...', cur_dir)

            dirs_to_skip = ('.git', )

            if dirs:
                for name in dirs_to_skip:
                    if name in dirs:
                        dirs.remove(name)

                for path in paths_to_skip:
                    path_base = path.replace(DEST_DIR_NAME + '/', '')
                    path_base = path_base.replace(cur_dir, '')
                    path_full = self.filesystem.join(root, path_base)
                    if path_base in dirs:
                        _log.info('Skipping: %s', path_full)
                        dirs.remove(path_base)
                        if self.import_in_place:
                            self.filesystem.rmtree(path_full)

            copy_list = []

            for filename in files:
                path_full = self.filesystem.join(root, filename)
                path_base = path_full.replace(self.source_repo_path + '/', '')
                path_base = self.destination_directory.replace(
                    self.web_tests_dir + '/', '') + '/' + path_base
                if path_base in paths_to_skip:
                    if self.import_in_place:
                        _log.debug('Pruning: %s', path_base)
                        self.filesystem.remove(path_full)
                        continue
                    else:
                        continue
                # FIXME: This block should really be a separate function, but the early-continues make that difficult.

                if is_basename_skipped(filename):
                    _log.debug('Skipping: %s', path_full)
                    _log.debug(
                        '  Reason: This file may cause Chromium presubmit to fail.'
                    )
                    continue

                copy_list.append({'src': path_full, 'dest': filename})

            if copy_list:
                # Only add this directory to the list if there's something to import
                self.import_list.append({
                    'dirname': root,
                    'copy_list': copy_list
                })

    def find_paths_to_skip(self):
        paths_to_skip = set()
        port = self.host.port_factory.get()
        w3c_import_expectations_path = self.path_finder.path_from_web_tests(
            'W3CImportExpectations')
        w3c_import_expectations = self.filesystem.read_text_file(
            w3c_import_expectations_path)
        expectations = TestExpectations(
            port, {w3c_import_expectations_path: w3c_import_expectations})

        # get test names that should be skipped
        for line in expectations.get_updated_lines(
                w3c_import_expectations_path):
            if line.is_glob:
                _log.warning(
                    'W3CImportExpectations:%d Globs are not allowed in this file.',
                    line.lineno)
                continue
            if ResultType.Skip in line.results:
                if line.tags:
                    _log.warning(
                        'W3CImportExpectations:%d should not have any specifiers',
                        line.lineno)
                paths_to_skip.add(line.test)

        return paths_to_skip

    def import_tests(self):
        """Reads |self.import_list|, and converts and copies files to their destination."""
        for dir_to_copy in self.import_list:
            if not dir_to_copy['copy_list']:
                continue

            orig_path = dir_to_copy['dirname']

            relative_dir = self.filesystem.relpath(orig_path,
                                                   self.source_repo_path)
            dest_dir = self.filesystem.join(self.destination_directory,
                                            relative_dir)

            if not self.filesystem.exists(dest_dir):
                self.filesystem.maybe_make_directory(dest_dir)

            for file_to_copy in dir_to_copy['copy_list']:
                self.copy_file(file_to_copy, dest_dir)

        _log.info('')
        _log.info('Import complete')
        _log.info('')

        if self._prefixed_properties:
            _log.info('Properties needing prefixes (by count):')
            for prefixed_property in sorted(
                    self._prefixed_properties,
                    key=lambda p: self._prefixed_properties[p]):
                _log.info('  %s: %s', prefixed_property,
                          self._prefixed_properties[prefixed_property])

    def copy_file(self, file_to_copy, dest_dir):
        """Converts and copies a file, if it should be copied.

        Args:
            file_to_copy: A dict in a file copy list constructed by
                find_importable_tests, which represents one file to copy, including
                the keys:
                    "src": Absolute path to the source location of the file.
                    "destination": File name of the destination file.
                And possibly also the keys "reference_support_info" or "is_jstest".
            dest_dir: Path to the directory where the file should be copied.
        """
        source_path = self.filesystem.normpath(file_to_copy['src'])
        dest_path = self.filesystem.join(dest_dir, file_to_copy['dest'])

        if self.filesystem.isdir(source_path):
            _log.error('%s refers to a directory', source_path)
            return

        if not self.filesystem.exists(source_path):
            _log.error('%s not found. Possible error in the test.',
                       source_path)
            return

        if not self.filesystem.exists(self.filesystem.dirname(dest_path)):
            if not self.import_in_place:
                self.filesystem.maybe_make_directory(
                    self.filesystem.dirname(dest_path))

        relpath = self.filesystem.relpath(dest_path, self.web_tests_dir)
        # FIXME: Maybe doing a file diff is in order here for existing files?
        # In other words, there's no sense in overwriting identical files, but
        # there's no harm in copying the identical thing.
        _log.debug('  copying %s', relpath)

        if not self.import_in_place:
            self.filesystem.copyfile(source_path, dest_path)
            # Fix perms: https://github.com/web-platform-tests/wpt/issues/23997
            if self.filesystem.read_binary_file(source_path)[:2] == b'#!' or \
                    self.filesystem.splitext(source_path)[1].lower() == '.bat':
                self.filesystem.make_executable(dest_path)
