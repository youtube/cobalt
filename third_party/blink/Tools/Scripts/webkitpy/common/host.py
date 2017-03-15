# Copyright (c) 2010 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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

import logging

from webkitpy.common.checkout.git import Git
from webkitpy.common.config.builders import BUILDERS
from webkitpy.common.net import web
from webkitpy.common.net.buildbot import BuildBot
from webkitpy.common.system.system_host import SystemHost
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.layout_tests.port.factory import PortFactory


_log = logging.getLogger(__name__)


class Host(SystemHost):

    def __init__(self):
        SystemHost.__init__(self)
        self.web = web.Web()

        self._git = None

        # Everything below this line is WebKit-specific and belongs on a higher-level object.
        self.buildbot = BuildBot()

        # FIXME: Unfortunately Port objects are currently the central-dispatch objects of the NRWT world.
        # In order to instantiate a port correctly, we have to pass it at least an executive, user, git, and filesystem
        # so for now we just pass along the whole Host object.
        # FIXME: PortFactory doesn't belong on this Host object if Port is going to have a Host (circular dependency).
        self.port_factory = PortFactory(self)

        self._engage_awesome_locale_hacks()

        self.builders = BuilderList(BUILDERS)

    # We call this from the Host constructor, as it's one of the
    # earliest calls made for all webkitpy-based programs.
    def _engage_awesome_locale_hacks(self):
        # To make life easier on our non-English users, we override
        # the locale environment variables inside webkitpy.
        # If we don't do this, programs like SVN will output localized
        # messages and svn.py will fail to parse them.
        # FIXME: We should do these overrides *only* for the subprocesses we know need them!
        # This hack only works in unix environments.
        self.environ['LANGUAGE'] = 'en'
        self.environ['LANG'] = 'en_US.UTF-8'
        self.environ['LC_MESSAGES'] = 'en_US.UTF-8'
        self.environ['LC_ALL'] = ''

    def git(self, path=None):
        if path:
            return Git(cwd=path, executive=self.executive, filesystem=self.filesystem)
        if not self._git:
            self._git = Git(filesystem=self.filesystem, executive=self.executive)
        return self._git
