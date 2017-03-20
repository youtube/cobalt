from twisted.application import service as tw_service, strports
from slavealloc.data import setup
from slavealloc.daemon import service
from slavealloc.daemon.http import site


class Allocator(tw_service.MultiService):
    def __init__(self, http_port, db_url, db_kwargs={},
                 base_url='/',
                 run_allocator=False, run_ui=False):
        tw_service.MultiService.__init__(self)

        setup.setup(db_url, db_kwargs)

        if run_allocator:
            self.allocator = service.AllocatorService()
            self.allocator.setServiceParent(self)
        else:
            self.allocator = None

        if run_allocator or run_ui:
            self.site = site.Site(self.allocator,
                                  run_allocator=run_allocator, run_ui=run_ui,
                                  base_url=base_url)
            self.httpservice = strports.service(http_port, self.site)
            self.httpservice.setServiceParent(self)
