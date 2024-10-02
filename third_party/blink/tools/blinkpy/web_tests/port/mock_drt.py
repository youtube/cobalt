# Copyright (c) 2012 Google Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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
"""This is an implementation of the Port interface that overrides other
ports and changes the Driver binary to "MockDRT".

The MockDRT objects emulate what a real DRT would do. In particular, they
return the output a real DRT would return for a given test, assuming that
test actually passes (except for reftests, which currently cause the
MockDRT to crash).
"""

import base64
import optparse
import os
import sys
import types

# Since we execute this script directly as part of the unit tests, we need to ensure
# that blink/tools is in sys.path for the next imports to work correctly.
tools_dir = os.path.dirname(
    os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
if tools_dir not in sys.path:
    sys.path.insert(0, tools_dir)

from blinkpy.common import exit_codes
from blinkpy.common import read_checksum_from_png
from blinkpy.common.system.system_host import SystemHost
from blinkpy.web_tests.port.driver import DriverInput, DriverOutput
from blinkpy.web_tests.port.factory import PortFactory


class MockDRTPort(object):
    port_name = 'mock'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        return port_name

    def __init__(self, host, port_name, **kwargs):
        self.__delegate = PortFactory(host).get(
            port_name.replace('mock-', ''), **kwargs)
        self.__delegate_driver_class = self.__delegate._driver_class
        self.__delegate._driver_class = types.MethodType(
            self._driver_class, self.__delegate)

    def __getattr__(self, name):
        return getattr(self.__delegate, name)

    def check_build(self, needs_http, printer):
        return exit_codes.OK_EXIT_STATUS

    def check_sys_deps(self):
        return exit_codes.OK_EXIT_STATUS

    def _driver_class(self, delegate):
        return self._mocked_driver_maker

    def _mocked_driver_maker(self, port, worker_number, no_timeout=False):
        path_to_this_file = self.host.filesystem.abspath(
            __file__.replace('.pyc', '.py'))
        driver = self.__delegate_driver_class()(self, worker_number,
                                                no_timeout)
        driver.cmd_line = self._overriding_cmd_line(
            driver.cmd_line, self.__delegate._path_to_driver(), sys.executable,
            path_to_this_file, self.__delegate.name())
        return driver

    @staticmethod
    def _overriding_cmd_line(original_cmd_line, driver_path, python_exe,
                             this_file, port_name):
        def new_cmd_line(per_test_args):
            cmd_line = original_cmd_line(per_test_args)
            index = cmd_line.index(driver_path)
            cmd_line[index:index + 1] = [
                python_exe, this_file, '--platform', port_name
            ]
            return cmd_line

        return new_cmd_line

    def start_http_server(self, additional_dirs, number_of_servers):
        pass

    def start_websocket_server(self):
        pass

    def acquire_http_lock(self):
        pass

    def stop_http_server(self):
        pass

    def stop_websocket_server(self):
        pass

    def release_http_lock(self):
        pass

    def setup_environ_for_server(self):
        env = self.__delegate.setup_environ_for_server()
        # We need to propagate PATH down so the python code can find the checkout.
        env['PATH'] = self.host.environ.get('PATH')
        return env

    def _lookup_virtual_test_args(self, test_name):
        # MockDRTPort doesn't support virtual test suites.
        raise NotImplementedError()


def main(argv, host, stdin, stdout, stderr):
    """Run the tests."""

    options, args = parse_options(argv)
    drt = MockDRT(options, args, host, stdin, stdout, stderr)
    return drt.run()


def parse_options(argv):
    # We do custom arg parsing instead of using the optparse module
    # because we don't want to have to list every command line flag DRT
    # accepts, and optparse complains about unrecognized flags.

    def get_arg(arg_name):
        if arg_name in argv:
            index = argv.index(arg_name)
            return argv[index + 1]
        return None

    options = optparse.Values({
        'actual_directory':
        get_arg('--actual-directory'),
        'platform':
        get_arg('--platform'),
    })
    return (options, argv)


class MockDRT(object):
    def __init__(self, options, args, host, stdin, stdout, stderr):
        self._options = options
        self._args = args
        self._host = host
        self._stdout = stdout
        self._stdin = stdin
        self._stderr = stderr

        port_name = None
        if options.platform:
            port_name = options.platform
        self._port = PortFactory(host).get(
            port_name=port_name, options=options)
        self._driver = self._port.create_driver(0)

    def run(self):
        self._stdout.write(b'#READY\n')
        self._stdout.flush()
        while True:
            line = self._stdin.readline()
            if not line:
                return 0
            driver_input = self.input_from_line(line)
            dirname, basename = self._port.split_test(driver_input.test_name)
            is_reftest = (self._port.reference_files(driver_input.test_name)
                          or self._port.is_reference_html_file(
                              self._port.host.filesystem, dirname, basename))
            output = self.output_for_test(driver_input, is_reftest)
            self.write_test_output(driver_input, output, is_reftest)

    def input_from_line(self, line):
        vals = line.strip().split(b"'")
        uri = vals[0].decode('utf-8')
        checksum = None
        if len(vals) == 2:
            checksum = vals[1]
        elif len(vals) != 1:
            raise NotImplementedError

        if uri.startswith('http://') or uri.startswith('https://'):
            test_name = self._driver.uri_to_test(uri)
        else:
            test_name = self._port.relative_test_filename(uri)

        return DriverInput(
            test_name,
            0,
            checksum,
            wpt_print_mode=self._port.is_wpt_print_reftest(test_name),
            args=[])

    def output_for_test(self, test_input, is_reftest):
        port = self._port
        actual_text = port.expected_text(test_input.test_name)
        actual_audio = port.expected_audio(test_input.test_name)
        actual_image = None
        actual_checksum = None
        if is_reftest:
            # Make up some output for reftests.
            actual_text = b'reference text\n'
            actual_checksum = b'mock-checksum'
            actual_image = b'blank'
            if test_input.test_name.endswith('-mismatch.html'):
                actual_text = b'not reference text\n'
                actual_checksum = b'not-mock-checksum'
                actual_image = b'not blank'
        elif test_input.image_hash:
            actual_checksum = port.expected_checksum(test_input.test_name)
            actual_image = port.expected_image(test_input.test_name)

        if self._options.actual_directory:
            actual_path = port.host.filesystem.join(
                self._options.actual_directory, test_input.test_name)
            root, _ = port.host.filesystem.splitext(actual_path)
            text_path = root + '-actual.txt'
            if port.host.filesystem.exists(text_path):
                actual_text = port.host.filesystem.read_binary_file(text_path)
            audio_path = root + '-actual.wav'
            if port.host.filesystem.exists(audio_path):
                actual_audio = port.host.filesystem.read_binary_file(
                    audio_path)
            image_path = root + '-actual.png'
            if port.host.filesystem.exists(image_path):
                actual_image = port.host.filesystem.read_binary_file(
                    image_path)
                with port.host.filesystem.open_binary_file_for_reading(
                        image_path) as filehandle:
                    actual_checksum = read_checksum_from_png.read_checksum(
                        filehandle)

        return DriverOutput(actual_text, actual_image, actual_checksum,
                            actual_audio)

    def write_test_output(self, test_input, output, is_reftest):
        if output.audio:
            self._stdout.write(b'Content-Type: audio/wav\n')
            self._stdout.write(b'Content-Transfer-Encoding: base64\n')
            self._stdout.write(base64.b64encode(output.audio))
            self._stdout.write(b'\n')
        else:
            self._stdout.write(b'Content-Type: text/plain\n')
            # FIXME: Note that we don't ensure there is a trailing newline!
            # This mirrors actual (Mac) DRT behavior but is a bug.
            if output.text:
                self._stdout.write(output.text)

        self._stdout.write(b'#EOF\n')

        if output.image_hash:
            self._stdout.write(b'\n')
            self._stdout.write(b'ActualHash: ' + output.image_hash + b'\n')
            self._stdout.write(b'ExpectedHash: ' + test_input.image_hash +
                               b'\n')
            if output.image_hash != test_input.image_hash:
                self._stdout.write(b'Content-Type: image/png\n')
                self._stdout.write(b'Content-Length: %s\n' % len(output.image))
                self._stdout.write(output.image)
        self._stdout.write(b'#EOF\n')
        self._stdout.flush()
        self._stderr.write(b'#EOF\n')
        self._stderr.flush()


if __name__ == '__main__':
    # Note that the Mock in MockDRT refers to the fact that it is emulating a
    # real DRT, and as such, it needs access to a real SystemHost, not a MockSystemHost.
    sys.exit(
        main(sys.argv[1:], SystemHost(), sys.stdin, sys.stdout, sys.stderr))
