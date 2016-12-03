from fabric.api import env, local, run as fabric_run
from fabric.context_managers import hide, show, lcd, cd as fabric_cd
from fabric.operations import put as fabric_put
from fabric.colors import green, red
import re
import os
import sys
import inspect
import shutil
import time

from util.retry import retry

OK = green('[OK]')
FAIL = red('[FAIL]')

RECONFIG_LOCKFILE = 'reconfig.lock'

BUILDBOT_WRANGLER = os.path.normpath(os.path.join(
    os.path.dirname(__file__),
    "../../../../buildfarm/maintenance/buildbot-wrangler.py"))

def is_local(host_string):
    if env.host_string in ('127.0.0.1', 'localhost'):
        return True
    else:
        return False

def cd(d):
    if is_local(env.host_string):
        return lcd(d)
    else:
        return fabric_cd(d)

def run(cmd, workdir=None):
    def doit():
        if is_local(env.host_string):
            return local(cmd, capture=True)
        else:
            return fabric_run(cmd)
    if workdir:
        with cd(workdir):
            return doit()
    else:
        return doit()

def put(src, dst):
    if is_local(env.host_string):
        return shutil.copyfile(src, dst)
    else:
        return fabric_put(src, dst)

def get_actions():
    current_module = sys.modules[__name__]
    for name in dir(current_module):
        attr = getattr(current_module, name)
        if inspect.isfunction(attr) and name.startswith('action_'):
            yield name.replace('action_', '')


def action_check(master):
    """Checks that the master parameters are valid"""
    with hide('stdout', 'stderr', 'running'):
        date = run('date')
        run('test -d %(bbcustom_dir)s' % master)
        run('test -d %(bbconfigs_dir)s' % master)
        run('test -d %(master_dir)s' % master)
        run('test -d %(tools_dir)s' % master)

        assert run('hg -R %(bbcustom_dir)s ident -b' % master) == \
            master['bbcustom_branch']
        assert run('hg -R %(bbconfigs_dir)s ident -b' % master) == \
            master['bbconfigs_branch']
        assert run('hg -R %(tools_dir)s ident -b' % master) == \
            master['tools_branch']
        print master['name'], date, OK


def action_checkconfig(master):
    """Runs buildbot checkconfig"""
    action_check(master)
    with hide('stdout', 'stderr'):
        try:
            run('make checkconfig', workdir=master['basedir'])
            print "%-14s %s" % (master['name'], OK)
        except:
            print "%-14s %s" % (master['name'], FAIL)
            raise


def action_show_revisions(master):
    """Reports the revisions of: buildbotcustom, buildbot-configs, tools, buildbot"""
    with hide('stdout', 'stderr', 'running'):
        bbcustom_rev = run('hg -R %(bbcustom_dir)s ident -i' % master)
        bbconfigs_rev = run('hg -R %(bbconfigs_dir)s ident -i' % master)
        tools_rev = run('hg -R %(tools_dir)s ident -i' % master)
        bbcustom_rev = bbcustom_rev.split()[0]
        bbconfigs_rev = bbconfigs_rev.split()[0]
        tools_rev = tools_rev.split()[0]

        bb_version = run('unset PYTHONHOME PYTHONPATH; '
                         '%(buildbot_bin)s --version' % master)
        bb_version = bb_version.replace('\r\n', '\n')
        m = re.search('^Buildbot version:.*-hg-([0-9a-f]+)-%s' %
                      master['buildbot_branch'], bb_version, re.M)
        if not m:
            print FAIL, "Failed to parse buildbot --version output:", \
                repr(bb_version)
            bb_rev = ""
        else:
            bb_rev = m.group(1)

        show_revisions_detail(master['name'], bbcustom_rev, bbconfigs_rev,
                              tools_rev, bb_rev)


def show_revisions_detail(master, bbcustom_rev, bbconfigs_rev,
                          tools_rev, buildbot_rev):
    print "%-25s %12s %12s %12s %12s" % (master, bbcustom_rev, bbconfigs_rev,
                                         tools_rev, buildbot_rev)


def show_revisions_header():
    show_revisions_detail("master", "bbcustom", "bbconfigs", "tools",
                          "buildbot")


def action_reconfig(master):
    """Performs a reconfig (only - no update or checkconfig)"""
    print "starting reconfig of %(hostname)s:%(basedir)s" % master
    with hide('stdout', 'stderr', 'running'):
        lockfile_check = run('if [ -e %s ]; then echo "lockfile found"; fi' % RECONFIG_LOCKFILE, workdir=master['basedir'])
        if lockfile_check != "":
            print FAIL, "lockfile (%s) found in %s:%s" % (RECONFIG_LOCKFILE,
                                                          master['hostname'],
                                                          master['basedir'])
            raise Exception("Couldn't get lockfile to reconfig")
    with show('running'):
        action_create_reconfig_lockfile(master, notify=False)
        put(BUILDBOT_WRANGLER,
            '%s/buildbot-wrangler.py' % master['basedir'])
        run('rm -f *.pyc', workdir=master['basedir'])
        run('python buildbot-wrangler.py reconfig %s' %
            master['master_dir'], workdir=master['basedir'])
        action_remove_reconfig_lockfile(master, notify=False)
    print OK, "finished reconfig of %(hostname)s:%(basedir)s" % master


def action_restart(master):
    with show('running'):
        put(BUILDBOT_WRANGLER, '%s/buildbot-wrangler.py' %
            master['basedir'])
        run('rm -f *.pyc', workdir=master['basedir'])
        run('python buildbot-wrangler.py restart %s' %
            master['master_dir'], workdir=master['basedir'])
    print OK, "finished restarting of %(hostname)s:%(basedir)s" % master


def action_graceful_restart(master):
    with show('running'):
        put(BUILDBOT_WRANGLER, '%s/buildbot-wrangler.py' %
            master['basedir'])
        run('rm -f *.pyc', workdir=master['basedir'])
        run('python buildbot-wrangler.py graceful_restart %s %s' %
            (master['master_dir'], master['http_port']), workdir=master['basedir'])
    print OK, \
        "finished gracefully restarting of %(hostname)s:%(basedir)s" % master


def action_stop(master):
    with show('running'):
        put(BUILDBOT_WRANGLER,
            '%s/buildbot-wrangler.py' % master['basedir'])
        run('python buildbot-wrangler.py stop %s' % master['master_dir'], workdir=master['basedir'])
    print OK, "stopped %(hostname)s:%(basedir)s" % master


def action_graceful_stop(master):
    with show('running'):
        put(BUILDBOT_WRANGLER,
            '%s/buildbot-wrangler.py' % master['basedir'])
        run('rm -f *.pyc', workdir=master['basedir'])
        run('python buildbot-wrangler.py graceful_stop %s %s' %
            (master['master_dir'], master['http_port']), workdir=master['basedir'])
    print OK, "gracefully stopped %(hostname)s:%(basedir)s" % master


def start(master):
    with show('running'):
        put(BUILDBOT_WRANGLER,
            '%s/buildbot-wrangler.py' % master['basedir'])
        run('rm -f *.pyc', workdir=master['basedir'])
        run('python buildbot-wrangler.py start %s' % master['master_dir'], workdir=master['basedir'])
    print OK, "started %(hostname)s:%(basedir)s" % master


def action_update(master):
    print "sleeping 30 seconds to make sure that hg.m.o syncs NFS... ",
    time.sleep(30)
    print OK
    with show('running'):
        retry(run, args=('source bin/activate && make update',),
                kwargs={'workdir': master['basedir']}, sleeptime=10,
                retry_exceptions=(SystemExit,))
    print OK, "updated %(hostname)s:%(basedir)s" % master


def action_update_buildbot(master):
    with show('running'):
        buildbot_dir = os.path.dirname(master['buildbot_setup'])
        run('hg pull', workdir=buildbot_dir)
        run('hg update -r %s' % master['buildbot_branch'], workdir=buildbot_dir)
        run('unset PYTHONHOME PYTHONPATH; %s setup.py install' %
            master['buildbot_python'], workdir=buildbot_dir)
    print OK, "updated buildbot in %(hostname)s:%(basedir)s" % master


def action_uptime(master):
    with hide('stdout', 'stderr', 'running'):
        uptime = run('uptime')
        print "%-25s %12s" % (master['name'], uptime)


def action_fix_makefile_symlink(master):
    with show('running'):
        run('rm -f %(basedir)s/Makefile' % master)
        run('ln -s %(bbconfigs_dir)s/Makefile.master %(basedir)s/Makefile' %
            master)
    print OK, "updated Makefile symlink in %(hostname)s:%(basedir)s" % master


def action_add_esr38_symlinks(master):
    with show('running'):
        run('ln -s %(bbconfigs_dir)s/mozilla/release-firefox-mozilla-esr38.py '
            '%(master_dir)s/' % master)
        run('ln -s %(bbconfigs_dir)s/mozilla/l10n-changesets_mozilla-esr38 '
            '%(master_dir)s/' % master)
        run('ln -s %(bbconfigs_dir)s/mozilla/release-thunderbird-comm-esr38.py '
            '%(master_dir)s/' % master)
        run('ln -s %(bbconfigs_dir)s/mozilla/l10n-changesets_thunderbird-esr38 '
            '%(master_dir)s/' % master)
    print OK, "Added esr38 symlinks in %(hostname)s:%(basedir)s" % master


def action_rm_34_1_symlinks(master):
    with show('running'):
        run('rm -f %(master_dir)s/release-firefox-mozilla-release-34.1.py' %
            master)
        run('rm -f %(master_dir)s/l10n-changesets_mozilla-release-34.1' %
            master)
    print OK, "Removed 34.1 symlinks in %(hostname)s:%(basedir)s" % master


def action_add_gecko_version_symlinks(master):
    with show('running'):
        run('ln -s %(bbconfigs_dir)s/mozilla/gecko_versions.json '
            '%(master_dir)s/' % master)


def action_add_config_seta_symlinks(master):
    with show('running'):
        run('ln -s %(bbconfigs_dir)s/mozilla-tests/config_seta.py '
            '%(master_dir)s/' % master)


def action_update_exception_timestamp(master):
    with show('running'):
        run('date +%s > /home/cltbld/.{0}-last-time.txt'.format(master['name']))


def per_host(fn):
    fn.per_host = True
    return fn


@per_host
def action_update_queue(host):
    with show('running'):
        queue_dir = "/builds/buildbot/queue"
        tools_dir = "%s/tools" % queue_dir
        run('hg pull -u', workdir=tools_dir)
    print OK, "updated queue in %s" % host


@per_host
def action_retry_dead_queue(host):
    for q in 'commands', 'pulse':
        cmd = "find /dev/shm/queue/%s/dead -type f" % q
        for f in run(cmd).split("\n"):
            f = f.strip()
            if not f:
                continue
            with show('running'):
                if f.endswith(".log"):
                    run("rm %s" % f)
                else:
                    run("mv %s /dev/shm/queue/%s/new" % (f, q))


def action_master_health(master):
    with show('running'):
        run('ls -l %(master_dir)s/*.pid' % master)
        run('free -m')


def action_create_reconfig_lockfile(master, notify=True):
    with hide('stdout', 'stderr', 'running'):
        run('touch %s' % RECONFIG_LOCKFILE, workdir=master['basedir'])
    if notify:
        print OK, "Created %s in %s:%s" % (RECONFIG_LOCKFILE,
                                           master['hostname'],
                                           master['basedir'])


def action_remove_reconfig_lockfile(master, notify=True):
    with hide('stdout', 'stderr', 'running'):
        run('rm -f %s' % RECONFIG_LOCKFILE, workdir=master['basedir'])
    if notify:
        print OK, "Removed %s from %s:%s" % (RECONFIG_LOCKFILE,
                                             master['hostname'],
                                             master['basedir'])


def action_restart_pulse_publisher(master):
    with show('running'):
        run('/etc/init.d/pulse_publisher restart')
    print OK, 'Pulse publisher restarted on %s:%s' % (master['hostname'], master['basedir'])

