from fabric.api import run
from fabric.context_managers import cd, hide, show
from fabric.context_managers import settings
from fabric.colors import green, yellow, red

OK = green('[OK]  ')
FAIL = red('[FAIL]')
INFO = yellow('[INFO]')


def per_host(fn):
    fn.per_host = True
    return fn


def per_device(fn):
    fn.per_device = True
    return fn


def use_json(fn):
    fn.needs_device_dict = True
    return fn


@per_host
def show_revision(foopy):
    with hide('stdout', 'stderr', 'running'):
        tools_rev = run('hg -R /builds/tools ident -i')

        print "%-14s %12s" % (foopy, tools_rev)


@per_device
def status(device):
   device_dir = '/builds/%s' % device
   stat_buildbot = "not running"
   stat_disabled = "enabled"
   stat_error = ""
   with hide('stdout', 'stderr', 'running'):
       with cd(device_dir):
            with settings(hide('everything'), warn_only=True):
                pid_exists = not run('test -e "$(echo %s/twistd.pid)"' % device_dir).failed
                if pid_exists:
                    if not run('kill -0 `cat %s/twistd.pid`' % device_dir).failed:
                        stat_buildbot = "running"
                if not run('test -e "$(echo %s/disabled.flg)"' % device_dir).failed:
                    stat_disabled = "disabled"
                    stat_error = run('cat %s/disabled.flg' % device_dir)
                if not run('test -e "$(echo %s/error.flg)"' % device_dir).failed:
                    stat_error = run('cat %s/error.flg' % device_dir)

   print "%s - %12s - %10s - %s" % (device, stat_buildbot, stat_disabled, repr(stat_error[:65].replace('\r\n', '--')))

@per_device
@use_json
def reboot(device, json):
  bank, relay = json['relayid'].split(":")
  run("python /builds/sut_tools/relay.py powercycle %s %s %s ; sleep 5" % (json['relayhost'], bank, relay))
  print OK, "Powercycled %s" % device



@per_host
def update(foopy, revision='default'):
    with show('running'):
        with cd('/builds/tools'):
            run('hg pull && hg update -r %s' % revision)
            run('find /builds/tools -name \\*.pyc -exec rm {} \\;')
            with hide('stdout', 'stderr', 'running'):
                tools_rev = run('hg ident -i')

    print OK, "updated %s tools to %12s" % (foopy, tools_rev)

@per_device
def create_error(device):
    with show('running'):
        with cd('/builds'):
            run('touch /builds/%s/error.flg' % device)
    print OK, "ran on %s" % device


@per_device
def stop(device):
    with show('running'):
        with cd('/builds/%s' % device):
            run('echo "disable please" > ./disabled.flg')
        print OK, "Stopped %s" % (device)

@per_device
def enable(device):
    with show('running'):
        with cd('/builds/%s' % device):
            run('rm -f ./disabled.flg')
            remove_error(device)
        print OK, "enabled %s" % device

@per_device
def remove_error(device):
    with show('running'):
        with cd('/builds/%s' % device):
            run('rm -f ./error.flg')
        print OK, "Removed error flag for %s" % (device)

@per_device
def create_device_dirs(device):
    from fabric.api import env
    with settings(warn_only=True, user='root'):
       run("mkdir /builds/%s" % device)
    with settings(user='root'):
       run("chown cltbld.cltbld /builds/%s" % device)

@per_device
def what_master(device):
    import re
    with hide('stdout', 'stderr', 'running'):
        with cd('/builds'):
            tac_path = './%s/buildbot.tac' % device
            tac_exists = False
            with settings(hide('everything'), warn_only=True):
                tac_exists = not run('test -e "$(echo %s)"' % tac_path).failed
            if tac_exists:
                output = run('cat %s | grep "^buildmaster_host"' % tac_path)
                m = re.search('^buildmaster_host\s*=\s*([\'"])(.*)[\'"]', output, re.M)
                if not m:
                    print FAIL, "Failed to parse buildbot.tac:", repr(output)
                    master = "No Master"
                else:
                    master = m.group(2)
            else:
                master = "No Master"
        print OK, "%s uses %s" % (device, master)

actions = [
    'what_master',
    'show_revision',
    'update',
    'stop_cp',
]
