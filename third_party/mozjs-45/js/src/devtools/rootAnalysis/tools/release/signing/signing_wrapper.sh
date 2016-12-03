#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

unset LAUNCHD_SOCKET

exec launchctl bsexec / sudo -u cltsign bin/python tools/release/signing/signing-server.py signing.ini $@

