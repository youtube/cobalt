import sys
from twisted.internet import defer, reactor
from slavealloc import client, exceptions


def setup_argparse(subparsers):
    subparser = subparsers.add_parser(
        'lock', help='lock a slave to a particular master')
    subparser.add_argument('slave',
                           help="slave to (un)lock")
    subparser.add_argument('master', nargs='?',
                           help="master to lock it to")
    subparser.add_argument('-u', '--unlock', dest='unlock',
                           default=False, action='store_true',
                           help="unlock a locked slave (no need to specify master)")
    return subparser


def process_args(subparser, args):
    if not args.slave:
        subparser.error("slave name is required")
    if '.' in ''.join(args.slave):
        subparser.error(
            "slave name must not contain '.'; give the unqualified hostname")
    if not args.master and not args.unlock:
        subparser.error("master name is required to lock")


@defer.inlineCallbacks
def main(args):
    agent = client.RestAgent(reactor, args.apiurl)

    # first get the slaveid
    path = 'slaves/%s?byname=1' % args.slave
    slave = yield agent.restRequest('GET', path, {})
    if not slave:
        raise exceptions.CmdlineError(
            "No slave found named '%s'." % args.slave)
    assert slave['name'] == args.slave
    slaveid = slave['slaveid']

    if args.unlock:
        if not slave['locked_masterid']:
            raise exceptions.CmdlineError("Slave is not locked")

        set_result = yield agent.restRequest('PUT',
                                             'slaves/%d' % slaveid,
                                             {'locked_masterid': None})
        success = set_result.get('success')
        if not success:
            raise exceptions.CmdlineError("Operation failed on server.")

        print >>sys.stderr, "Slave '%s' unlocked" % args.slave
    else:
        # get the masterid
        path = 'masters/%s?byname=1' % args.master
        master = yield agent.restRequest('GET', path, {})
        if not master:
            raise exceptions.CmdlineError(
                "No master found named '%s'." % args.master)
        masterid = master['masterid']

        set_result = yield agent.restRequest('PUT',
                                             'slaves/%d' % slaveid,
                                             {'locked_masterid': masterid})
        success = set_result.get('success')
        if not success:
            raise exceptions.CmdlineError("Operation failed on server.")

        print >>sys.stderr, "Locked '%s' to '%s' (%s:%s)" % (args.slave,
                                                             master['nickname'], master['fqdn'], master['pb_port'])
