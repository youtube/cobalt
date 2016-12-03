import sys
from twisted.internet import defer, reactor
from slavealloc import client, exceptions


def setup_argparse(subparsers):
    subparser = subparsers.add_parser(
        'disable', help='disable a slave, preventing it from starting')
    subparser.add_argument('slave',
                           help="slave to disable (or enable with --enable)")
    subparser.add_argument('-m', '--message',
                           dest='message', help="new slave notes")
    subparser.add_argument('-e', '--enable', dest='enable',
                           default=False, action='store_true',
                           help="enable a disabled slave (deprecated)")
    return subparser


def process_args(subparser, args):
    if not args.slave:
        subparser.error("slave name is required")
    if '.' in ''.join(args.slave):
        subparser.error(
            "slave name must not contain '.'; give the unqualified hostname")


def bool_to_word(bool):
    return {True: 'enabled', False: 'disabled'}[bool]


@defer.inlineCallbacks
def set_enabled(args, slavename, enabled, message):
    """
    Set the enabled status for a slave, and optionally (if C{message} is not
    None) the notes as well.
    """
    agent = client.RestAgent(reactor, args.apiurl)

    # first get the slaveid
    path = 'slaves/%s?byname=1' % slavename
    slave = yield agent.restRequest('GET', path, {})
    if not slave:
        raise exceptions.CmdlineError(
            "No slave found named '%s'." % slavename)
    assert slave['name'] == slavename
    slaveid = slave['slaveid']
    if slave['notes']:
        print >>sys.stderr, "previous slave notes: '%s'" % slave['notes']

    # then set its state, if not already set
    if ((enabled and not slave['enabled']) or
            (not enabled and slave['enabled'])):
        to_set = {'enabled': enabled}
        if message is not None:
            to_set['notes'] = message
        set_result = yield agent.restRequest('PUT',
                                             'slaves/%d' % slaveid, to_set)
        success = set_result.get('success')
        if not success:
            raise exceptions.CmdlineError("Operation failed on server.")
        print >>sys.stderr, "%s %s" % (
            slavename, bool_to_word(enabled))
    else:
        print >>sys.stderr, "%s is already %s" % (
            slavename, bool_to_word(enabled))


def main(args):
    if args.enable:
        print >>sys.stderr, ("NOTE: 'slavealloc enable SLAVENAME' is available; "
                             "'slavealloc disable -e' is deprecated")
    return set_enabled(args, args.slave, args.enable, args.message)
