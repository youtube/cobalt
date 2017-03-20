//
// Base Classes
//

var DenormalizedModel = Backbone.Model.extend({
    // call this to update denormalized columns (e.g., 'datacenter') to
    // their normalized column ('dcid') changes.
    bindDenormalizedColumns: function(column_info) {
        var self = this;

        $.each(column_info, function (i, info) {
            var foreign_name_col = info.foreign_name_col || 'name';
            var change_fn = function (model, id) {
                var newname = (id === null)? null :
                        info.model.get(id).get(foreign_name_col);
                var set = {};
                set[info.name_col] = newname;
                model.set(set);
            };

            self.bind('change:' + info.id_col, change_fn);
        });
    }
});

//
// Slaves
//

var Slave = DenormalizedModel.extend({
    initialize: function() {
        this.id = this.get('slaveid');

        this.bindDenormalizedColumns([
            { name_col: 'distro', model: window.distros, id_col: 'distroid' },
            { name_col: 'datacenter', model: window.datacenters, id_col: 'dcid' },
            { name_col: 'bitlength', model: window.bitlengths, id_col: 'bitsid' },
            { name_col: 'speed', model: window.speeds, id_col: 'speedid' },
            { name_col: 'purpose', model: window.purposes, id_col: 'purposeid' },
            { name_col: 'trustlevel', model: window.trustlevels, id_col: 'trustid' },
            { name_col: 'environment', model: window.environments, id_col: 'envid' },
            { name_col: 'custom_template', model: window.tac_templates,
                    id_col: 'custom_tplid', foreign_name_col: 'tplid' },
            { name_col: 'pool', model: window.pools, id_col: 'poolid' },
            { name_col: 'locked_master', model: window.masters,
                    id_col: 'locked_masterid', foreign_name_col: 'nickname' },
            { name_col: 'current_master', model: window.masters,
                    id_col: 'current_masterid', foreign_name_col: 'nickname' }
        ]);
    }
});

var Slaves = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/slaves',
    model: Slave,
    comparator: function(m) {
        return m.get('name');
    }
});

//
// Masters
//

var Master = DenormalizedModel.extend({
    initialize: function() {
        this.id = this.get('masterid');

        this.bindDenormalizedColumns([
            { name_col: 'pool', model: window.pools, id_col: 'poolid' }
        ]);
    }
});

var Masters = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/masters',
    model: Master,
    comparator: function(m) {
        return m.get('nickname');
    }
});

//
// ID-to-name models
//

var Distro = Backbone.Model.extend({
    initialize: function() {
        this.id = this.get('distroid');
    }
});

var Distros = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/distros',
    model: Distro,

    // information about the columns in this collection
    columns: [
        { id: "name", title: "Name" }
    ],
    comparator: function(m) {
        return m.get('name');
    }
});

var Datacenter = Backbone.Model.extend({
    initialize: function() {
        this.id = this.get('dcid');
    }
});

var Datacenters = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/datacenters',
    model: Datacenter,

    // information about the columns in this collection
    columns: [
        { id: "name", title: "Name" }
    ],
    comparator: function(m) {
        return m.get('name');
    }
});

var Bitlength = Backbone.Model.extend({
    initialize: function() {
        this.id = this.get('bitsid');
    }
});

var Bitlengths = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/bitlengths',
    model: Bitlength,

    // information about the columns in this collection
    columns: [
        { id: "name", title: "Name" }
    ],
    comparator: function(m) {
        return m.get('name');
    }
});

var Speed = Backbone.Model.extend({
    initialize: function() {
        this.id = this.get('speedid');
    }
});

var Speeds = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/speeds',
    model: Speed,

    // information about the columns in this collection
    columns: [
        { id: "name", title: "Name" }
    ],
    comparator: function(m) {
        return m.get('name');
    }
});

var Purpose = Backbone.Model.extend({
    initialize: function() {
        this.id = this.get('purposeid');
    }
});

var Purposes = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/purposes',
    model: Purpose,

    // information about the columns in this collection
    columns: [
        { id: "name", title: "Name" }
    ],
    comparator: function(m) {
        return m.get('name');
    }
});

var Trustlevel = Backbone.Model.extend({
    initialize: function() {
        this.id = this.get('trustid');
    }
});

var Trustlevels = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/trustlevels',
    model: Trustlevel,

    // information about the columns in this collection
    columns: [
        { id: "name", title: "Name" }
    ],
    comparator: function(m) {
        return m.get('name');
    }
});

var Environment = Backbone.Model.extend({
    initialize: function() {
        this.id = this.get('envid');
    }
});

var Environments = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/environments',
    model: Environment,

    // information about the columns in this collection
    columns: [
        { id: "name", title: "Name" }
    ],
    comparator: function(m) {
        return m.get('name');
    }
});

var Pool = Backbone.Model.extend({
    initialize: function() {
        this.id = this.get('poolid');
    }
});

var Pools = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/pools',
    model: Pool,

    // information about the columns in this collection
    columns: [
        { id: "name", title: "Name" }
    ],
    comparator: function(m) {
        return m.get('name');
    }
});

var TACTemplate = Backbone.Model.extend({
    initialize: function() {
        this.id = this.get('tplid');
    }
});

var TACTemplates = Backbone.Collection.extend({
    url: window.slavealloc_base_url + 'api/tac_templates',
    model: TACTemplate,

    // information about the columns in this collection
    columns: [
        { id: "name", title: "Name" },
        { id: "template", title: "Template" }
    ],
    comparator: function(m) {
        return m.get('name');
    }
});

