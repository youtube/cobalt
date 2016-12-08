import signal
import os
import time


def kill(pid):
    """Kill process pid with progressively more agressive signals."""
    siglist = [signal.SIGINT, signal.SIGTERM]
    while True:
        if siglist:
            sig = siglist.pop(0)
        else:
            sig = signal.SIGKILL
        os.kill(pid, sig)
        time.sleep(5)
        rc = os.waitpid(pid, os.WNOHANG)
        try:
            os.kill(pid, 0)
        except OSError:
            return rc[1]
