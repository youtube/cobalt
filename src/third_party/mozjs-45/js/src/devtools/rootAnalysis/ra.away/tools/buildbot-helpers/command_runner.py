#!/usr/bin/env python
"""
Runs commands from a queue!
"""
import subprocess
import os
import signal
import time
from mozilla_buildtools.queuedir import QueueDir
from buildbot.util import json
import logging
log = logging.getLogger(__name__)


class Job(object):
    def __init__(self, cmd, item_id, log_fp):
        self.cmd = cmd
        self.log = log_fp
        self.item_id = item_id
        self.started = None
        self.last_signal_time = 0
        self.last_signal = None

        self.proc = None

    def start(self):
        devnull = open(os.devnull, 'r')
        self.log.write("Running %s\n" % self.cmd)
        self.log.flush()
        self.proc = subprocess.Popen(self.cmd, close_fds=True, stdin=devnull,
                                     stdout=self.log, stderr=self.log)
        self.started = time.time()

    def check(self):
        now = time.time()
        if now - self.started > self.max_time:
            # Kill stuff off
            if now - self.last_signal_time > 60:
                s = {None: signal.SIGINT, signal.SIGINT:
                     signal.SIGTERM}.get(self.last_signal, signal.SIGKILL)
                log.info("Killing %i with %i", self.proc.pid, s)
                try:
                    self.log.write("Killing with %s\n" % s)
                    os.kill(self.proc.pid, s)
                    self.last_signal = s
                    self.last_signal_time = now
                except OSError:
                    # Ok, process must have exited already
                    log.exception("Failed to kill")
                    pass

        result = self.proc.poll()
        if result is not None:
            self.log.write("\nResult: %s, Elapsed: %1.1f seconds\n" % (result, time.time() - self.started))
            self.log.close()
        return result


class CommandRunner(object):
    def __init__(self, options):
        self.queuedir = options.queuedir
        self.q = QueueDir('commands', self.queuedir)
        self.concurrency = options.concurrency
        self.retry_time = options.retry_time
        self.max_retries = options.max_retries
        self.max_time = options.max_time

        self.active = []

        # List of (signal_time, level, proc)
        self.to_kill = []

    def run(self, job):
        """
        Runs the given job
        """
        log.info("Running %s", job.cmd)
        try:
            job.start()
            self.active.append(job)
        except OSError:
            job.log.write("\nFailed with OSError; requeuing in %i seconds\n" %
                          self.retry_time)
            # Wait to requeue it
            # If we die, then it's still in cur, and will be moved back into
            # 'new' eventually
            self.q.requeue(job.item_id, self.retry_time, self.max_retries)

    def monitor(self):
        """
        Monitor running jobs
        """
        for job in self.active[:]:
            self.q.touch(job.item_id)
            result = job.check()

            if result is not None:
                self.active.remove(job)
                if result == 0:
                    self.q.remove(job.item_id)
                else:
                    log.warn("%s failed; requeuing", job.item_id)
                    # Requeue it!
                    self.q.requeue(
                        job.item_id, self.retry_time, self.max_retries)

    def loop(self):
        """
        Main processing loop. Read new items from the queue and run them!
        """
        while True:
            self.monitor()
            if len(self.active) >= self.concurrency:
                # Wait!
                time.sleep(1)
                continue

            while len(self.active) < self.concurrency:
                item = self.q.pop()
                if not item:
                    # Don't wait for very long, since we have to check up on
                    # our children
                    if self.active:
                        self.q.wait(1)
                    else:
                        self.q.wait()
                    break

                item_id, fp = item
                try:
                    command = json.load(fp)
                    job = Job(command, item_id, self.q.getlog(item_id))
                    job.max_time = self.max_time
                    self.run(job)
                except ValueError:
                    # Couldn't parse it as json
                    # There's no hope!
                    self.q.log(item_id, "Couldn't load json; murdering")
                    self.q.murder(item_id)
                finally:
                    fp.close()


def main():
    from optparse import OptionParser
    import logging.handlers
    parser = OptionParser()
    parser.set_defaults(
        concurrency=1,
        max_retries=1,
        retry_time=0,
        verbosity=0,
        logfile=None,
        max_time=60,
    )
    parser.add_option("-q", "--queuedir", dest="queuedir")
    parser.add_option("-j", "--jobs", dest="concurrency", type="int",
                      help="number of commands to run at once")
    parser.add_option("-r", "--max_retries", dest="max_retries",
                      type="int", help="number of times to retry commands")
    parser.add_option("-t", "--retry_time", dest="retry_time",
                      type="int", help="seconds to wait between retries")
    parser.add_option("-v", "--verbose", dest="verbosity",
                      action="count", help="increase verbosity")
    parser.add_option(
        "-l", "--logfile", dest="logfile", help="where to send logs")
    parser.add_option("-m", "--max_time", dest="max_time", type="int",
                      help="maximum time for a command to run")

    options, args = parser.parse_args()

    # Set up logging
    if options.verbosity == 0:
        log_level = logging.WARNING
    elif options.verbosity == 1:
        log_level = logging.INFO
    else:
        log_level = logging.DEBUG

    if not options.logfile:
        logging.basicConfig(
            level=log_level, format="%(asctime)s - %(message)s")
    else:
        logger = logging.getLogger()
        logger.setLevel(log_level)
        handler = logging.handlers.RotatingFileHandler(
            options.logfile, maxBytes=1024 ** 2, backupCount=5)
        formatter = logging.Formatter("%(asctime)s - %(message)s")
        handler.setFormatter(formatter)
        logger.addHandler(handler)

    if not options.queuedir:
        parser.error("-q/--queuedir is required")

    runner = CommandRunner(options)
    runner.loop()

if __name__ == '__main__':
    main()
