#!/usr/bin/python
"""%prog [options] master_dir

Reconfigures the buildbot master in master_dir, waiting for it to finish.

Any errors generated will be printed to stderr.
"""
import os
import sys
import time
import signal
import subprocess
import urllib


def graceful_stop(port):
    url = "http://localhost:%s/shutdown" % port
    data = urllib.urlencode(dict(submit='Clean Shutdown'))
    try:
        urllib.urlopen(url, data)
    except IOError:
        pass


class Watcher:
    def __init__(self, fname):
        self.fname = fname

        self.started = False

        if os.path.exists(fname):
            os.utime(fname, None)
        else:
            try:
                open(fname, 'w').close()
            except:
                print "Could not create %s." % fname
                sys.exit(1)
        self.fp = open(fname)
        self.fp.seek(0, 2)  # SEEK_END
        self.current_inode = os.fstat(self.fp.fileno()).st_ino


class StopWatcher(Watcher):
    def watch_logfile(self):
        in_error = False
        while True:
            time.sleep(0.5)
            if not self.fp:
                try:
                    self.fp = open(self.fname)
                    self.current_inode = os.fstat(self.fp.fileno()).st_ino
                except IOError:
                    continue

            for line in self.fp.readlines():
                if not self.started:
                    if "Received SIGTERM" in line:
                        self.started = True
                    elif "Initiating clean shutdown" in line:
                        self.started = True
                    else:
                        # Don't do anything else until we've actually started
                        # the shutdown
                        continue

                # Print out "Waiting for n build(s) to finish"
                if "Waiting for" in line:
                    print >> sys.stderr, line.rstrip()

                # Print out everything until the next blank line
                if in_error:
                    if not line.strip():
                        in_error = False
                        continue
                    print >> sys.stderr, line.rstrip()

                if "Server Shut Down" in line:
                    return

                if "Unhandled Error" in line:
                    in_error = True
                    print >> sys.stderr, line.rstrip()

            inode = os.stat(self.fname).st_ino
            if inode != self.current_inode:
                # Log has been replaced.  Try and open it again later
                self.fp = None
                self.current_inode = None


class ReconfigWatcher(Watcher):
    def watch_logfile(self):
        """Watches the logfile `fname` for reconfigure messages and errors.

        If "configuration update complete" is seen, this function returns.

        If any unhandled errors are seen, they are printed out to stderr.
        """
        in_error = False
        while True:
            time.sleep(0.5)
            if not self.fp:
                try:
                    self.fp = open(self.fname)
                    self.current_inode = os.fstat(self.fp.fileno()).st_ino
                except IOError:
                    continue

            for line in self.fp.readlines():
                if not self.started:
                    if "loading configuration from" in line:
                        self.started = True
                    else:
                        # Don't do anything else until we've actually started
                        # the reconfig
                        continue

                # Print out everything until the next blank line
                if in_error:
                    if not line.strip():
                        in_error = False
                        continue
                    print >> sys.stderr, line.rstrip()

                if "configuration update complete" in line:
                    return

                if "configuration update failed" in line:
                    raise Exception("reconfig failed")

                if "Unhandled Error" in line:
                    in_error = True
                    print >> sys.stderr, line.rstrip()

            inode = os.stat(self.fname).st_ino
            if inode != self.current_inode:
                # Log has been replaced.  Try and open it again later
                self.fp = None
                self.current_inode = None

if __name__ == '__main__':
    args = sys.argv[1:]
    if len(args) <= 1 or len(args) >= 4:
        print "Usage: buildbot-reconfig.py restart|reconfig|stop master_dir [http_port]"
        sys.exit(1)

    action = args[0]
    if action not in ("restart", "reconfig", "stop", "start", "graceful_stop", "graceful_restart"):
        print "Unknown action", action
        sys.exit(1)

    master_dir = args[1]
    twistd_log = os.path.join(master_dir, "twistd.log")
    pidfile = os.path.join(master_dir, "twistd.pid")

    pid = None
    if os.path.exists(pidfile):
        pid = int(open(pidfile).read())
    elif action == "reconfig":
        print "Master doesn't appear to be running"
        sys.exit(1)

    if action == "reconfig":
        w = ReconfigWatcher(twistd_log)
        os.kill(pid, signal.SIGHUP)
        w.watch_logfile()

    elif action == "restart":
        if pid:
            w = StopWatcher(twistd_log)
            os.kill(pid, signal.SIGTERM)
            w.watch_logfile()

        w = ReconfigWatcher(twistd_log)
        null = open(os.devnull, 'w')
        p = subprocess.Popen(
            ['make', 'start', master_dir], stdout=null, stderr=null)
        w.watch_logfile()

    elif action == "stop":
        if pid:
            w = StopWatcher(twistd_log)
            os.kill(pid, signal.SIGTERM)
            w.watch_logfile()
        else:
            print "master already stopped"

    elif action == "graceful_stop":
        if pid:
            w = StopWatcher(twistd_log)
            graceful_stop(args[2])
            w.watch_logfile()
        else:
            print "master already stopped"

    elif action == "graceful_restart":
        if pid:
            w = StopWatcher(twistd_log)
            graceful_stop(args[2])
            w.watch_logfile()

        w = ReconfigWatcher(twistd_log)
        null = open(os.devnull, 'w')
        p = subprocess.Popen(
            ['make', 'start', master_dir], stdout=null, stderr=null)
        w.watch_logfile()

    elif action == "start":
        if pid:
            print "Master is already running"
        else:
            w = ReconfigWatcher(twistd_log)
            null = open(os.devnull, 'w')
            p = subprocess.Popen(
                ['make', 'start', master_dir], stdout=null, stderr=null)
            w.watch_logfile()
