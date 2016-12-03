import unittest
import subprocess
import os

from nose.plugins.skip import SkipTest

from util.commands import run_cmd, get_output, run_cmd_periodic_poll, TERMINATED_PROCESS_MSG


def has_urandom():
    return os.path.exists('/dev/urandom')


class TestRunCmd(unittest.TestCase):
    def testSimple(self):
        self.assertEquals(run_cmd(['true']), 0)

    def testFailure(self):
        self.assertRaises(subprocess.CalledProcessError, run_cmd, ['false'])


class TestRunCmdPeriodicPoll(unittest.TestCase):
    def testSimple(self):
        self.assertEquals(run_cmd_periodic_poll(['true']), 0)

    def testFailure(self):
        self.assertRaises(subprocess.CalledProcessError, run_cmd_periodic_poll,
                          ['false'])

    def testSuccess2secs(self):
        self.assertEquals(
            run_cmd_periodic_poll(['bash', '-c', 'sleep 2 && true']),
            0)

    def testFailure2secs(self):
        self.assertRaises(
            subprocess.CalledProcessError, run_cmd_periodic_poll,
            ['bash', '-c', 'sleep 2 && false'])

    def testSuccess3secsWith2secsPoll(self):
        self.assertEquals(
            run_cmd_periodic_poll(['bash', '-c', 'sleep 3 && true'],
                                  warning_interval=2),
            0)

    def testTerminatedByTimeout(self):
        self.assertRaises(
            subprocess.CalledProcessError, run_cmd_periodic_poll,
            ['bash', '-c', 'sleep 1 && true'], warning_interval=1, timeout=0.5)

    def testSuccessCallback(self):
        self.callback_called = 0

        def callback(start_time, eapsed, proc):
            self.callback_called += 1

        run_cmd_periodic_poll(['bash', '-c', 'sleep 5 && true'],
                              warning_interval=2, warning_callback=callback),
        self.assertEqual(self.callback_called, 2)

    def testFailureCallback(self):
        self.callback_called = 0

        def callback(start_time, eapsed, proc):
            self.callback_called += 1

        self.assertRaises(
            subprocess.CalledProcessError, run_cmd_periodic_poll,
            ['bash', '-c', 'sleep 5 && false'], warning_interval=2,
            warning_callback=callback)
        self.assertEqual(self.callback_called, 2)


class TestGetOutput(unittest.TestCase):
    def testOutput(self):
        output = get_output(['echo', 'hello'])
        self.assertEquals(output, 'hello\n')

    def testStdErr(self):
        output = get_output(
            ['bash', '-c', 'echo hello 1>&2'], include_stderr=True)
        self.assertEquals(output, 'hello\n')

    def testNoStdErr(self):
        output = get_output(['bash', '-c', 'echo hello 1>&2'])
        self.assertEquals(output, '')

    def testBadOutput(self):
        self.assertRaises(subprocess.CalledProcessError, get_output, ['false'])

    def testOutputAttachedToError(self):
        """Older versions of CalledProcessError don't attach 'output' to
           themselves. This test is to ensure that get_output always does."""
        try:
            get_output(['bash', '-c', 'echo hello && false'])
        except subprocess.CalledProcessError, e:
            self.assertEquals(e.output, 'hello\n')
        else:
            self.fail("get_output did not raise CalledProcessError")

    def testOutputTimeout(self):
        try:
            get_output(['bash', '-c', 'sleep 5'],
                       timeout=1)
        except subprocess.CalledProcessError, e:
            self.assertTrue(e.output.startswith(TERMINATED_PROCESS_MSG))
        else:
            self.fail("get_output did not raise CalledProcessError")

    def testTerminateAndGetOutputGetStdErrorToo(self):
        text_s = "this is just a test"
        text_e = "this should go to the stderr"
        cmd = ['bash', '-c', 'echo "%s" && echo "%s" 1>&2 && sleep 1 && true' % (text_s, text_e)]
        try:
            get_output(cmd, include_stderr=True, timeout=0.5)
        except subprocess.CalledProcessError as error:
            self.assertTrue(text_s in error.output)
            self.assertTrue(text_e in error.output)
        else:
            self.fail("get_output did not raise CalledProcessError")

    def testTerminateAndGetOutput(self):
        text_s = "this is just a test"
        text_e = "this should go to the stderr"
        cmd = ['bash', '-c', 'echo "%s" && echo "%s" 1>&2 && sleep 1 && true' % (text_s, text_e)]
        try:
            get_output(cmd, timeout=0.5)
        except subprocess.CalledProcessError as error:
            self.assertTrue(text_s in error.output)
            self.assertTrue(text_e not in error.output)
        else:
            self.fail("get_output did not raise CalledProcessError")

    def testInsaneOutputStreamTimeout(self):
        if not has_urandom():
            raise SkipTest
        text_s = "this is just a test"
        # create a huge output, check that text_s is in the raised exception
        # output.
        # tested with ~400 MB output: decreasing the size of the output to make
        # tests faster (terminates happens on time, but proc.communicate() can
        # take few seconds)
        cmd = ['bash', '-c', 'echo "%s" && dd if=/dev/urandom bs=4096 count=1000 2>&1 && sleep 2 && true' % text_s]
        try:
            get_output(cmd, include_stderr=False, timeout=1.0)
        except subprocess.CalledProcessError as error:
            self.assertTrue(text_s in error.output)
        else:
            self.fail("get_output did not raise CalledProcessError")
