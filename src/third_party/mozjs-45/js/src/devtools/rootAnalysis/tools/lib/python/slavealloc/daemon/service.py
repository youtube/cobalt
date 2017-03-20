from twisted.python import log
from twisted.internet import defer
from twisted.application import service
from slavealloc.logic import allocate, buildbottac
from slavealloc import exceptions


class AllocatorService(service.Service):

    def startService(self):
        log.msg("starting AllocatorService")
        service.Service.startService(self)
        # doesn't do anything for now..

    def stopService(self):
        log.msg("stopping AllocatorService")
        return service.Service.stopService(self)

    def getBuildbotTac(self, slave_name):
        # slave allocation is *synchronous* and happens in the main thread.
        # For now, this is a good thing - it allows us to ensure that multiple
        # slaves are not simultaneously reassigned.  This should be revisited
        # later.
        d = defer.succeed(None)

        def gettac(_):
            try:
                allocation = allocate.Allocation(slave_name)
            except exceptions.NoAllocationError:
                log.msg("rejecting slave '%s'" % slave_name)
                raise

            tac = buildbottac.make_buildbot_tac(allocation)

            allocation.commit()
            if allocation.enabled:
                log.msg("allocated '%s' to '%s' (%s:%s)" % (slave_name,
                                                            allocation.master_nickname,
                                                            allocation.master_fqdn,
                                                            allocation.master_pb_port))
            else:
                log.msg(
                    "slave '%s' is disabled; no allocation made" % slave_name)

            return tac
        d.addCallback(gettac)
        return d
