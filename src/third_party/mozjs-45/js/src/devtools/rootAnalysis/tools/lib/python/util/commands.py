"""Functions for running commands"""
import subprocess
import os
import time
import platform
import logging
log = logging.getLogger(__name__)

try:
    import win32file
    import win32api
    PYWIN32 = True
except ImportError:
    PYWIN32 = False


# timeout message, used in TRANSIENT_HG_ERRORS and in tests.
TERMINATED_PROCESS_MSG = "timeout, process terminated"


def log_cmd(cmd, **kwargs):
    # cwd is special in that we always want it printed, even if it's not
    # explicitly chosen
    kwargs = kwargs.copy()
    if 'cwd' not in kwargs:
        kwargs['cwd'] = os.getcwd()
    log.info("command: START")
    log.info("command: %s" % subprocess.list2cmdline(cmd))
    for key, value in kwargs.iteritems():
        log.info("command: %s: %s", key, str(value))


def merge_env(env):
    new_env = os.environ.copy()
    new_env.update(env)
    return new_env


def run_cmd(cmd, **kwargs):
    """Run cmd (a list of arguments).  Raise subprocess.CalledProcessError if
    the command exits with non-zero.  If the command returns successfully,
    return 0."""
    log_cmd(cmd, **kwargs)
    # We update this after logging because we don't want all of the inherited
    # env vars muddling up the output
    if 'env' in kwargs:
        kwargs['env'] = merge_env(kwargs['env'])
    try:
        t = time.time()
        log.info("command: output:")
        return subprocess.check_call(cmd, **kwargs)
    except subprocess.CalledProcessError:
        log.info('command: ERROR', exc_info=True)
        raise
    finally:
        elapsed = time.time() - t
        log.info("command: END (%.2fs elapsed)\n", elapsed)


def run_quiet_cmd(cmd, **kwargs):
    devnull = open(os.devnull, 'w')
    return subprocess.check_call(cmd, stdout=devnull, stderr=devnull, **kwargs)


def run_remote_cmd(cmd, server, username=None, sshKey=None, ssh='ssh',
                   **kwargs):
    cmd_prefix = [ssh]
    if username:
        cmd_prefix.extend(['-l', username])
    if sshKey:
        cmd_prefix.extend(['-i', os.path.expanduser(sshKey)])
    cmd_prefix.append(server)
    if isinstance(cmd, basestring):
        cmd = [cmd]
    return run_cmd(cmd_prefix + cmd, **kwargs)


def run_cmd_periodic_poll(cmd, warning_interval=300, poll_interval=0.25,
                          warning_callback=None, timeout=86400, **kwargs):
    """Run cmd (a list of arguments) in a subprocess and check its completion
    periodically.  Raise subprocess.CalledProcessError if the command exits
    with non-zero.  If the command returns successfully, return 0.
    warning_callback function will be called with the following arguments if the
    command's execution takes longer then warning_interval:
        start_time, elapsed, proc
    cmd will be terminated if it takes longer than timeout. The default value for
    timeout is deliberately set to a ridiculous large value: 1 day - so that
    the command will be terminated by buildbot not by this function.
    If you need to stop a process when it takes longer than expected, set a decent
    timeout value when calling this function.
    """
    log_cmd(cmd, **kwargs)
    # We update this after logging because we don't want all of the inherited
    # env vars muddling up the output
    if 'env' in kwargs:
        kwargs['env'] = merge_env(kwargs['env'])
    log.info("command: output:")
    proc = subprocess.Popen(cmd, **kwargs)
    start_time = time.time()
    last_check = start_time

    while True:
        rc = proc.poll()
        now = time.time()
        elapsed = now - start_time

        if rc is not None:
            log.debug("Process returned %s", rc)
            if rc == 0:
                elapsed = time.time() - start_time
                log.info("command: END (%.2fs elapsed)\n", elapsed)
                return 0
            else:
                raise subprocess.CalledProcessError(rc, cmd)

        if now - last_check > warning_interval:
            # it's time to raise a warning message
            # reset last_check to avoid spamming callback
            last_check = now
            if warning_callback:
                log.debug("Calling warning_callback function: %s(%s)" %
                          (warning_callback, start_time))
                try:
                    warning_callback(start_time, elapsed, proc)
                except Exception:
                    log.error("Callback raised an exception, ignoring...",
                              exc_info=True)
            else:
                log.warning("Command execution is taking longer than"
                            "warning_interval (%d)"
                            ", executing warning_callback"
                            "Started at: %s, elapsed: %.2fs" % (warning_interval,
                                                                start_time,
                                                                elapsed))
        if elapsed > timeout:
            # the process is taking too long, terminate it
            log.info("process, is taking too long: %ss (timeout %s). Terminating" %
                     (now - start_time, timeout))
            proc.terminate()
            proc.wait()
            error = subprocess.CalledProcessError(proc.returncode, cmd)
            error.output = TERMINATED_PROCESS_MSG
            raise error
        # sleep for a while...
        time.sleep(poll_interval)


def get_output(cmd, include_stderr=False, dont_log=False, timeout=86400,
               poll_interval=0.25, **kwargs):
    """Run cmd (a list of arguments) in a subprocess and check its completion
    periodically and returns the output.  If include_stderr
    is set, stderr will be included in the output, otherwise it will be sent to
    the caller's stderr stream.

    Raise subprocess.CalledProcessError if the command exits
    with non-zero.  If the command returns successfully, return 0.
    warning_callback function will be called with the following arguments if the
    command's execution takes longer then warning_interval:
        start_time, elapsed, proc

    cmd will be terminated if it takes longer than timeout. The default value for
    timeout is deliberately set to a ridiculous large value: 1 day - so that
    the command will be terminated by buildbot not by this function.
    If you need to stop a process when it takes longer than expected, set a decent
    timeout value when calling this function.

    Please note that proc.communicate() can be an expensive operation when the output
    is large, some samples:
    output size | time spent in proc.communicate():
       [bytes]  |  [s]
        409600  |  0.027
       4096000  |  0.27
      40960000  |  2.7
     409600000  | 27.5
    """
    if include_stderr:
        stderr = subprocess.STDOUT
    else:
        stderr = None

    log_cmd(cmd, **kwargs)
    if 'env' in kwargs:
        kwargs['env'] = merge_env(kwargs['env'])

    output = []
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=stderr, **kwargs)
    start_time = time.time()

    while True:
        rc = proc.poll()
        now = time.time()
        elapsed = now - start_time
        if rc is not None:
            # proc.stdout.read() is a blocking operation but if we are here,
            # proc has already finished.
            output = proc.stdout.read()
            log.debug("Process returned %s", rc)
            if rc == 0:
                log.info("command: END (%.2fs elapsed)\n", elapsed)
                if not dont_log:
                    log.info("command: output:")
                    log.info(output)
                return output
            else:
                error = subprocess.CalledProcessError(proc.returncode, cmd)
                error.output = output
                raise error

        if elapsed > timeout:
            log.info("process, is taking too long: %ss (timeout %ss).Terminating" %
                     (elapsed, timeout))
            proc.terminate()
            (stdoutdata, stderrdata) = proc.communicate()
            # the process is terminated but .communicate can take few seconds to
            # complete
            msg = "%s, stdout: %s" % (TERMINATED_PROCESS_MSG, stdoutdata)
            if include_stderr:
                msg = "%s, stderr: %s" % (msg, stderrdata)
            error = subprocess.CalledProcessError(proc.returncode, cmd)
            error.output = msg
            raise error
        # sleep for a while
        time.sleep(poll_interval)


def remove_path(path):
    """This is a replacement for shutil.rmtree that works better under
    windows. Thanks to Bear at the OSAF for the code.
    (Borrowed from buildbot.slave.commands)"""
    log.debug("Removing %s", path)

    if _is_windows():
        log.info("Using _rmtree_windows ...")
        _rmtree_windows(path)
        return

    if not os.path.exists(path):
        # This handles broken links
        if os.path.islink(path):
            os.remove(path)
        return

    if os.path.islink(path):
        os.remove(path)
        return

    # Verify the directory is read/write/execute for the current user
    os.chmod(path, 0700)

    for name in os.listdir(path):
        full_name = os.path.join(path, name)
        # on Windows, if we don't have write permission we can't remove
        # the file/directory either, so turn that on
        if os.name == 'nt':
            if not os.access(full_name, os.W_OK):
                # I think this is now redundant, but I don't have an NT
                # machine to test on, so I'm going to leave it in place
                # -warner
                os.chmod(full_name, 0600)

        if os.path.isdir(full_name):
            remove_path(full_name)
        else:
            # Don't try to chmod links
            if not os.path.islink(full_name):
                os.chmod(full_name, 0700)
            os.remove(full_name)
    os.rmdir(path)


# _is_windows and _rmtree_windows taken
# from mozharness

def _is_windows():
    system = platform.system()
    if system in ("Windows", "Microsoft"):
        return True
    if system.startswith("CYGWIN"):
        return True
    if os.name == 'nt':
        return True


def _rmtree_windows(path):
    """ Windows-specific rmtree that handles path lengths longer than MAX_PATH.
        Ported from clobberer.py.
    """
    assert _is_windows()
    path = os.path.realpath(path)
    full_path = '\\\\?\\' + path
    if not os.path.exists(full_path):
        return
    if not PYWIN32:
        if not os.path.isdir(path):
            return run_cmd('del /F /Q "%s"' % path)
        else:
            return run_cmd('rmdir /S /Q "%s"' % path)
    # Make sure directory is writable
    win32file.SetFileAttributesW('\\\\?\\' + path, win32file.FILE_ATTRIBUTE_NORMAL)
    # Since we call rmtree() with a file, sometimes
    if not os.path.isdir('\\\\?\\' + path):
        return win32file.DeleteFile('\\\\?\\' + path)

    for ffrec in win32api.FindFiles('\\\\?\\' + path + '\\*.*'):
        file_attr = ffrec[0]
        name = ffrec[8]
        if name == '.' or name == '..':
            continue
        full_name = os.path.join(path, name)

        if file_attr & win32file.FILE_ATTRIBUTE_DIRECTORY:
            _rmtree_windows(full_name)
        else:
            win32file.SetFileAttributesW('\\\\?\\' + full_name, win32file.FILE_ATTRIBUTE_NORMAL)
            win32file.DeleteFile('\\\\?\\' + full_name)
    win32file.RemoveDirectory('\\\\?\\' + path)
