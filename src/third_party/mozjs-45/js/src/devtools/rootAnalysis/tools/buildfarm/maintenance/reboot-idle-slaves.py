#!/builds/slaverebooter/bin/python2.7
"""Idle Slave Rebooter

Usage: reboot-idle-slaves.py [-h] [--dryrun] (<config_file>)

-h --help         Show this help message.
--dryrun          Don't do any reboots, just print what would've been done.
"""

from datetime import datetime
from furl import furl
from os import path
import requests
import site
from threading import Thread
import time
import Queue
import pprint

import logging
from logging.handlers import RotatingFileHandler
log = logging.getLogger(__name__)
handler = RotatingFileHandler("reboot-idle-slaves.log",
                              maxBytes=52428800,
                              backupCount=50)
formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
handler.setFormatter(formatter)
log.addHandler(handler)

site.addsitedir(path.join(path.dirname(path.realpath(__file__)), "../../lib/python"))

from util.retry import retry

MAX_WORKERS = 16
IDLE_THRESHOLD = 5*60*60
PENDING, RUNNING, SUCCESS, FAILURE = range(4)
WORKER_WAIT_THRESHOLD = 30*60
STARTING_WAIT_INCREMENT = 1
MAX_WAIT_INTERVAL = 2*60

SLAVE_QUEUE = Queue.Queue()

def get_production_slaves(slaveapi):
    url = furl(slaveapi)
    url.path.add("slaves")
    url.args["environment"] = "prod"
    url.args["enabled"] = 1
    r = retry(requests.get, args=(str(url),))
    return r.json()["slaves"]


def get_slave(slaveapi, slave):
    url = furl(slaveapi)
    url.path.add("slaves").add(slave)
    return retry(requests.get, args=(str(url),)).json()


def get_formatted_time(dt):
    return dt.strftime("%A, %B %d, %H:%M")


def get_latest_timestamp_from_result(result):
    if not result:
        return 0
    times = []
    for key in ("finish_timestamp", "request_timestamp", "start_timestamp"):
        if result[key]:
            times.append(result[key])
    return max(times)


def get_latest_result(results):
    res = None
    for result in results:
        if not res:
            res = result
            continue
        res_ts = get_latest_timestamp_from_result(res)
        if res_ts < get_latest_timestamp_from_result(result):
            res = result
    return res


def get_recent_action(slaveapi, slave, action):
    url = furl(slaveapi)
    url.path.add("slaves").add(slave).add("actions").add(action)
    history = retry(requests.get, args=(str(url),)).json()
    results = []
    for key in history.keys():
        if not key == action:
            continue
        for item in history[action]:
            results.append(history[action][item])
    return get_latest_result(results)


def get_recent_graceful(slaveapi, slave):
    return get_recent_action(slaveapi, slave, "shutdown_buildslave")


def get_recent_reboot(slaveapi, slave):
    return get_recent_action(slaveapi, slave, "reboot")


def get_recent_job(slaveapi, slave):
    info = get_slave(slaveapi, slave)
    if not info["recent_jobs"]:
        return None

    return info["recent_jobs"][0]["endtime"]


def do_graceful(slaveapi, slave):
    # We need to set a graceful shutdown for the slave on the off chance that
    # it picks up a job before us making the decision to reboot it, and the
    # reboot actually happening. In most cases this will happen nearly
    # instantly.
    log.debug("%s - Setting graceful shutdown", slave)
    url = furl(slaveapi)
    url.path.add("slaves").add(slave).add("actions").add("shutdown_buildslave")
    url.args["waittime"] = 30
    r = retry(requests.post, args=(str(url),)).json()
    url.args["requestid"] = r["requestid"]

    time.sleep(30)  # Sleep to give a graceful some leeway to complete
    log.info("%s - issued graceful, re-adding to queue", slave)
    SLAVE_QUEUE.put_nowait(slave)
    return


def do_reboot(slaveapi, slave):
    url = furl(slaveapi)
    url.path.add("slaves").add(slave).add("actions").add("reboot")
    retry(requests.post, args=(str(url),))
    # Because SlaveAPI fully escalates reboots (all the way to IT bug filing),
    # there's no reason for us to watch for it to complete.
    log.info("%s - Reboot queued", slave)
    return


def process_slave(slaveapi, dryrun=False):
    slave = None  # No slave name yet
    try:
        try:
            slave = SLAVE_QUEUE.get_nowait()
            log.debug("%s - got slave from SLAVE_QUEUE", slave)
        except Queue.Empty:
            return  # Unlikely due to our thread creation logic, but possible

        last_job_ts = get_recent_job(slaveapi, slave)

        # Ignore slaves without recent job information
        if not last_job_ts:
            log.info("%s - Skipping reboot because no job history found", slave)
            return

        last_job_dt = datetime.fromtimestamp(last_job_ts)
        # And also slaves that haven't been idle for more than the threshold
        if not (datetime.now() - last_job_dt).total_seconds() > IDLE_THRESHOLD:
            log.info("%s - Skipping reboot because last job ended recently at %s",
                     slave, get_formatted_time(last_job_dt))
            return

        recent_graceful = get_recent_graceful(slaveapi, slave)
        recent_graceful_ts = get_latest_timestamp_from_result(recent_graceful)
        recent_reboot = get_recent_reboot(slaveapi, slave)
        recent_reboot_ts = get_latest_timestamp_from_result(recent_reboot)
        # Determine the timestamp we care about for doing work
        idle_timestamp = max(last_job_ts, recent_reboot_ts)
        idle_dt = datetime.fromtimestamp(idle_timestamp)

        # If an action is in-flight, lets assume no work to do this run.
        if (recent_graceful and "state" in recent_graceful and
                recent_graceful["state"] in (PENDING, RUNNING)):
            log.info("%s - waiting on graceful shutdown, will recheck next run",
                     slave)
            return
        if (recent_reboot and "state" in recent_reboot and
                recent_reboot["state"] in (PENDING, RUNNING)):
            log.info("%s - waiting on a reboot request, assume success",
                     slave)
            return

        # No work if we recently performed an action that should recover
        if not (datetime.now() - idle_dt).total_seconds() > IDLE_THRESHOLD:
            log.info("%s - Skipping reboot because we recently attempted recovery %s",
                     slave, get_formatted_time(idle_dt))
            return

        if recent_graceful_ts <= idle_timestamp:
            # we've passed IDLE_THRESHOLD since last reboot/job
            # ---> initiate graceful
            if dryrun:
                log.info("%s - Last job ended at %s, would've gracefulled",
                         slave, get_formatted_time(last_job_dt))
                return
            return do_graceful(slaveapi, slave)
        else:  # (recent_graceful_ts > idle_timestamp)
            # has recently graceful'd but needs a reboot
            # ---> initiate reboot
            if dryrun:
                log.info("%s - Last job ended at %s, would've rebooted",
                         slave, get_formatted_time(last_job_dt))
                return
            if recent_graceful["state"] in (FAILURE,):
                log.info("%s - Graceful shutdown failed, rebooting anyway", slave)
            else:
                log.info("%s - Graceful shutdown passed, rebooting", slave)
            return do_reboot(slaveapi, slave)
    except:
        log.exception("%s - Caught exception while processing", slave)


if __name__ == "__main__":
    from ConfigParser import RawConfigParser
    from docopt import docopt, DocoptExit
    args = docopt(__doc__)

    dryrun = args["--dryrun"]
    config_file = args["<config_file>"]

    cfg = RawConfigParser()
    cfg.read(config_file)

    slaveapi = cfg.get("main", "slaveapi_server")
    n_workers = cfg.getint("main", "workers")
    verbose = cfg.getboolean("main", "verbose")
    excludes = cfg.options("exclude")

    logging.basicConfig(level=logging.DEBUG, format="%(asctime)s - %(levelname)s - %(message)s")
    if verbose:
        logging.getLogger("requests").setLevel(logging.DEBUG)
        logging.getLogger("util.retry").setLevel(logging.DEBUG)
    else:
        logging.getLogger("requests").setLevel(logging.WARN)
        logging.getLogger("util.retry").setLevel(logging.WARN)

    if n_workers > MAX_WORKERS:
        raise DocoptExit("Number of workers requested (%d) exceeds maximum (%d)" % (n_workers, MAX_WORKERS))

    def is_excluded(name):
        for pattern in excludes:
            if pattern in name:
                return True
        return False

    workers = {}

    try:
        log.info("Populating List of Slaves to Check...")
        for slave in get_production_slaves(slaveapi):
            name = slave["name"]
            if is_excluded(name):
                log.debug("%s - Excluding because it matches an excluded pattern.", name)
                continue
            log.debug("%s - Adding item to queue", name)
            SLAVE_QUEUE.put_nowait(name)

        # Run while there is any workers or any queued work
        elapsed = 0
        worker_wait_increment = STARTING_WAIT_INCREMENT
        while len(workers) or SLAVE_QUEUE.qsize():
            # Block until a worker frees
            while len(workers) and len(workers) >= min(n_workers, SLAVE_QUEUE.qsize()) and \
                  elapsed < WORKER_WAIT_THRESHOLD:
                log.debug("Waiting for a free worker...")
                if elapsed + worker_wait_increment >= WORKER_WAIT_THRESHOLD:
                    worker_wait_increment = WORKER_WAIT_THRESHOLD - elapsed
                log.debug("Sleeping %d seconds..." % worker_wait_increment)
                time.sleep(worker_wait_increment)
                elapsed += worker_wait_increment
                worker_wait_increment = worker_wait_increment * 2
                if worker_wait_increment > MAX_WAIT_INTERVAL:
                    worker_wait_increment = MAX_WAIT_INTERVAL

                for wname, w in workers.items():
                    log.debug("worker: %s" % wname)
                    if not w.is_alive():
                        del workers[wname]
            if elapsed >= WORKER_WAIT_THRESHOLD:
                log.warning("Gave up waiting for a free worker after %d seconds" % elapsed)
                break

            # Start a new worker if there is more work
            if len(workers) < n_workers and SLAVE_QUEUE.qsize():
                log.debug("Starting a new worker...")
                t = Thread(target=process_slave, args=(slaveapi, dryrun))
                t.start()
                workers[t.ident] = t
                worker_wait_increment = STARTING_WAIT_INCREMENT
                log.debug("Started worker %s", t.ident)

        # Wait for any remaining workers to finish before exiting.
        for w in workers.values():
            while w.is_alive():
                log.debug("Found a running worker. Attempting to join...")
                worker_wait_increment = STARTING_WAIT_INCREMENT
                w.join(1)

        if SLAVE_QUEUE.qsize():
            # This should not be possible, but report anyway.
            log.info("%s items remained in queue at exit",
                     SLAVE_QUEUE.qsize())
    except KeyboardInterrupt:
        raise

    log.info("All done. Exiting...")
