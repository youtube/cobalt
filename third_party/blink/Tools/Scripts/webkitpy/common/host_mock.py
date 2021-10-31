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

from webkitpy.common.config.builders import BUILDERS
from webkitpy.common.checkout.git_mock import MockGit
from webkitpy.common.net.buildbot_mock import MockBuildBot
from webkitpy.common.net.web_mock import MockWeb
from webkitpy.common.system.system_host_mock import MockSystemHost
from webkitpy.common.webkit_finder import WebKitFinder

# New-style ports need to move down into webkitpy.common.
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.layout_tests.port.factory import PortFactory
from webkitpy.layout_tests.port.test import add_unit_tests_to_mock_filesystem


class MockHost(MockSystemHost):

    def __init__(self,
                 log_executive=False,
                 web=None,
                 git=None,
                 os_name=None,
                 os_version=None,
                 time_return_val=123):
        super(MockHost, self).__init__(
            log_executive=log_executive,
            os_name=os_name,
            os_version=os_version,
            time_return_val=time_return_val)

        add_unit_tests_to_mock_filesystem(self.filesystem)
        self._add_base_manifest_to_mock_filesystem(self.filesystem)
        self.web = web or MockWeb()
        self._git = git

        self.buildbot = MockBuildBot()

        # Note: We're using a real PortFactory here. Tests which don't wish to depend
        # on the list of known ports should override this with a MockPortFactory.
        self.port_factory = PortFactory(self)

        self.builders = BuilderList(BUILDERS)

    def git(self, path=None):
        if path:
            return MockGit(cwd=path, filesystem=self.filesystem, executive=self.executive)
        if not self._git:
            self._git = MockGit(filesystem=self.filesystem, executive=self.executive)
        # Various pieces of code (wrongly) call filesystem.chdir(checkout_root).
        # Making the checkout_root exist in the mock filesystem makes that chdir not raise.
        self.filesystem.maybe_make_directory(self._git.checkout_root)
        return self._git

    def _add_base_manifest_to_mock_filesystem(self, filesystem):
        webkit_finder = WebKitFinder(filesystem)

        external_dir = webkit_finder.path_from_webkit_base('LayoutTests', 'external')
        filesystem.maybe_make_directory(filesystem.join(external_dir, 'wpt'))

        manifest_base_path = filesystem.join(external_dir, 'WPT_BASE_MANIFEST.json')
        filesystem.files[manifest_base_path] = '{}'
