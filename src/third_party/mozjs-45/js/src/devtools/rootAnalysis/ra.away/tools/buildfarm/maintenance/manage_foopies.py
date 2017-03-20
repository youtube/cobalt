#!/usr/bin/env python
import foopy_fabric
from fabric.api import env
from fabric.context_managers import settings
from fabric.colors import red
from Crypto.Random import atfork

FAIL = red('[FAIL]')

_devices_map = dict()
_foopy_map = dict()


def _calc_maps(devices_file):
    global _devices_map, _foopy_map
    all_devices = json.load(urllib.urlopen(devices_file))
    _devices_map = dict([[x, all_devices[x]] for x in all_devices
                         if 'foopy' in all_devices[x]
                         and all_devices[x]['foopy'] is not None])
    # Extract foopies from devices
    all_foopies = [all_devices[x]['foopy'] for x in all_devices
                   if 'foopy' in all_devices[x]]
    # Use set to trim dupes
    all_foopies = set(all_foopies) - set(["None"])
    # For sanity when using -H all, we revert to list and sort
    all_foopies = [x for x in all_foopies]
    all_foopies.sort()
    for foopy in all_foopies:
        _foopy_map[foopy] = dict([[x, _devices_map[x]] for x in _devices_map
                                  if _devices_map[x]['foopy'] == foopy])


def _calc_selected(hosts, devices):
    global _foopy_map, _devices_map
    from copy import deepcopy
    selected = dict()
    if 'all' in hosts or 'all' in devices:
        selected = deepcopy(_foopy_map)
        return selected

    for f in hosts:
        if f in _foopy_map:
            selected[f] = deepcopy(_foopy_map[f])
    for d in devices:
        if d not in _devices_map:
            sys.stderr.write("Warning: device '%s' not found in device map\n" % d)
            continue
        if 'foopy' not in _devices_map[d]:
            continue
        if _devices_map[d]['foopy'] not in _foopy_map:
            continue
        if _devices_map[d]['foopy'] in selected:
            selected[_devices_map[d]['foopy']][d] = deepcopy(d)
        else:  # No mapping for this foopy
            selected[_devices_map[d]['foopy']] = dict()
            selected[_devices_map[d]['foopy']][d] = deepcopy(d)
    return selected


def run_action_on_foopy(action, foopy):
    atfork()
    try:
        action_func = getattr(foopy_fabric, action)
        with settings(host_string="%s.build.mozilla.org" % foopy):
            if options.argument:
                action_func(foopy, options.argument)
            else:
                action_func(foopy)
            return True
    except AttributeError:
        print FAIL, "[%s] %s action is not defined." % (foopy, action)
        return False
    except:
        import traceback
        print "Failed to run", action, "on", foopy
        print traceback.format_exc()
        return False


def run_action_on_devices(action, foopy_dict):
    atfork()
    action_func = getattr(foopy_fabric, action)
    try:
        with settings(host_string="%s.build.mozilla.org" % foopy_dict['host']):
            for device in foopy_dict['devices']:
                if hasattr(action_func, "needs_device_dict"):
                    action_func(device, _devices_map[device])
                else:
                    action_func(device)
            return True
        return False
    except AttributeError:
        print FAIL, "[%s] %s action is not defined." % (foopy_dict['host'], action)
        return False
    except:
        import traceback
        print "Failed to run", action, "on", foopy_dict['host']
        print traceback.format_exc()
        return False

if __name__ == '__main__':
    from optparse import OptionParser
    import sys
    import urllib
    try:
        import simplejson as json
    except:
        import json

    parser = OptionParser("""%%prog [options] action [action ...]""")

    parser.set_defaults(
        hosts=[],
        devices=[],
        concurrency=1,
    )
    parser.add_option("-f", "--devices-file", dest="devices_file", help="list/url of devices.json")
    parser.add_option("-H", "--host", dest="hosts", action="append")
    parser.add_option("-D", "--device", dest="devices", action="append")
    parser.add_option("-j", dest="concurrency", type="int")
    parser.add_option("-a", dest="argument", action="store")

    options, actions = parser.parse_args()

    if options.concurrency > 1:
        import multiprocessing

    if not options.devices_file:
        parser.error("devices-file is required")

    if not actions:
        parser.error("at least one action is required")

    _calc_maps(options.devices_file)

    selected = _calc_selected(options.hosts, options.devices)

    if len(selected.keys()) == 0:
        parser.error("You need to specify a foopy via -H or a device via -D")

    env.user = 'cltbld'

    for action in actions:
        fn = run_action_on_foopy
        genArg = lambda x: x
        if hasattr(getattr(foopy_fabric, action), "per_device"):
            fn = run_action_on_devices
            genArg = lambda x: {'host': x, 'devices': selected[x]}

        if options.concurrency == 1:
            for foopy in sorted(selected.keys()):
                fn(action, genArg(foopy))
        else:
            # Don't prompt for passwords when forking
            env.abort_on_prompts = True

            p = multiprocessing.Pool(processes=options.concurrency)
            results = []
            for foopy in sorted(selected.keys()):
                result = p.apply_async(fn, (action, genArg(foopy)))
                results.append((foopy, result))
            p.close()
            failed = False
            for foopy, result in results:
                if not result.get():
                    print foopy, "FAILED"
                    failed = True
            p.join()
            if failed:
                sys.exit(1)
