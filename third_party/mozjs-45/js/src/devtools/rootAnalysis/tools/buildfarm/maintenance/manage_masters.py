#!/usr/bin/env python

import site
from os import path
import time
from fabric.api import env
from fabric.context_managers import settings
from Crypto.Random import atfork

site.addsitedir(
    path.join(path.dirname(path.realpath(__file__)), "../../lib/python"))

import util.fabric.actions


def print_status(remaining, failed_masters):
    if remaining:
        print "=" * 30, "Remaining masters", "=" * 30
        for m in remaining:
            print m
    if failed_masters:
        print "=" * 30, "failed masters", "=" * 30
        for m in failed_masters:
            print m
    print "=" * 80


def run_action_on_master(action, master):
    atfork()
    try:
        action_func = getattr(util.fabric.actions, "action_%s" % action)
        with settings(host_string=master.get('ip_address', master['hostname'])):
            action_func(master)
            return True
    except AttributeError:
        print "[%s] %s action is not defined." % (master['hostname'], action)
        return False
    except:
        import traceback
        print "Failed to run", action, "on", master['name']
        print traceback.format_exc()
        return False

if __name__ == '__main__':
    import sys
    import urllib
    from optparse import OptionParser
    import textwrap
    try:
        import simplejson as json
    except ImportError:
        import json

    actions = []
    action_module = util.fabric.actions
    for a in action_module.get_actions():
        actions.append(a)
    actions.sort()
    actions_with_help = '\n'.join(["  %s\n    %s" % (a,
                                                     action_module.__dict__['action_%s' % a].__doc__)
                                   for a in actions if
                                   action_module.__dict__['action_%s' % a].__doc__])
    action_without_help = ", ".join([a for a in actions if not
                                     action_module.__dict__['action_%s' %
                                                            a].__doc__])
    action_help = actions_with_help
    if action_without_help:
        action_help += "\nOther supported actions:\n" + \
            textwrap.TextWrapper(initial_indent='  ', subsequent_indent='    ').fill(action_without_help)
    parser = OptionParser("""%%prog [options] action [action ...]

Supported actions:
%s""" % action_help)

    parser.set_defaults(
        hosts=[],
        roles=[],
        datacentre=[],
        concurrency=1,
        show_list=False,
        all_masters=False,
        ignored_roles=[],
    )
    parser.add_option("-f", "--master-file", dest="master_file",
                      help="list/url of masters")
    parser.add_option("-H", "--host", dest="hosts", action="append")
    parser.add_option("-R", "--role", dest="roles", action="append")
    parser.add_option("-M", "--match", dest="match", action="append",
                      help="masters that match the term")
    parser.add_option("-D", "--datacentre", dest="datacentre", action="append")
    parser.add_option("-j", dest="concurrency", type="int")
    parser.add_option("-l", dest="show_list", action="store_true",
                      help="list hosts")
    parser.add_option("--all", dest="all_masters", action="store_true",
                      help="work on all masters, not just enabled ones")
    parser.add_option("-i", dest="status_interval", type="int", default="60",
                      help="Interval between statuses")
    parser.add_option("-u", "--username", dest="username", default="cltbld",
                      help="Username passed to Fabric")
    parser.add_option("-k", "--ssh-key", dest="ssh_key",
                      help="SSH key passed to Fabric")
    parser.add_option("--ignore-role", dest="ignored_roles", action="append",
                      help="Ignore masters with this role. May be passed multiple times.")

    options, actions = parser.parse_args()

    if options.concurrency > 1:
        import multiprocessing

    if not options.master_file:
        parser.error("master-file is required")

    if not actions and not options.show_list:
        parser.error("at least one action is required")

    # Load master data
    all_masters = json.load(urllib.urlopen(options.master_file))

    masters = []

    for m in all_masters:
        if not m['enabled'] and not options.all_masters:
            continue
        if options.ignored_roles and m['role'] in options.ignored_roles:
            continue
        if options.datacentre and m['datacentre'] not in options.datacentre:
            continue
        if m['name'] in options.hosts:
            masters.append(m)
        elif m['role'] in options.roles:
            masters.append(m)
        elif options.match:
            for match in options.match:
                if match in m["name"]:
                    masters.append(m)
        elif 'all' in options.hosts or 'all' in options.roles:
            masters.append(m)

    if options.show_list:
        if len(masters) == 0:
            masters = [m for m in all_masters if m['enabled'] or
                       options.all_masters]

        fmt = "%(role)-9s %(name)-14s %(hostname)s:%(basedir)s"
        print fmt % dict(role='role', name='name', hostname='hostname',
                         basedir='basedir')
        for m in masters:
            print fmt % m
        sys.exit(0)

    if len(masters) == 0:
        parser.error("No masters matched, check your options -H, -R, -M, -D")

    env.user = options.username
    if options.ssh_key:
        env.key_filename = options.ssh_key
    selected_masters = masters
    for action in actions:
        if hasattr(action, 'per_host'):
            hosts = set(m['hostname'] for m in masters)
            masters = [dict(hostname=h) for h in hosts]
        else:
            masters = selected_masters

        try:
            action_header = getattr(action_module, action + "_header")
            action_header()
        except AttributeError:
            # we don't care if there's no header to print
            pass

        if options.concurrency == 1:
            for master in masters:
                run_action_on_master(action, master)
        else:
            # Don't prompt for passwords when forking
            env.abort_on_prompts = True

            p = multiprocessing.Pool(processes=options.concurrency)
            results = []
            for master in masters:
                result = p.apply_async(run_action_on_master, (action, master))
                results.append((master, result))
            p.close()
            failed = False
            failed_masters = []
            tries = 0
            while True:
                for master, result in list(results):
                    if result.ready():
                        results.remove((master, result))
                        if not result.get():
                            failed_masters.append(master['name'])
                            print master['name'], "FAILED"
                            failed = True
                tries += 1
                if not results:
                    break
                if tries % options.status_interval == 0:
                    print_status([m['name'] for (m, r) in results],
                                 failed_masters)
                time.sleep(1)

            p.join()
            # One final print before we exit, to be sure that results are not
            # missed
            print_status([m['name'] for (m, r) in results],
                         failed_masters)
            if failed:
                # failure info may have scrolled, so repeat here
                print "Action '%s' failed on %d masters:\n%s\n" \
                    % (action, len(failed_masters),
                       textwrap.TextWrapper(initial_indent='  ',
                                            subsequent_indent='  ').fill(", ".join(
                                                                         failed_masters)))
                sys.exit(1)
