//
// Parent Classes
//

// A class to view a collection in a DataTables table, with each row
// represented by a TableRowView instance, and with table rows being editable
// via TableRowFormView instances.  The subclass should set this.model in
// initialize before callign the parent method, and should set the rowViewClass
// class variable to the appropriate subclass.  Subclasses can also set
// defaultPageLength to affect pagination.  The 'columns' attribute should be a
// list of coluns to display, giving 'id' (model attribute) and 'title'
// (display name).  If a 'renderFn' attribute is supplied, it names a method
// that will be called with the row model, and should return an HTML string
// containing the appropriate content.  The data in the model named by 'id'
// will still be used for sorting.
var TableView = Backbone.View.extend({
    tagName: 'div',
    defaultPageLength: 50, // or 0 to disable pagination

    initialize: function(args) {
        this.editingRow = null; // row currently being edited

        this.refresh = $.proxy(this, 'refresh');
        this.render = $.proxy(this, 'render');

        this.model.bind('refresh', this.refresh);
    },

    render: function() {
        var self = this;

        // dataTable likes to add sibling nodes, so add the table within
        // the enclosing div, this.el
        $(this.el).append('<table class="display" ' +
            'cellspacing="0" cellpadding="0" border="0"></table>');

        // calculate the columns for the table; this is coordinated with
        // the data for the table in refresh(), below

        // hidden id column
        var aoColumns = [ { bVisible: false } ];

        // data columns
        aoColumns = aoColumns.concat(
            this.columns.map(function(col) {
                var rv = { sTitle: col.title, sClass: 'col-' + col.id };
                if (col.renderFn) {
                    rv.bUseRendered = false;
                    rv.fnRender = function(oObj) {
                        var model = self.model.get(oObj.aData[0]);
                        return self[col.renderFn].apply(self, [ model ]);
                    };
                } 
                if (col.sClass) {
                    rv.sClass = col.sClass;
                }
                return rv;
            }));

        // and un-sortable 'edit' column
        aoColumns.push({
            sTitle: 'Edit',
            bSortable: false,
            fnRender: function () { return '<button class="row-edit"/>'; }
        });

        // create an empty table
        var dtArgs = {
            aoColumns: aoColumns,
            bJQueryUI: true,
            bAutoWidth: false
        };
        if (this.defaultPageLength !== 0) {
            var pl = this.defaultPageLength;
            dtArgs['bPaginate'] = true;
            dtArgs['iDisplayLength'] = pl;
            dtArgs['aLengthMenu'] = [ [ pl, -1 ], [ pl, "All" ] ];
        } else {
            dtArgs['bPaginage'] = false;
        }

        this.dataTable = this.$('table').dataTable(dtArgs);

        this.refresh();

        return this;
    },

    refresh : function (args) {
        var self = this;

        var newData = self.model.map(function(instance_model) {
            // put the model in the hidden column 0
            var row = [ instance_model.id ];
            // data columns
            row = row.concat(
                $.map(self.columns, function(col, i) {
                    return String(instance_model.get(col.id) || '');
                }));
            // and the 'edit' column
            row.push("");
            return row;
        });

        // clear (without drawing) and add the new data
        self.dataTable.fnClearTable(false);
        self.dataTable.fnAddData(newData);
        $.each(self.dataTable.fnGetNodes(), function (i, tr) {
            var row = self.dataTable.fnGetData(tr);
            var instance_model = self.model.get(row[0]);
            var view = new self.rowViewClass({
                model: instance_model,
                el: tr,
                dataTable: self.dataTable,
                parentView: self
            });
            view.render();
        });
    }
});

// A class to represent each row in a TableView; subclasses should set
// editViewClass, but need not do anything else.
var TableRowView = Backbone.View.extend({
    initialize: function(args) {
        this.dataTable = args.dataTable;
        this.parentView = args.parentView;
        this.editView = null;

        this.refresh = $.proxy(this, 'refresh');
        this.toggleEdit = $.proxy(this, 'toggleEdit');

        this.model.bind('change', this.refresh);
    },

    events: {
        'click .row-edit': 'toggleEdit'
    },

    render: function() {
        // note that this.el is set by the parent view
        var edit_button = this.$(".row-edit");
        edit_button.button({ icons: { primary: 'ui-icon-pencil' }, text: false });
        return this;
    },

    refresh: function() {
        var self = this;
        var lastcol = self.parentView.columns.length - 1;
        var rownum = this.dataTable.fnGetPosition(this.el);

        $.each(self.parentView.columns, function (i, col) {
            var val = self.model.get(col.id);
            if (val === null) { val = ''; } // translate null to a string
            self.dataTable.fnUpdate(String(val),
                rownum,
                i+1, // 1-based column (column 0 is the hidden id)
                false, // don't redraw
                (i === lastcol)); // but update data structures on last col
        });

        // redraw once, now that everything is updated
        self.dataTable.fnDraw(false);
    },

    toggleEdit: function(evt) {
        var editingRow = this.parentView.editingRow;
        if (editingRow == this) {
            this.doStopEdit();
        } else {
            if (editingRow) {
                editingRow.doStopEdit();
            }
            this.doStartEdit();
        }

        evt.preventDefault();
    },

    doStopEdit: function() {
        this.dataTable.fnClose(this.el);
        $(this.el).removeClass('row_selected');
        this.parentView.editingRow = null;
        this.editView.remove();
        this.editView = null;
    },

    doStartEdit: function() {
        $(this.el).addClass('row_selected');
        var editRow = this.dataTable.fnOpen(this.el, '', 'edit-row');

        var td = $('td', editRow);
        this.editView = new this.editViewClass({ model: this.model, el: td });
        this.editView.render();

        this.parentView.editingRow = this;
    }
});

// A class to represent the drop-down editing form for a table row.  Subclasses
// should call the parent initialize method from initialize, and should also
// set this.formElements to a list of EditControlView instances.
var TableEditRowView = Backbone.View.extend({
    initialize: function(args) {
        this.render = $.proxy(this, 'render');
    },

    render: function() {
        var self = this;
        var div = $('<div>', { 'class': 'edit-row' });

        $.each(this.formElements, function(i, elt) {
            elt.render();
            div.append(elt.el);
        });

        $(this.el).append(div);
        return this;
    }
});

// A control contained in a TableEditRowView.  List instances of subclasses in
// the formElements attribute of a TableEditRowView instance.  Subclasses
// should implement 'render' and 'modelChanged', and handle events in whatever
// way is suitable.
var EditControlView = Backbone.View.extend({
    tagName: 'div',
    className: 'edit-control',

    initialize: function(args) {
        this.column = args.column;
        this.title = args.title;

        this.model.bind('change:' + this.column,
                        $.proxy(this, 'modelChanged'));
    }
});

// like EditControlView, but with a pre-built render method
// that adds a text element and handles modelChanged and
// controlChanged.  
var TextEditControlView = EditControlView.extend({
    events: {
        'change :text': 'controlChanged'
    },

    modelChanged: function() {
        this.$(':text').val(this.model.get(this.column));
    },

    controlChanged: function() {
        var sets = {};
        sets[this.column] = this.$(':text').val();
        this.model.set(sets);
        this.model.save();
    },

    render: function() {
        var el = $(this.el);
        el.append($('<label/>', { text: this.title + ': ' }));

        var input = $('<input/>', { name: this.column, type: 'text' });
        el.append(input);

        input.val(this.model.get(this.column));

        return this;
    }
});

// Similarly, an EditControlView that implements a select box, where
// the choices are taken from the collection named in the
// choice_collection argument.  The display name is taken from the 
// choice_collection_name_col column in the choice_collection model,
// which defaults to 'name'
var SelectEditControlView = EditControlView.extend({
    choice_collection_name_col: 'name',

    events: {
        'change select': 'controlChanged'
    },

    initialize: function(args) {
        EditControlView.prototype.initialize.call(this, args);

        this.allow_null = args.allow_null;
        this.choice_collection = args.choice_collection;
        if (args.choice_collection_name_col) {
            this.choice_collection_name_col = args.choice_collection_name_col;
        }
    },

    modelChanged: function() {
        var val = this.model.get(this.column);

        // replace null with "null"
        val = (val === null)? "null" : val;

        this.$('select').val(val);
    },

    controlChanged: function() {
        var sets = {};
        var val = this.$('select').val();

        // replace "null" with null; otherwise parse the integer id
        val = (val == "null")? null : parseInt(val, 10);

        sets[this.column] = val;
        this.model.set(sets);
        this.model.save();
    },

    render: function() {
        var self = this;
        var el = $(self.el);
        el.append($('<label/>', { text: self.title + ': ' }));

        var select = $('<select/>', { name: self.column });
        self.choice_collection.each(function (model) {
            $('<option/>', {
                val: model.id,
                text: model.get(self.choice_collection_name_col)
            }).appendTo(select);
        });
        if (this.allow_null) {
            $('<option/>', { val: 'null', text: '' }).appendTo(select);
        }

        el.append(select);

        this.modelChanged();

        return self;
    }
});

// like EditControlView, but displaying a checkbox and rendering a boolean.
// value.
var CheckboxEditControlView = EditControlView.extend({
    events: {
        'change :checkbox': 'controlChanged'
    },

    modelChanged: function() {
        this.$(':checkbox').attr('checked', this.model.get(this.column));
    },

    controlChanged: function() {
        var sets = {};
        sets[this.column] = this.$(':checkbox').is(':checked')?  true : false;
        this.model.set(sets);
        this.model.save();
    },

    render: function() {
        var el = $(this.el);
        el.append($('<label/>', { text: this.title + ': ' }));

        var input = $('<input/>', { name: this.column, type: 'checkbox' });
        el.append(input);

        input.attr('checked', this.model.get(this.column));

        return this;
    }
});

//
// Slaves
//

var SlaveEditRowView = TableEditRowView.extend({
    initialize: function(args) {
        TableEditRowView.prototype.initialize.call(this, args);

        this.formElements = [
            new SelectEditControlView({model: this.model,
                    column: 'distroid', title: 'Distro',
                    choice_collection: window.distros }),
            new SelectEditControlView({model: this.model,
                    column: 'bitsid', title: 'Bitlength',
                    choice_collection: window.bitlengths }),
            new SelectEditControlView({model: this.model,
                    column: 'speedid', title: 'Speed',
                    choice_collection: window.speeds }),
            new TextEditControlView({model: this.model,
                    column: 'basedir', title: 'Basedir'}),
            new SelectEditControlView({model: this.model,
                    column: 'dcid', title: 'Datacenter',
                    choice_collection: window.datacenters }),
            new SelectEditControlView({model: this.model,
                    column: 'trustid', title: 'Trustlevel',
                    choice_collection: window.trustlevels }),
            new SelectEditControlView({model: this.model,
                    column: 'envid', title: 'Environment',
                    choice_collection: window.environments }),
            new SelectEditControlView({model: this.model,
                    column: 'purposeid', title: 'Purposes',
                    choice_collection: window.purposes }),
            new SelectEditControlView({model: this.model,
                    column: 'poolid', title: 'Pool',
                    choice_collection: window.pools }),
            new SelectEditControlView({model: this.model,
                    column: 'locked_masterid', title: 'Locked Master',
                    choice_collection: window.masters,
                    choice_collection_name_col: 'nickname',
                    allow_null: true }),
            new SelectEditControlView({model: this.model,
                    column: 'custom_tplid', title: 'TAC Template',
                    choice_collection: window.tac_templates,
                    choice_collection_name_col: 'name',
                    allow_null: true }),
            new CheckboxEditControlView({model: this.model,
                    column: 'enabled', title: 'Enabled' }),
            new TextEditControlView({model: this.model,
                    column: 'notes', title: 'Notes'})
        ];
    }
});

var SlaveTableRowView = TableRowView.extend({
    editViewClass: SlaveEditRowView
});

var BugRe = /(bug\#*\ *)(\d+)/ig;
BugRe.compile(BugRe);

var SlavesTableView = TableView.extend({
    rowViewClass: SlaveTableRowView,
    defaultPageLength: 10,

    columns: [
        { id: "name", title: "Name", 
	        renderFn: 'renderName'},
        { id: "distro", title: "Distro" },
        { id: "bitlength", title: "Bits" },
        { id: "speed", title: "Speed" },
        { id: "datacenter", title: "DC" },
        { id: "trustlevel", title: "Trust" },
        { id: "environment", title: "Environ" },
        { id: "purpose", title: "Purpose" },
        { id: "pool", title: "Pool" },
        { id: "current_master", title: "Current",
                renderFn: 'renderMasterCurrent' },
        { id: "locked_master", title: "Locked",
                renderFn: 'renderMasterLocked' },
        { id: "notes", title: "Notes",
	        renderFn: 'renderNotes' },
        { id: "enabled", renderFn: 'renderEnabled',
          sClass: 'center', title: "Enab" }
    ],

    initialize: function(args) {
        this.model = window.slaves;

        TableView.prototype.initialize.call(this, args);
    },

    renderName: function(model) {
	var slavename = model.get('name');
	return '<div id="' + slavename + '">' + slavename + '&nbsp;<span id="' + slavename + '-buglink"><img src="./icons/help.png" alt="Check bug status" title="Check bug status" onCLick="getBugByAlias(\'' + slavename + '\');" /></span></div>';
    },

    renderMaster: function(model, masterid) {
        // handle an empty master quickly
        if (masterid === null) {
            return '';
        }

        var master_model = window.masters.get(masterid);
        var fqdn = master_model.get('fqdn');
        var port = master_model.get('http_port');
        var nickname = master_model.get('nickname');
        var slavename = model.get('name');

        if (port === 0) {
            return slavename; // that's odd, but OK..
        }

        var a = $('<a/>', {
            href: 'http://' + fqdn + ':' + port + '/buildslaves/' + slavename,
            text: nickname
        });

        // wrap that in a div and extract the contents as a string
        return $('<div>').append(a).html();
    },

    renderMasterCurrent: function(model) {
        return this.renderMaster(model, model.get('current_masterid'));
    },

    renderMasterLocked: function(model) {
        return this.renderMaster(model, model.get('locked_masterid'));
    },

    renderNotes: function(model) {
        var notes = model.get('notes');
        if (notes) {
	    return notes.replace(BugRe,
				 "<a href='https://bugzil.la/$2'>$1$2</a>");
        } else {
            return '';
        }
    },

    renderEnabled: function (model) {
        if (model.get('enabled')) {
            return '&bull;';
        } else {
            return '';
        }
    }
});

//
// Masters
//

var MasterEditRowView = TableEditRowView.extend({
    initialize: function(args) {
        TableEditRowView.prototype.initialize.call(this, args);

        this.formElements = [
            new TextEditControlView({model: this.model,
                    column: 'nickname', title: 'Nickname'}),
            new TextEditControlView({model: this.model,
                    column: 'fqdn', title: 'FQDN'}),
            new TextEditControlView({model: this.model,
                    column: 'pb_port', title: 'PB Port'}),
            new TextEditControlView({model: this.model,
                    column: 'http_port', title: 'HTTP Port'}),
            new SelectEditControlView({model: this.model,
                    column: 'poolid', title: 'Pool',
                    choice_collection: window.pools }),
            new CheckboxEditControlView({model: this.model,
                    column: 'enabled', title: 'Enabled' }),
            new TextEditControlView({model: this.model,
                    column: 'notes', title: 'Notes'})
        ];
    }
});

var MasterTableRowView = TableRowView.extend({
    editViewClass: MasterEditRowView
});

var MastersTableView = TableView.extend({
    rowViewClass: MasterTableRowView,
    defaultPageLength: 0,

    columns: [
        { id: "nickname", title: "Nickname" },
        { id: "fqdn", renderFn: "renderLink", title: "Link" },
        { id: "pb_port", title: "PB Port" },
        { id: "datacenter", title: "DC" },
        { id: "pool", title: "Pool" },
        { id: "notes", title: "Notes", },
        { id: "enabled", renderFn: 'renderEnabled',
          sClass: 'center', title: "Enab" }
    ],

    initialize: function(args) {
        this.model = window.masters;

        TableView.prototype.initialize.call(this, args);
    },

    renderLink: function(model) {
        var fqdn = model.get('fqdn');
        var port = model.get('http_port');

        // shorten the hostname if possible
        var hostname = fqdn.replace('build.mozilla.org', 'b.m.o');
        hostname = hostname.replace('mozilla.org', 'm.o');

        if (port === 0) {
            return hostname;
        }

        var a = $('<a/>', {
            href: 'http://' + fqdn + ':' + port,
            text: hostname + ':' + port
        });
        // wrap that in a div and extract the contents as a string
        return $('<div>').append(a).html();
    },

    renderEnabled: function (model) {
        if (model.get('enabled')) {
            return '&bull;';
        } else {
            return '';
        }
    }
});

//
// View Tabs
//

var ViewTabsView = Backbone.View.extend({
    initialize: function(args) {
        this.viewSpecs = args.viewSpecs;

        this.render = $.proxy(this, 'render');
    },

    render: function() {
        var el = $(this.el);

        $.each(this.viewSpecs, function (i, viewSpec) {
            // skip views without titles
            if (!viewSpec.title) {
                return;
            }
            var id = 'menu-' + viewSpec.name;
            var input = $('<input/>', {
                type: 'radio',
                name: 'viewmenu',
                id: id
            });
            input.click(function (elt) {
                window.location.hash = viewSpec.name;
            });

            el.append(input);
            el.append($('<label>', {
                'for': id,
                text: viewSpec.title
            }));
        });
        el.buttonset();

        return this;
    }
});

//
// Reports
// 

var ReportTableView = Backbone.View.extend({
    initialize: function(args) {
        this.refresh = $.proxy(this, 'refresh');
        this.render = $.proxy(this, 'render');
        this.makeReportData = $.proxy(this, 'makeReportData');
    },

    render: function() {
        // dataTable likes to add sibling nodes, so add the table within
        // the enclosing div, this.el
        $(this.el).append('<table class="display" ' +
            'cellspacing="0" cellpadding="0" border="0"></table>');

        // create an empty table with the right columns
        this.dataTable = this.$('table').dataTable({
            aoColumns: this.makeReportColumns(),
            bJQueryUI: true,
            bPaginate: false,
            bAutoWidth: false
        });

        this.refresh();

        return this;
    },

    refresh: function() {
        var data = this.makeReportData();

        // clear table without redrawing, then update with redraw
        this.dataTable.fnClearTable(false);
        this.dataTable.fnAddData(data, true);
    }
});

var SilosTableView = ReportTableView.extend({
    initialize: function(args) {
        ReportTableView.prototype.initialize.call(this, args);
        window.slaves.bind('refresh', this.refresh);
        window.slaves.bind('change', this.refresh);
    },

    makeData: function() {
        var pools = {};
        var silo_info = [];

        window.slaves.each(function (slave) {
            var silo_tuple = [
                slave.get('distro'), 
                slave.get('bitlength'),
                slave.get('speed'),
                slave.get('datacenter'),
                slave.get('trustlevel'),
                slave.get('environment'),
                slave.get('purpose')
            ];

            var silokey = silo_tuple.join(':');
            var this_silo;
            if (!silo_info[silokey]) {
                this_silo = silo_info[silokey] = {
                    'silo_tuple': silo_tuple,
                    'by_pool' : {}
                };
            } else {
                this_silo = silo_info[silokey];
            }

            var pool = slave.get('pool');
            this_silo.by_pool[pool] = (this_silo.by_pool[pool] || 0) + 1;

            pools[pool] = 1;
        });

        // now pools is a mapping of { pool : 1 } for all pools containing
        // >0 slaves and silo_info contains, for each silo, the display tuple
        // and a count of slaves in each pool by name (with 0's omitted)

        // convert to a sorted array
        var pool_names = _.keys(pools).sort();

        // make up the columns: silo components followed by pools
        var columns = [
            { sTitle: 'Distro' },
            { sTitle: 'Bits' },
            { sTitle: 'Speed' },
            { sTitle: 'DC' },
            { sTitle: 'Trust' },
            { sTitle: 'Env' },
            { sTitle: 'Purp' }
        ];
        columns = columns.concat(_.map(pool_names, function(pool) {
            return { sTitle: pool };
        }));

        this.reportColumns = columns;

        // make up the data
        var data = _.map(_.keys(silo_info), function(silo_name) {
            var silo = silo_info[silo_name];
            var row = silo.silo_tuple;
            row = row.concat(
                _.map(pool_names, function(pool) {
                    return String(silo.by_pool[pool] || '');
            }));
            return row;
        });

        this.reportData = data;
    },

    makeReportData: function() {
        this.makeData();
        return this.reportData;
    },

    makeReportColumns: function() {
        this.makeData();
        return this.reportColumns;
    }
});

//
// Dashboard
//

var DashboardView = Backbone.View.extend({
    tagName: 'div',
    className: 'dashboard',

    initialize: function(args) {
        this.render = $.proxy(this, 'render');
    },

    render: function() {
        $(this.el).load(window.slavealloc_base_url + 'ui/dashboard.html');
        return this;
    }
});

