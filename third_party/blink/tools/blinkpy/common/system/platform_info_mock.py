# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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


class MockPlatformInfo(object):
    def __init__(self,
                 os_name='mac',
                 os_version='mac10.14',
                 linux_distribution=None,
                 is_highdpi=False,
                 is_running_rosetta=False,
                 machine=None,
                 interactive=True):
        self.os_name = os_name
        self.os_version = os_version
        self.interactive = interactive
        self._linux_distribution = linux_distribution
        self._is_highdpi = is_highdpi
        self._is_running_rosetta = is_running_rosetta
        self._machine = machine or 'x86_64'

    def is_mac(self):
        return self.os_name == 'mac'

    def is_linux(self):
        return self.os_name == 'linux'

    def is_win(self):
        return self.os_name == 'win'

    def is_highdpi(self):
        return self._is_highdpi

    def is_running_rosetta(self):
        return self._is_running_rosetta

    def is_freebsd(self):
        return self.os_name == 'freebsd'

    def display_name(self):
        return 'MockPlatform 1.0'

    def linux_distribution(self):
        return self._linux_distribution if self.is_linux() else None

    def total_bytes_memory(self):
        return 3 * 1024 * 1024 * 1024  # 3GB is a reasonable amount of ram to mock.

    def terminal_width(self):
        return 80

    def get_machine(self):
        return self._machine
