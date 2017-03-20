SlaveallocController = Backbone.Router.extend({
    initialize: function(args) {
        this.activateRoute = $.proxy(this, 'activateRoute');

        viewSpecs = [
            { name: '', title: null, rendered: false,
              view_factory: function() { return new DashboardView(); } },
            { name: 'slaves', title: 'Slaves', rendered: false,
              view_factory: function() { return new SlavesTableView(); } },
            { name: 'masters', title: 'Masters', rendered: false,
              view_factory: function() { return new MastersTableView(); } },
            { name: 'silos', title: 'Silos', rendered: false,
              view_factory: function() { return new SilosTableView(); } }
        ];

        this.currentViewSpec = null;

        this.viewTabsView = new ViewTabsView({
            viewSpecs: viewSpecs,
            el: $('#viewtabs')
        });
        this.viewTabsView.render();

        var self = this;
        $.each(viewSpecs, function (i, viewSpec) {
            self.route(viewSpec.name, viewSpec.name,
                       function() { self.activateRoute(viewSpec); });
        });
    },

    activateRoute: function(viewSpec) {
        // done loading..
        $('.loading').remove();

        // render the new one, if necessary
        // TODO: spinner
        if (!viewSpec.rendered) {
            viewSpec.view = viewSpec.view_factory();
            viewSpec.view.render();
            $(viewSpec.view.el).hide();
            $('#viewcontent').append(viewSpec.view.el);
            viewSpec.rendered = true;
        }

        // hide the previous view from the DOM
        if (this.currentViewSpec) {
            $(this.currentViewSpec.view.el).hide();
        }

        // .. and show the new one
        $(viewSpec.view.el).show();

        // then record the updated current view
        this.currentViewSpec = viewSpec;
    }
});
