import time
import socket

template_header = """\
# AUTOMATICALLY GENERATED - DO NOT MODIFY
# generated: %(gendate)s on %(genhost)s

"""

tac_template_disabled = template_header + """\
print "SLAVE DISABLED; NOT STARTING"

import sys
sys.exit(0)
"""

default_template = """\
from twisted.application import service
from buildslave.bot import BuildSlave

import twisted.spread.pb
twisted.spread.pb.MAX_BROKER_REFS = 4096

maxdelay = 300
buildmaster_host = %(buildmaster_host)r
passwd = %(passwd)r
maxRotatedFiles = 10
basedir = %(basedir)r
umask = 002
slavename = %(slavename)r
usepty = False
rotateLength = 1000000
port = %(port)r
keepalive = None

application = service.Application('buildslave')
try:
    from twisted.python.logfile import LogFile
    from twisted.python.log import ILogObserver, FileLogObserver
    logfile = LogFile.fromFullPath("twistd.log", rotateLength=rotateLength,
                             maxRotatedFiles=maxRotatedFiles)
    application.setComponent(ILogObserver, FileLogObserver(logfile).emit)
except ImportError:
    pass
s = BuildSlave(buildmaster_host, port, slavename, passwd, basedir,
               keepalive, usepty, umask=umask, maxdelay=maxdelay, allow_shutdown="file")
s.setServiceParent(application)
"""

idleizer_template = """\
# enable idleizer
from buildslave import idleizer
idlz = idleizer.Idleizer(s,
        # 7 hours' idle time before a reboot
        max_idle_time=3600*7,
        # 1 hour disconnected from a master before a reboot
        max_disconnected_time=3600*1)
idlz.setServiceParent(application)
"""

# get this value once and keep it - getfqdn() can be a *very* expensive call
genhost = socket.getfqdn()


def make_buildbot_tac(allocation):
    info = dict()

    info['gendate'] = time.ctime()
    info['genhost'] = genhost

    # short-circuit for disabled slaves
    if not allocation.enabled:
        return tac_template_disabled % info

    info['buildmaster_host'] = allocation.master_fqdn
    info['port'] = allocation.master_pb_port
    info['slavename'] = allocation.slavename
    info['basedir'] = allocation.slave_basedir
    info['passwd'] = allocation.slave_password
    template = template_header
    if allocation.template:
        template += allocation.template
    else:
        template += default_template
        if not 'panda-' in info['slavename'] and \
           not 'tegra' in info['slavename']:
            # XXX We shouldn't hardcode the slave check here
            template += idleizer_template

    return unicode(template % info).encode('utf8')
