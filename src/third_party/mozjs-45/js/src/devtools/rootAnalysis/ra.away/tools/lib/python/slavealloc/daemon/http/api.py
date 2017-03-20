import sqlalchemy as sa
import simplejson
from twisted.python import log
from twisted.web import resource
from slavealloc import exceptions
from slavealloc.data import queries, model
from slavealloc.logic import allocate, buildbottac

# point your browser to /api/ to see the full set of docs
docs_tpl = """
<h1>REST Interface</h1>
<p>This is a JSON-based REST interface.  Rows are represented as JSON objects with
keys corersponding to database columns.</p>

<p>For each table, there is a similarly named sub-URL, e.g., <a
href="/api/slaves">/api/slaves</a>.  A GET to that URL will return a full dump
of the table.  As a convenience, the slaves and masters tables have all of
their normalized fields denormalized, so in addition to a <tt>poolid</tt> you
will get a <tt>pool</tt> string.</p>

<p>A particular row can be fetched by appending the primary key in the next URL
component, e.g., <a href="/api/pools/3">/api/pools/3</a>.  To fetch the row by
name instead, use the name in the URL and add <tt>?byname=1</tt>, e.g., <a
href="/api/masters/pm03?byname=1">/api/masters/pm03?byname=1</a>.  If a fetched
row does not exist, the API will return <tt>{}</tt> with a 404 status code.</p>

<p>You can PUT a JSON object with any subset of keys to either of these URLs to
modify the row.  The denormalized columns in the masters and slaves cannot be
PUT.</p>

<p>The entire set of available tables is:
    <ul>
    %(tables)s
    </ul>
</p>

<p>The <a href="/api/gettac/slavename">/api/gettac/slavename</a> URL is
different: it invokes the allocator, but does not "commit" the allocation --
meaning that the selected master is not recorded in the
<tt>current_masterid</tt> table for the requested slave.  The result is JSON --
either {success=False} or {success=True, tac='content of buildbot.tac'}.</p>

"""

# base classes


class Instance(resource.Resource):
    isLeaf = True
    okResponse = simplejson.dumps(dict(success=True))
    missingResponse = simplejson.dumps({})

    # a tuple of columns that can be updated via PUT
    update_keys = ()

    # the table to access
    table = None

    # the column containing unique id's
    id_column = None

    # the column containing unique names
    name_column = None

    def __init__(self, id=None, name=None):
        self.id = id
        self.name = name

    def whereClause(self, require_id=False):
        "Calculate a where clause and an args dict for this instance"
        if self.id is not None:
            return (self.id_column == sa.bindparam('id'),
                    dict(id=self.id))
        else:
            assert not require_id
            return (self.name_column == sa.bindparam('name'),
                    dict(name=self.name))

    def render_GET(self, request):
        wc, args = self.whereClause()
        res = self.table.select(wc).execute(args)
        row = res.fetchone()
        res.close()

        request.setHeader('content-type', 'application/json')
        request.setHeader('Cache-control', 'no-cache')

        # handle nonexistent rows
        if not row:
            request.setResponseCode(404)
            return self.missingResponse

        return simplejson.dumps(dict(row))

    def render_PUT(self, request):
        json = simplejson.load(request.content)

        sets = dict((k, json[k]) for k in self.update_keys if k in json)
        if not sets:
            return  # nothing to do!

        log.msg("%s: updating id %s from %r" %
                (self.table.name, self.id, sets))

        wc, args = self.whereClause(require_id=True)
        args.update(sets)
        self.table.update(wc).execute(args)

        return self.okResponse


class Collection(resource.Resource):
    addSlash = True
    isLeaf = False

    # the query to return for GET requests; if None, this will use the instance
    # class's table and just select everything
    query = None

    def getChild(self, path_component, request):
        if not path_component:
            return

        # if we're looking up by name, try that instead
        if request.args.get('byname'):
            return self.instance_class(name=path_component)
        else:
            return self.instance_class(id=path_component)

    def render_GET(self, request):
        query = self.query
        if query is None:
            query = self.instance_class.table.select()
        res = query.execute()

        request.setHeader('content-type', 'application/json')
        request.setHeader('Cache-control', 'no-cache')
        return simplejson.dumps([dict(r.items()) for r in res.fetchall()])


# concrete classes

# slaves
class SlaveResource(Instance):
    table = model.slaves
    id_column = model.slaves.c.slaveid
    name_column = model.slaves.c.name
    update_keys = ('distroid', 'dcid', 'bitsid', 'purposeid', 'trustid',
                   'envid', 'poolid', 'basedir', 'locked_masterid', 'notes',
                   'enabled', 'custom_tplid')


class SlavesResource(Collection):
    instance_class = SlaveResource
    # set in render_GET
    query = None

    def render_GET(self, request):
        environments = request.args.get('environment')
        purposes = request.args.get('purpose')
        pools = request.args.get('pool')
        enabled = request.args.get('enabled', [None])[0]
        self.query = queries.denormalized_slaves(environments, purposes, pools, enabled)
        return Collection.render_GET(self, request)

# masters


class MasterResource(Instance):
    table = model.masters
    id_column = model.masters.c.masterid
    name_column = model.masters.c.nickname
    update_keys = ('nickname', 'fqdn', 'pb_port', 'http_port', 'poolid',
                   'dcid', 'notes', 'enabled')


class MastersResource(Collection):
    instance_class = MasterResource
    query = queries.denormalized_masters

# TAC templates


class TACTemplateResource(Instance):
    table = model.tac_templates
    id_column = model.tac_templates.c.tplid
    name_column = model.tac_templates.c.name
    update_keys = ()  # view only via API


class TACTemplatesResource(Collection):
    instance_class = TACTemplateResource

# simple


def simple_table_resource(tbl, id_column_name, name_column_name='name'):
    "make instance and collection classes for a simple id/name table"
    class SimpleInstance(Instance):
        table = tbl
        id_column = table.c[id_column_name]
        name_column = table.c[name_column_name]
        update_keys = (name_column_name,)

    class SimpleCollection(Collection):
        instance_class = SimpleInstance

    return SimpleCollection

DistrosResource = simple_table_resource(model.distros, 'distroid')
DatacentersResource = simple_table_resource(model.datacenters, 'dcid')
BitlengthsResource = simple_table_resource(model.bitlengths, 'bitsid')
SpeedsResource = simple_table_resource(model.speeds, 'speedid')
PurposesResource = simple_table_resource(model.purposes, 'purposeid')
TrustlevelsResource = simple_table_resource(model.trustlevels, 'trustid')
EnvironmentsResource = simple_table_resource(model.environments, 'envid')
PoolsResource = simple_table_resource(model.pools, 'poolid')

# allocator


class BuildbotTacResource(resource.Resource):
    isLeaf = True

    def __init__(self, slave):
        self.slave = slave

    def render_GET(self, request):
        try:
            alloc = allocate.Allocation(self.slave)
        except exceptions.NoAllocationError:
            alloc = None

        request.setHeader('content-type', 'application/json')
        request.setHeader('Cache-control', 'no-cache')

        if not alloc:
            request.setResponseCode(404)
            return simplejson.dumps(dict(success=False))
        else:
            return simplejson.dumps(dict(
                success=True,
                tac=buildbottac.make_buildbot_tac(alloc)))


class BuildbotTacRootResource(resource.Resource):
    "A JSON-style allocator that will get a TAC file, but not record it"
    isLeaf = False

    def getChild(self, path_component, request):
        return BuildbotTacResource(path_component)

# root URI


class ApiRoot(resource.Resource):
    addSlash = True
    isLeaf = False

    def __init__(self):
        resource.Resource.__init__(self)
        self.tables = []

        self.addTable('slaves', SlavesResource)
        self.addTable('masters', MastersResource)
        self.addTable('distros', DistrosResource)
        self.addTable('datacenters', DatacentersResource)
        self.addTable('bitlengths', BitlengthsResource)
        self.addTable('speeds', SpeedsResource)
        self.addTable('purposes', PurposesResource)
        self.addTable('trustlevels', TrustlevelsResource)
        self.addTable('environments', EnvironmentsResource)
        self.addTable('pools', PoolsResource)
        self.addTable('tac_templates', TACTemplatesResource)
        self.addTable('gettac', BuildbotTacRootResource)

    def addTable(self, name, coll_class):
        self.tables.append(name)
        self.putChild(name, coll_class())

    def getChild(self, path_component, request):
        # allow '/api/' to mean the same as '/api'
        if not path_component:
            return self

    def render_GET(self, request):
        tables_list = '\n'.join(['<li>%s</li>' % t for t in self.tables])
        return docs_tpl % dict(tables=tables_list)


def makeRootResource():
    return ApiRoot()
