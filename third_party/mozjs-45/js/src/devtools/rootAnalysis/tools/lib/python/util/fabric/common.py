from os import path
import logging

from util.commands import run_cmd, run_cmd_periodic_poll
from util.retry import retry

FABRIC_SCRIPT = path.abspath(path.join(
    path.dirname(__file__),
    "../../../../buildfarm/maintenance/manage_masters.py"))


def check_fabric():
    try:
        import fabric
        assert fabric  # pylint
    except ImportError:
        logging.error("FAIL: fabric not installed", exc_info=True)
        raise


class FabricHelper(object):
    def __init__(self, masters_json_file, roles, concurrency=8,
                 subprocess=False, warning_interval=300, callback=None,
                 username=None, ssh_key=None):
        self.masters_json_file = masters_json_file
        self.roles = roles
        self.concurrency = concurrency
        self.subprocess = subprocess
        self.warning_interval = warning_interval
        self.callback = callback
        self.username = username
        self.ssh_key = ssh_key

    def fabric_cmd(self, actions, **cmdKwargs):
        cmd = ['python', FABRIC_SCRIPT, '-f', self.masters_json_file,
               '-j', str(self.concurrency)]
        for role in self.roles:
            cmd += ['-R', role]
        if self.username:
            cmd += ['--username', self.username]
        if self.ssh_key:
            cmd += ['--ssh-key', self.ssh_key]
        cmd += actions
        # don't buffer output
        env = {'PYTHONUNBUFFERED': '1'}
        if self.subprocess:
            run_cmd_periodic_poll(cmd, warning_interval=self.warning_interval,
                                  warning_callback=self.callback, env=env,
                                  **cmdKwargs)
        else:
            run_cmd(cmd, env=env)

    def update(self, **cmdKwargs):
        self.fabric_cmd(['update'], **cmdKwargs)

    def checkconfig(self, **cmdKwargs):
        self.fabric_cmd(['checkconfig'], **cmdKwargs)

    def reconfig(self, **cmdKwargs):
        retry(self.fabric_cmd, args=(['reconfig'], ), **cmdKwargs)

    def update_and_reconfig(self, **cmdKwargs):
        self.update(**cmdKwargs)
        self.checkconfig(**cmdKwargs)
        self.reconfig(**cmdKwargs)
