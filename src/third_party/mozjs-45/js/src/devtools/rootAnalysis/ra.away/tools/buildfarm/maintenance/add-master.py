#!/usr/bin/python
import simplejson
import sys
import os
import re


def new_master(hostname, type_, instance=1):
    assert type_ in ('build', 'tests', 'try')
    localname = "%s%i" % (type_, instance)
    hostname_data = re.match(
        "buildbot-master(\d{2})\.build\.(\w+)\.mozilla\.com", hostname)
    assert hostname_data
    host_abbrev = "bm%s" % hostname_data.group(1)
    datacentre = hostname_data.group(2)

    if type_ == 'build':
        port_base = 0 + instance
    elif type_ == 'try':
        port_base = 100 + instance
    elif type_ == 'tests':
        port_base = 200 + instance

    return {
        "hostname": hostname,
        "enabled": False,
        "environment": "production",
        "master_dir": "/builds/buildbot/%s/master" % localname,
        "name": "%s-%s" % (host_abbrev, localname),
        "role": type_,
        "basedir": "/builds/buildbot/%s" % localname,
        "buildbot_version": "0.8.2",
        "buildbot_bin": "/builds/buildbot/%s/bin/buildbot" % localname,
        "buildbot_branch": "production-0.8",
        "buildbot_python": "/builds/buildbot/%s/bin/python" % localname,
        "buildbot_setup": "/builds/buildbot/%s/buildbot/master/setup.py" % localname,
        "bbconfigs_dir": "/builds/buildbot/%s/buildbot-configs" % localname,
        "db_name": "%s:/builds/buildbot/%s/master" % (hostname, localname),
        "bbcustom_dir": "/builds/buildbot/%s/buildbotcustom" % localname,
        "bbcustom_branch": "production-0.8",
        "bbconfigs_branch": "production",
        "tools_dir": "/builds/buildbot/%s/tools" % localname,
        "tools_branch": "default",
        "ssh_port": 7000 + port_base,
        "http_port": 8000 + port_base,
        "pb_port": 9000 + port_base,
        "datacentre": datacentre,
    }

fn = sys.argv[1]
masters = simplejson.load(open(sys.argv[1]))
masters.append(new_master(*sys.argv[2:]))

os.rename(fn, '%s.bak' % fn)

open(fn, 'w').write(simplejson.dumps(masters, indent=2, sort_keys=True))
