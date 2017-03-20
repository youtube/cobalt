from twisted.internet import defer, reactor
from slavealloc import client, exceptions


def setup_argparse(subparsers):
    subparser = subparsers.add_parser(
        'gettac', help='get a tac file for a slave')
    subparser.add_argument('slave', nargs='*',
                           help="slave hostnames to allocate for (no domain)")
    subparser.add_argument('-n', '--noop', dest='noop',
                           default=False, action='store_true',
                           help="don't actually allocate")
    return subparser


def process_args(subparser, args):
    if not args.slave:
        subparser.error("at least one slave name is required")
    if '.' in ''.join(args.slave):
        subparser.error(
            "slave name must not contain '.'; give the unqualified hostname")


@defer.inlineCallbacks
def main(args):
    agent = client.RestAgent(reactor, args.apiurl)

    for slave in args.slave:
        path = 'gettac/%s' % slave
        res = yield agent.restRequest('GET', path, {})

        if not res.get('success'):
            raise exceptions.CmdlineError(
                "could not generate TAC for %s" % slave)

        print res['tac']
