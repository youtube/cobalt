# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Chromium Mac implementation of the Port interface."""

import logging

from blinkpy.web_tests.port import base

_log = logging.getLogger(__name__)


class MacPort(base.Port):
    SUPPORTED_VERSIONS = ('mac10.13', 'mac10.14', 'mac10.15', 'mac11',
                          'mac11-arm64', 'mac12', 'mac12-arm64', 'mac13',
                          'mac13-arm64')
    port_name = 'mac'

    FALLBACK_PATHS = {}

    FALLBACK_PATHS['mac13'] = ['mac']
    FALLBACK_PATHS['mac13-arm64'] = ['mac-mac13-arm64'
                                     ] + FALLBACK_PATHS['mac13']

    FALLBACK_PATHS['mac12'] = ['mac-mac12'] + FALLBACK_PATHS['mac13']
    FALLBACK_PATHS['mac12-arm64'] = ['mac-mac12-arm64'
                                     ] + FALLBACK_PATHS['mac12']
    FALLBACK_PATHS['mac11'] = ['mac-mac11'] + FALLBACK_PATHS['mac12']
    FALLBACK_PATHS['mac11-arm64'] = ['mac-mac11-arm64'
                                     ] + FALLBACK_PATHS['mac11']
    FALLBACK_PATHS['mac10.15'] = ['mac-mac10.15'] + FALLBACK_PATHS['mac11']
    FALLBACK_PATHS['mac10.14'] = ['mac-mac10.14'] + FALLBACK_PATHS['mac10.15']
    FALLBACK_PATHS['mac10.13'] = ['mac-mac10.13'] + FALLBACK_PATHS['mac10.14']

    CONTENT_SHELL_NAME = 'Content Shell'

    BUILD_REQUIREMENTS_URL = 'https://chromium.googlesource.com/chromium/src/+/main/docs/mac_build_instructions.md'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('mac'):
            parts = [port_name, host.platform.os_version]
            # Maybe add an architecture suffix.
            # In this context, 'arm64' refers to Apple M1.
            # No suffix is appended for Intel-based ports.
            if (host.platform.get_machine() == 'arm64'
                    or host.platform.is_running_rosetta()):
                # TODO(crbug.com/1253659): verify this under native arm.
                parts.append('arm64')
            return '-'.join(parts)
        return port_name

    def __init__(self, host, port_name, **kwargs):
        super(MacPort, self).__init__(host, port_name, **kwargs)

        self._version = port_name[port_name.index('mac-') + len('mac-'):]

        if self._version.endswith('arm64'):
            self._architecture = 'arm64'

        assert self._version in self.SUPPORTED_VERSIONS

    def check_build(self, needs_http, printer):
        result = super(MacPort, self).check_build(needs_http, printer)
        if result:
            _log.error('For complete Mac build requirements, please see:')
            _log.error('')
            _log.error(
                '    https://chromium.googlesource.com/chromium/src/+/main/docs/mac_build_instructions.md'
            )

        return result

    def operating_system(self):
        return 'mac'

    #
    # PROTECTED METHODS
    #

    def path_to_apache(self):
        import platform
        if platform.machine() == 'arm64':
            return self._path_from_chromium_base('third_party',
                                                 'apache-mac-arm64', 'bin',
                                                 'httpd')
        return self._path_from_chromium_base(
            'third_party', 'apache-mac', 'bin', 'httpd')

    def path_to_apache_config_file(self):
        config_file_basename = 'apache2-httpd-%s-php7.conf' % (self._apache_version(),)
        return self._filesystem.join(self.apache_config_directory(), config_file_basename)

    def default_smoke_test_only(self):
        # only run platform specific tests on older mac versions
        if self._version in {'mac10.13', 'mac10.14'}:
            return True
        return super().default_smoke_test_only()

    def path_to_smoke_tests_file(self):
        if self._version in {'mac10.13', 'mac10.14'}:
            return self._filesystem.join(self.web_tests_dir(), 'SmokeTests',
                                         'Mac.txt')
        return super().path_to_smoke_tests_file()

    def _path_to_driver(self, target=None):
        return self._build_path_with_target(target,
                                            self.driver_name() + '.app',
                                            'Contents', 'MacOS',
                                            self.driver_name())
