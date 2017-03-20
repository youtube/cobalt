#!/usr/bin/python
try:
    import simplejson as json
except ImportError:
    import json
import urllib
import logging
import re

log = logging.getLogger(__name__)


def check_masters(masters):
    retval = True
    for master in masters:
        if not check_master(master):
            retval = False
    return retval


def load_masters(url):
    if 'http' in url:
        fp = urllib.urlopen(url)
    else:
        fp = open(url)
    return json.load(fp)


def check_master(master):
    # Check required keys
    name = master['name']
    required_keys = ('hostname', 'enabled', 'master_dir', 'name', 'role',
                     'basedir', 'bbconfigs_dir', 'db_name', 'bbcustom_dir',
                     'bbcustom_branch', 'bbconfigs_branch', 'tools_dir',
                     'tools_branch', 'datacentre', 'buildbot_bin', 'buildbot_branch',
                     'buildbot_python', 'buildbot_setup', 'environment')
    opt_keys = ('http_port', 'ssh_port', 'pb_port', 'buildbot_version',
                'limit_fx_platforms', 'limit_tb_platforms', 'limit_b2g_platforms',
                'release_branches', 'thunderbird_release_branches', 'mobile_release_branches')
    int_keys = ('http_port', 'ssh_port', 'pb_port')

    for k in required_keys:
        if k not in master:
            log.error("%s - missing key %s", name, k)
            return False

    for k in master.keys():
        if k not in required_keys + opt_keys:
            log.error("%s - unknown key %s", name, k)
            return False

    for k in int_keys:
        if k in master:
            if type(master[k]) not in (int, long):
                log.error("%s - non-integer key %s", name, k)
                return False

    hostname, domain = master['hostname'].split(".", 1)

    # Check domain
    if not re.match("(build|srv\.releng)\.(\w+)\.mozilla\.com", domain):
        log.error("%s - bad domain %s", name, domain)
        return False

    # Check hostname
    host_num = re.match("buildbot-master(\d{2})", hostname)
    if not host_num:
        log.error("%s - bad hostname %s", name, hostname)
        return False

    # Check short name
    role = master['role']
    abbrev = "bm%s" % host_num.group(1)
    exp = "%s-%s(\d)" % (abbrev, role)
    instance_num = re.match(exp, name)
    if not instance_num:
        # TODO: schedulers don't follow this logic
        log.error("%s - bad name (doesn't match %s)", name, exp)
        return False

    # Check port numbers
    instance_num = int(instance_num.group(1))
    instance = "%s%i" % (role, instance_num)
    if role == 'build':
        required_ports = ['http', 'ssh', 'pb']
        role_offset = 0
    elif role == 'try':
        required_ports = ['http', 'ssh', 'pb']
        role_offset = 100
    elif role == 'tests':
        required_ports = ['http', 'ssh', 'pb']
        role_offset = 200
    elif role == 'scheduler':
        required_ports = ['ssh', 'pb']
        role_offset = 300
    else:
        log.error("%s - unknown role %s", name, role)
        return False

    ports = {
        "ssh": 7000 + role_offset + instance_num,
        "http": 8000 + role_offset + instance_num,
        "pb": 9000 + role_offset + instance_num,
    }
    for proto in required_ports:
        master_port = master.get("%s_port" % proto)
        if master_port != ports[proto]:
            log.error("%s - bad %s port (got %i, expected %i)",
                      name, proto, master_port, ports[proto])
            return False

    # Check master_dir
    if master['master_dir'] != "/builds/buildbot/%s/master" % instance:
        # TODO this needs tweaking for tests
        log.error("%s - bad master_dir %s" % (name, master.get(
            'master_dir', 'None')))
        return False

    # Check basedir
    if master['basedir'] != "/builds/buildbot/%s" % instance:
        log.error("%s - bad basedir", name)
        return False

    # Check db_name
    db_name = '%s:%s' % (master['hostname'], master['master_dir'])
    if master['db_name'] != db_name:
        log.error("%s - bad db_name (should be %s)", name, db_name)
        return False

    # Check datacentre
    if master['datacentre'] not in ('scl3',):
        log.error("%s - bad datacentre", name)
        return False

    if master['datacentre'] not in master['hostname']:
        log.error("%s - datacentre/hostname mismatch", name)
        return False

    return True

if __name__ == '__main__':
    logging.basicConfig()
    masters = load_masters('production-masters.json')
    check_masters(masters)
