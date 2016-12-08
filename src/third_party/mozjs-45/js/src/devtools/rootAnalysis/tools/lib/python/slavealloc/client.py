import simplejson
from zope.interface import implements
from twisted.web import client, iweb, http_headers, _newclient
from twisted.internet import protocol, defer

# _newclient horks its Failure objects up by putting them in a list,
# so this errback will unhork them


def unhorkNewclientFailure(f):
    f.trap(_newclient.RequestGenerationFailed)
    exc = f.value
    if len(exc.reasons) != 1:
        return f
    return exc.reasons[0]


class JsonProducer(object):
    "Produce a JSON-encoded request body"
    implements(iweb.IBodyProducer)

    def __init__(self, data):
        self.body = simplejson.dumps(data)
        self.length = len(self.body)

    def startProducing(self, consumer):
        consumer.write(self.body)
        return defer.succeed(None)

    def pauseProducing(self):
        pass

    def stopProducing(self):
        pass


class JsonProtocol(protocol.Protocol):
    "Consume JSON data and fire a Deferred with the decoded result"
    def __init__(self, d):
        self.d = d
        self.d.addCallback(self.toJson)
        self.data_segments = []

    def dataReceived(self, bytes):
        self.data_segments.append(bytes)

    def toJson(self, data):
        try:
            return simplejson.loads(data)
        except:
            print "RESPONSE BODY:\n" + data
            raise

    def connectionLost(self, reason):
        if reason.check(client.ResponseDone):
            data = ''.join(self.data_segments)
            self.d.callback(data)
        else:
            self.d.errback(reason)


class RestAgent(client.Agent):
    """A wrapper around L{Agent} to make JSON-based REST requests simpler."""

    def __init__(self, reactor, apiurl):
        """Construct the REST agent, including the base URL of the API"""
        client.Agent.__init__(self, reactor)
        self.apiurl = apiurl.rstrip('/')

    def restRequest(self, method, path, request_data):
        """Issue a REST request, wrapping L{Agent.request}.  The C{pathparts}
        argument is a tuple of path components, and C{request_data} is a Python
        data structure that will be JSON-encoded.  The result will be decoded
        and returned as the value of the Deferred."""
        body_producer = JsonProducer(request_data)
        headers = http_headers.Headers({
            'User-Agent': ['slavealloc command line'],
            'Content-Type': ['application/json'],
        })
        url = self.apiurl + '/' + path
        d = self.request(method, url, headers, body_producer)
        d.addErrback(unhorkNewclientFailure)

        def json_to_python(response):
            # TODO: error handling, check content type
            d = defer.Deferred()
            response.deliverBody(JsonProtocol(d))
            return d
        d.addCallback(json_to_python)
        return d
