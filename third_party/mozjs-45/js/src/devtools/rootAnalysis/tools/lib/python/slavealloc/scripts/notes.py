import sys
from twisted.internet import defer, reactor
from slavealloc import client, exceptions


def setup_argparse(subparsers):
    subparser = subparsers.add_parser(
        'notes', help="get or set a slave's notes")
    subparser.add_argument('slave',
                           help="slave to edit")
    subparser.add_argument('-m', '--message',
                           dest='message', help="new slave notes")
    return subparser


def process_args(subparser, args):
    if not args.slave:
        subparser.error("slave name is required")
    if '.' in ''.join(args.slave):
        subparser.error(
            "slave name must not contain '.'; give the unqualified hostname")


@defer.inlineCallbacks
def main(args):
    agent = client.RestAgent(reactor, args.apiurl)

    # get first
    path = 'slaves/%s?byname=1' % args.slave
    slave = yield agent.restRequest('GET', path, {})
    if not slave:
        raise exceptions.CmdlineError(
            "No slave found named '%s'." % args.slave)
    assert slave['name'] == args.slave
    slaveid = slave['slaveid']
    if args.message is None:
        if slave['notes']:
            print slave['notes']
        return
    else:
        print >>sys.stderr, "previous slave notes: '%s'" % slave['notes']

    # set the notes
    set_result = yield agent.restRequest('PUT',
                                         'slaves/%d' % slaveid, {'notes': args.message})
    success = set_result.get('success')
    if not success:
        raise exceptions.CmdlineError("Operation failed on server.")
    print >>sys.stderr, "notes set to '%s'" % args.message
