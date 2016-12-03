"""
Implement an on-disk queue for stuff
"""
import os
import tempfile
import time
import logging
log = logging.getLogger(__name__)

try:
    import pyinotify
    assert pyinotify

    class _MovedHandler(pyinotify.ProcessEvent):
        def process_IN_MOVED_TO(self, event):
            pass
except ImportError:
    pyinotify = None


class QueueDir(object):
    # How long before things are considered to be "old"
    # Also how long between cleanup jobs
    cleanup_time = 300  # 5 minutes

    # Should the producer do cleanup?
    producer_cleanup = True

    # Mapping of names to QueueDir instances
    _objects = {}

    def __init__(self, name, queue_dir):
        self._objects[name] = self
        self.queue_dir = queue_dir

        self.pid = os.getpid()
        self.started = int(time.time())
        self.count = 0
        self.last_cleanup = 0
        # List of time, item_id for items to move from cur back into new
        self.to_requeue = []

        self.tmp_dir = os.path.join(self.queue_dir, 'tmp')
        self.new_dir = os.path.join(self.queue_dir, 'new')
        self.cur_dir = os.path.join(self.queue_dir, 'cur')
        self.log_dir = os.path.join(self.queue_dir, 'logs')
        self.dead_dir = os.path.join(self.queue_dir, 'dead')

        self.setup()

    @classmethod
    def getQueue(cls, name):
        return cls._objects[name]

    def setup(self):
        for d in (self.tmp_dir, self.new_dir, self.cur_dir, self.log_dir, self.dead_dir):
            # Create our directories a bit at a time so we can make sure the
            # modes are created properly
            parts = d.split("/")
            path = "/"
            for p in parts:
                path = os.path.join(path, p)
                if not os.path.exists(path):
                    os.mkdir(path)
                    os.chmod(path, 0755)

        self.cleanup()

    def cleanup(self):
        """
        Removes old items from tmp
        Removes old logs from log_dir
        Moves old items from cur into new

        'old' is defined by the cleanup_time property
        """
        now = time.time()
        if now - self.last_cleanup < self.cleanup_time:
            return
        self.last_cleanup = now
        dirs = [self.tmp_dir, self.log_dir]
        for d in dirs:
            for f in os.listdir(d):
                fn = os.path.join(d, f)
                try:
                    if os.path.getmtime(fn) < now - self.cleanup_time:
                        os.unlink(fn)
                except OSError:
                    pass

        for f in os.listdir(self.cur_dir):
            fn = os.path.join(self.cur_dir, f)
            try:
                if os.path.getmtime(fn) < now - self.cleanup_time:
                    self.requeue(f)
            except OSError:
                pass

    ###
    # For producers
    ###
    def add(self, data):
        """
        Adds a new item to the queue.
        """
        # write data to tmp
        fd, tmp_name = tempfile.mkstemp(prefix="%i-%i-%i" % (self.started,
                                                             self.count, self.pid), dir=self.tmp_dir)
        os.write(fd, data)
        os.close(fd)

        dst_name = os.path.join(self.new_dir, os.path.basename(tmp_name))
        os.rename(tmp_name, dst_name)
        self.count += 1

        if self.producer_cleanup:
            self.cleanup()

    ###
    # For consumers
    ###
    def pop(self, sorted=True):
        """
        Moves an item from new into cur
        Returns item_id, file handle
        Returns None if queue is empty
        If sorted is True, then the earliest item is returned
        """
        self._check_to_requeue()
        self.cleanup()
        items = os.listdir(self.new_dir)
        if sorted:
            items.sort(
                key=lambda f: os.path.getmtime(os.path.join(self.new_dir, f)))
        for item in items:
            try:
                dst_name = os.path.join(self.cur_dir, item)
                os.rename(os.path.join(self.new_dir, item), dst_name)
                os.utime(dst_name, None)
                return item, open(dst_name, 'rb')
            except OSError:
                pass
        return None

    def peek(self):
        """
        Returns True if there are new items in the queue
        """
        items = os.listdir(self.new_dir)
        return len(items) > 0

    def touch(self, item_id):
        """
        Indicate that we're still working on this item
        """
        fn = os.path.join(self.cur_dir, item_id)
        try:
            os.utime(fn, None)
        except OSError:
            # Somebody else moved this; that's probably ok
            pass

    def getcount(self, item_id):
        """
        Returns how many times this item has been run
        """
        try:
            return int(item_id.split(".")[1])
        except:
            return 0

    def getlogname(self, item_id):
        if "." in item_id:
            item_id = item_id.split(".")[0]
        fn = os.path.join(self.log_dir, "%s.log" % item_id)
        return fn

    def getlog(self, item_id):
        """
        Creates and returns a file object for a log file for this item
        """
        return open(self.getlogname(item_id), "a+")

    def log(self, item_id, msg):
        self.getlog(item_id).write(msg + "\n")

    def remove(self, item_id):
        """
        Removes item_id from cur
        """
        os.unlink(os.path.join(self.cur_dir, item_id))

    def _check_to_requeue(self):
        if not self.to_requeue:
            return
        now = time.time()
        for t, item_id in self.to_requeue[:]:
            if now > t:
                self.requeue(item_id)
                self.to_requeue.remove((t, item_id))
            else:
                self.touch(item_id)

    def requeue(self, item_id, delay=None, max_retries=None):
        """
        Moves item_id from cur back into new, incrementing the counter at the
        end.

        If delay is set, it is a number of seconds to wait before moving the
        item back into new. It will remain in cur until the appropriate time
        has expired.
        The state for this is kept in the QueueDir instance, so if the instance
        goes away, the item won't be requeued on schedule. It will eventually
        be moved out of cur when the cleanup time expires however.
        You must be call pop() at some point in the future for requeued items
        to be processed.
        """
        try:
            bits = item_id.split(".")
            core_item_id = ".".join(bits[:-1])
            count = int(bits[-1]) + 1
        except:
            core_item_id = item_id
            count = 1

        if max_retries is not None and count > max_retries:
            log.info("Maximum retry count exceeded; murdering %s", item_id)
            self.murder(item_id)
            return

        if delay:
            self.to_requeue.append((time.time() + delay, item_id))
            self.to_requeue.sort()
            return

        dst_name = os.path.join(self.new_dir, "%s.%i" % (core_item_id, count))
        try:
            os.rename(os.path.join(self.cur_dir, item_id), dst_name)
            os.utime(dst_name, None)
        except OSError:
            # Somebody else got to it first
            pass

    def murder(self, item_id):
        """
        Moves item_id and log from cur into dead for future inspection
        """
        dst_name = os.path.join(self.dead_dir, item_id)
        os.rename(os.path.join(self.cur_dir, item_id), dst_name)
        if os.path.exists(self.getlogname(item_id)):
            dst_name = os.path.join(self.dead_dir, "%s.log" % item_id)
            os.rename(self.getlogname(item_id), dst_name)

    if pyinotify:
        def wait(self, timeout=None):
            """
            Waits for new items to arrive in new.
            timeout is in seconds, and is the maximum amount of time to wait. we
            might return before that.
            """
            # Check if we have any items to requeue
            if self.to_requeue:
                reque_time = self.to_requeue[0][0] - time.time()
                # Need to do something right now!
                if reque_time < 0:
                    return
                if timeout:
                    timeout = min(reque_time, timeout)
                else:
                    timeout = reque_time

            if timeout:
                timeout *= 1000

            log.debug("Sleeping for %s", timeout)

            wm = pyinotify.WatchManager()
            try:
                wm.add_watch(self.new_dir, pyinotify.IN_MOVED_TO)

                notifier = pyinotify.Notifier(wm, _MovedHandler())
                notifier.check_events(timeout)
                notifier.process_events()
            finally:
                wm.close()
    else:
        def wait(self, timeout=None):
            """
            Waits for new items to arrive in new.
            timeout is in seconds, and is the maximum amount of time to wait. we
            might return before that.
            """
            # Check if we have any items to requeue
            if self.to_requeue:
                reque_time = self.to_requeue[0][0] - time.time()
                # Need to do something right now!
                if reque_time < 0:
                    return
                if timeout:
                    timeout = min(reque_time, timeout)
                else:
                    timeout = reque_time

            log.debug("Sleeping for %s", timeout)

            start = time.time()
            while True:
                if self.peek():
                    return
                time.sleep(1)
                if timeout and time.time() - start > timeout:
                    return
