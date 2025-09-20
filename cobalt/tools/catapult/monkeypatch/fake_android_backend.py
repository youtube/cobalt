import logging


class Forwarder(object):
  _local_port = 9222
  _remote_port = 9223

  def Close(self):
    None


class FakeFactory(object):

  def Create(self, *args, **kwargs):
    logging.info("Create called")
    return Forwarder()


class FakeBackend(object):
  forwarder_factory = FakeFactory()

  def GetOSName(self):
    # Lie about our OS, so browser_target_url() doesn't need modifications
    return "castos"


class FakeConn(object):
  platform_backend = FakeBackend()
