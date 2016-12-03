# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import mock
import unittest

from util.retry import retry, retriable, retrying, retrier

ATTEMPT_N = 1


def _succeedOnSecondAttempt(foo=None, exception=Exception):
    global ATTEMPT_N
    if ATTEMPT_N == 2:
        ATTEMPT_N += 1
        return
    ATTEMPT_N += 1
    raise exception("Fail")


def _alwaysPass():
    global ATTEMPT_N
    ATTEMPT_N += 1
    return True


def _mirrorArgs(*args, **kwargs):
    return args, kwargs


def _alwaysFail():
    raise Exception("Fail")


class NewError(Exception):
    pass


class OtherError(Exception):
    pass


def _raiseCustomException():
    return _succeedOnSecondAttempt(exception=NewError)


class TestRetry(unittest.TestCase):
    def setUp(self):
        global ATTEMPT_N
        ATTEMPT_N = 1
        self.sleep_patcher = mock.patch('time.sleep')
        self.sleep_patcher.start()

    def tearDown(self):
        self.sleep_patcher.stop()

    def testRetrySucceed(self):
        # Will raise if anything goes wrong
        retry(_succeedOnSecondAttempt, attempts=2, sleeptime=0, jitter=0)

    def testRetriableSucceed(self):
        func = retriable(attempts=2, sleeptime=0, jitter=0)(_succeedOnSecondAttempt)
        func()

    def testRetryFailWithoutCatching(self):
        self.assertRaises(Exception, retry, _alwaysFail, sleeptime=0, jitter=0,
                          exceptions=())

    def testRetriableFailWithoutCatching(self):
        func = retriable(sleeptime=0)(_alwaysFail)
        self.assertRaises(Exception, func, retry_exceptions=())

    def testRetryFailEnsureRaisesLastException(self):
        self.assertRaises(Exception, retry, _alwaysFail, sleeptime=0, jitter=0)

    def testRetriableFailEnsureRaisesLastException(self):
        func = retriable(sleeptime=0)(_alwaysFail)
        self.assertRaises(Exception, func)

    def testRetrySelectiveExceptionSucceed(self):
        retry(_raiseCustomException, attempts=2, sleeptime=0, jitter=0,
              retry_exceptions=(NewError,))

    def testRetriableSelectiveExceptionSucceed(self):
        func = retriable(attempts=2, sleeptime=0, jitter=0,
                         retry_exceptions=(NewError,))(_raiseCustomException)
        func()

    def testRetrySelectiveExceptionFail(self):
        self.assertRaises(NewError, retry, _raiseCustomException, attempts=2,
                          sleeptime=0, jitter=0, retry_exceptions=(OtherError,))

    def testRetriableSelectiveExceptionFail(self):
        func = retriable(attempts=2, sleeptime=0, jitter=0,
                         retry_exceptions=(OtherError,))(_raiseCustomException)
        self.assertRaises(NewError, func)

    def testRetryWithSleep(self):
        retry(_succeedOnSecondAttempt, attempts=2, sleeptime=1)

    def testRetriableWithSleep(self):
        func = retriable(attempts=2, sleeptime=1)(_succeedOnSecondAttempt)
        func()

    def testRetryOnlyRunOnce(self):
        """Tests that retry() doesn't call the action again after success"""
        global ATTEMPT_N
        retry(_alwaysPass, attempts=3, sleeptime=0, jitter=0)
        # ATTEMPT_N gets increased regardless of pass/fail
        self.assertEquals(2, ATTEMPT_N)

    def testRetriableOnlyRunOnce(self):
        global ATTEMPT_N
        func = retriable(attempts=3, sleeptime=0, jitter=0)(_alwaysPass)
        func()
        # ATTEMPT_N gets increased regardless of pass/fail
        self.assertEquals(2, ATTEMPT_N)

    def testRetryReturns(self):
        ret = retry(_alwaysPass, sleeptime=0, jitter=0)
        self.assertEquals(ret, True)

    def testRetriableReturns(self):
        func = retriable(sleeptime=0, jitter=0)(_alwaysPass)
        ret = func()
        self.assertEquals(ret, True)

    def testRetryCleanupIsCalled(self):
        cleanup = mock.Mock()
        retry(_succeedOnSecondAttempt, cleanup=cleanup, sleeptime=0, jitter=0)
        self.assertEquals(cleanup.call_count, 1)

    def testRetriableCleanupIsCalled(self):
        cleanup = mock.Mock()
        func = retriable(cleanup=cleanup, sleeptime=0, jitter=0)(_succeedOnSecondAttempt)
        func()
        self.assertEquals(cleanup.call_count, 1)

    def testRetryArgsPassed(self):
        args = (1, 'two', 3)
        kwargs = dict(foo='a', bar=7)
        ret = retry(_mirrorArgs, args=args, kwargs=kwargs.copy(), sleeptime=0, jitter=0)
        self.assertEqual(ret[0], args)
        self.assertEqual(ret[1], kwargs)

    def testRetriableArgsPassed(self):
        args = (1, 'two', 3)
        kwargs = dict(foo='a', bar=7)
        func = retriable(sleeptime=0, jitter=0)(_mirrorArgs)
        ret = func(*args, **kwargs)
        self.assertEqual(ret[0], args)
        self.assertEqual(ret[1], kwargs)

    def test_retrying_id(self):
        """Make sure that the context manager doesn't change the original
        callable"""
        def wrapped():
            pass
        before = id(wrapped)
        with retrying(wrapped) as w:
            within = id(w)
        after = id(wrapped)
        self.assertEqual(before, after)
        self.assertNotEqual(before, within)

    def test_retrying_call_retry(self):
        """Make sure to distribute retry and function args/kwargs properly"""
        def wrapped(*args, **kwargs):
            pass
        with mock.patch("redo.retry") as mocked_retry:
            with retrying(wrapped, 1, x="y") as w:
                w("a", b=1, c="a")
            mocked_retry.assert_called_once_with(
                wrapped, 1, x='y', args=('a',), kwargs={'c': 'a', 'b': 1})

    def test_retrier(self):
        """Make sure retrier behaves properly"""
        n = 0
        for _ in retrier(attempts=5, sleeptime=0, jitter=0):
            n += 1
        self.assertEquals(n, 5)

    def test_retrier_sleep(self):
        """Make sure retrier sleep is behaving"""
        with mock.patch("time.sleep") as sleep:
            # Test that normal sleep scaling works
            for _ in retrier(attempts=5, sleeptime=10, max_sleeptime=300, sleepscale=2, jitter=0):
                pass
            expected = [mock.call(x) for x in (10, 20, 40, 80)]
            self.assertEquals(sleep.call_args_list, expected)

    def test_retrier_sleep_no_jitter(self):
        """Make sure retrier sleep is behaving"""
        with mock.patch("time.sleep") as sleep:
            # Test that normal sleep scaling works without a jitter
            for _ in retrier(attempts=5, sleeptime=10, max_sleeptime=300, sleepscale=2, jitter=None):
                pass
            expected = [mock.call(x) for x in (10, 20, 40, 80)]
            self.assertEquals(sleep.call_args_list, expected)

    def test_retrier_maxsleep(self):
        with mock.patch("time.sleep") as sleep:
            # Test that max sleep time works
            for _ in retrier(attempts=5, sleeptime=10, max_sleeptime=30, sleepscale=2, jitter=0):
                pass
            expected = [mock.call(x) for x in (10, 20, 30, 30)]
            self.assertEquals(sleep.call_args_list, expected)

    def test_jitter_bounds(self):
        self.assertRaises(Exception, retrier(sleeptime=1, jitter=2))

    def test_retrier_jitter(self):
        with mock.patch("time.sleep") as sleep:
            # Test that jitter works
            with mock.patch("random.randint") as randint:
                randint.return_value = 3
                for _ in retrier(attempts=5, sleeptime=10, max_sleeptime=300,
                                 sleepscale=2, jitter=3):
                    randint.return_value *= -1
                expected = [mock.call(x) for x in (7, 23, 37, 83)]
                self.assertEquals(sleep.call_args_list, expected)
                self.assertEquals(randint.call_args, mock.call(-48, 48))
