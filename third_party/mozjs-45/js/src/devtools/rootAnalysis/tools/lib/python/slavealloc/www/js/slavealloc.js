var js_root = window.slavealloc_base_url + 'ui/js/';

load(
    js_root + 'deps/jquery-1.9.1.min.js',
    js_root + 'deps/underscore-1.4.4.min.js')
.thenLoad(
    js_root + 'deps/jquery.dataTables-1.9.4.min.js',
    js_root + 'deps/jquery-ui-1.8.9.custom.min.js',
    js_root + 'deps/backbone-0.9.10.min.js')
.thenLoad(
    js_root + 'models.js',
    js_root + 'views.js',
    js_root + 'controller.js')
.thenRun(
    function (next) {
        window.slaves = new Slaves();
        window.slaves.fetch({ success: next,
            error: function() { $('#error').text('error loading slaves').show(); } });
    },
    function (next) {
        window.masters = new Masters();
        window.masters.fetch({ success: next,
            error: function() { $('#error').text('error loading masters').show(); } });
    },
    function (next) {
        window.distros = new Distros();
        window.distros.fetch({ success: next,
            error: function() { $('#error').text('error loading distros').show(); } });
    },
    function (next) {
        window.datacenters = new Datacenters();
        window.datacenters.fetch({ success: next,
            error: function() { $('#error').text('error loading datacenters').show(); } });
    },
    function (next) {
        window.bitlengths = new Bitlengths();
        window.bitlengths.fetch({ success: next,
            error: function() { $('#error').text('error loading bitlengths').show(); } });
    },
    function (next) {
        window.speeds = new Speeds();
        window.speeds.fetch({ success: next,
            error: function() { $('#error').text('error loading speeds').show(); } });
    },
    function (next) {
        window.purposes = new Purposes();
        window.purposes.fetch({ success: next,
            error: function() { $('#error').text('error loading purposes').show(); } });
    },
    function (next) {
        window.trustlevels = new Trustlevels();
        window.trustlevels.fetch({ success: next,
            error: function() { $('#error').text('error loading trustlevels').show(); } });
    },
    function (next) {
        window.environments = new Environments();
        window.environments.fetch({ success: next,
            error: function() { $('#error').text('error loading environments').show(); } });
    },
    function (next) {
        window.pools = new Pools();
        window.pools.fetch({ success: next,
            error: function() { $('#error').text('error loading pools').show(); } });
    },
    function (next) {
        window.tac_templates = new TACTemplates();
        window.tac_templates.fetch({ success: next,
            error: function() { $('#error').text('error loading tac_templates').show(); } });
    })
.thenRun(function () {
    // fire up the controller and start the history mgmt
    var controller = new SlaveallocController();
    Backbone.history.start();
});
