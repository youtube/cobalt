Resources.ApplicationCacheModel=class extends SDK.SDKModel{constructor(target){super(target);target.registerApplicationCacheDispatcher(new Resources.ApplicationCacheDispatcher(this));this._agent=target.applicationCacheAgent();this._agent.enable();var resourceTreeModel=target.model(SDK.ResourceTreeModel);resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.FrameNavigated,this._frameNavigated,this);resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.FrameDetached,this._frameDetached,this);this._statuses={};this._manifestURLsByFrame={};this._mainFrameNavigated();this._onLine=true;}
async _frameNavigated(event){var frame=(event.data);if(frame.isMainFrame()){this._mainFrameNavigated();return;}
var frameId=frame.id;var manifestURL=await this._agent.getManifestForFrame(frameId);if(manifestURL!==null&&!manifestURL)
this._frameManifestRemoved(frameId);}
_frameDetached(event){var frame=(event.data);this._frameManifestRemoved(frame.id);}
reset(){this._statuses={};this._manifestURLsByFrame={};this.dispatchEventToListeners(Resources.ApplicationCacheModel.Events.FrameManifestsReset);}
async _mainFrameNavigated(){var framesWithManifests=await this._agent.getFramesWithManifests();for(var frame of framesWithManifests||[])
this._frameManifestUpdated(frame.frameId,frame.manifestURL,frame.status);}
_frameManifestUpdated(frameId,manifestURL,status){if(status===applicationCache.UNCACHED){this._frameManifestRemoved(frameId);return;}
if(!manifestURL)
return;if(this._manifestURLsByFrame[frameId]&&manifestURL!==this._manifestURLsByFrame[frameId])
this._frameManifestRemoved(frameId);var statusChanged=this._statuses[frameId]!==status;this._statuses[frameId]=status;if(!this._manifestURLsByFrame[frameId]){this._manifestURLsByFrame[frameId]=manifestURL;this.dispatchEventToListeners(Resources.ApplicationCacheModel.Events.FrameManifestAdded,frameId);}
if(statusChanged)
this.dispatchEventToListeners(Resources.ApplicationCacheModel.Events.FrameManifestStatusUpdated,frameId);}
_frameManifestRemoved(frameId){if(!this._manifestURLsByFrame[frameId])
return;delete this._manifestURLsByFrame[frameId];delete this._statuses[frameId];this.dispatchEventToListeners(Resources.ApplicationCacheModel.Events.FrameManifestRemoved,frameId);}
frameManifestURL(frameId){return this._manifestURLsByFrame[frameId]||'';}
frameManifestStatus(frameId){return this._statuses[frameId]||applicationCache.UNCACHED;}
get onLine(){return this._onLine;}
_statusUpdated(frameId,manifestURL,status){this._frameManifestUpdated(frameId,manifestURL,status);}
requestApplicationCache(frameId){return this._agent.getApplicationCacheForFrame(frameId);}
_networkStateUpdated(isNowOnline){this._onLine=isNowOnline;this.dispatchEventToListeners(Resources.ApplicationCacheModel.Events.NetworkStateChanged,isNowOnline);}};SDK.SDKModel.register(Resources.ApplicationCacheModel,SDK.Target.Capability.DOM,false);Resources.ApplicationCacheModel.Events={FrameManifestStatusUpdated:Symbol('FrameManifestStatusUpdated'),FrameManifestAdded:Symbol('FrameManifestAdded'),FrameManifestRemoved:Symbol('FrameManifestRemoved'),FrameManifestsReset:Symbol('FrameManifestsReset'),NetworkStateChanged:Symbol('NetworkStateChanged')};Resources.ApplicationCacheDispatcher=class{constructor(applicationCacheModel){this._applicationCacheModel=applicationCacheModel;}
applicationCacheStatusUpdated(frameId,manifestURL,status){this._applicationCacheModel._statusUpdated(frameId,manifestURL,status);}
networkStateUpdated(isNowOnline){this._applicationCacheModel._networkStateUpdated(isNowOnline);}};;Resources.AppManifestView=class extends UI.VBox{constructor(){super(true);this.registerRequiredCSS('resources/appManifestView.css');this._emptyView=new UI.EmptyWidget(Common.UIString('No manifest detected'));var p=this._emptyView.appendParagraph();var linkElement=UI.createExternalLink('https://developers.google.com/web/fundamentals/engage-and-retain/web-app-manifest/?utm_source=devtools',Common.UIString('Read more about the web manifest'));p.appendChild(UI.formatLocalized('A web manifest allows you to control how your app behaves when launched and displayed to the user. %s',[linkElement]));this._emptyView.show(this.contentElement);this._emptyView.hideWidget();this._reportView=new UI.ReportView(Common.UIString('App Manifest'));this._reportView.show(this.contentElement);this._reportView.hideWidget();this._errorsSection=this._reportView.appendSection(Common.UIString('Errors and warnings'));this._identitySection=this._reportView.appendSection(Common.UIString('Identity'));var toolbar=this._identitySection.createToolbar();toolbar.renderAsLinks();var addToHomeScreen=new UI.ToolbarButton(Common.UIString('Add to homescreen'),undefined,Common.UIString('Add to homescreen'));addToHomeScreen.addEventListener(UI.ToolbarButton.Events.Click,this._addToHomescreen,this);toolbar.appendToolbarItem(addToHomeScreen);this._presentationSection=this._reportView.appendSection(Common.UIString('Presentation'));this._iconsSection=this._reportView.appendSection(Common.UIString('Icons'));this._nameField=this._identitySection.appendField(Common.UIString('Name'));this._shortNameField=this._identitySection.appendField(Common.UIString('Short name'));this._startURLField=this._presentationSection.appendField(Common.UIString('Start URL'));var themeColorField=this._presentationSection.appendField(Common.UIString('Theme color'));this._themeColorSwatch=InlineEditor.ColorSwatch.create();themeColorField.appendChild(this._themeColorSwatch);var backgroundColorField=this._presentationSection.appendField(Common.UIString('Background color'));this._backgroundColorSwatch=InlineEditor.ColorSwatch.create();backgroundColorField.appendChild(this._backgroundColorSwatch);this._orientationField=this._presentationSection.appendField(Common.UIString('Orientation'));this._displayField=this._presentationSection.appendField(Common.UIString('Display'));SDK.targetManager.observeModels(SDK.ResourceTreeModel,this);}
modelAdded(resourceTreeModel){if(this._resourceTreeModel)
return;this._resourceTreeModel=resourceTreeModel;this._updateManifest();resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.MainFrameNavigated,this._updateManifest,this);}
modelRemoved(resourceTreeModel){if(!this._resourceTreeModel||this._resourceTreeModel!==resourceTreeModel)
return;resourceTreeModel.removeEventListener(SDK.ResourceTreeModel.Events.MainFrameNavigated,this._updateManifest,this);delete this._resourceTreeModel;}
_updateManifest(){this._resourceTreeModel.fetchAppManifest(this._renderManifest.bind(this));}
_renderManifest(url,data,errors){if(!data&&!errors.length){this._emptyView.showWidget();this._reportView.hideWidget();return;}
this._emptyView.hideWidget();this._reportView.showWidget();this._reportView.setURL(Components.Linkifier.linkifyURL(url));this._errorsSection.clearContent();this._errorsSection.element.classList.toggle('hidden',!errors.length);for(var error of errors){this._errorsSection.appendRow().appendChild(UI.createLabel(error.message,error.critical?'smallicon-error':'smallicon-warning'));}
if(!data)
return;if(data.charCodeAt(0)===0xFEFF)
data=data.slice(1);var parsedManifest=JSON.parse(data);this._nameField.textContent=stringProperty('name');this._shortNameField.textContent=stringProperty('short_name');this._startURLField.removeChildren();var startURL=stringProperty('start_url');if(startURL){var completeURL=(Common.ParsedURL.completeURL(url,startURL));this._startURLField.appendChild(Components.Linkifier.linkifyURL(completeURL,{text:startURL}));}
this._themeColorSwatch.classList.toggle('hidden',!stringProperty('theme_color'));var themeColor=Common.Color.parse(stringProperty('theme_color')||'white')||Common.Color.parse('white');this._themeColorSwatch.setColor((themeColor));this._backgroundColorSwatch.classList.toggle('hidden',!stringProperty('background_color'));var backgroundColor=Common.Color.parse(stringProperty('background_color')||'white')||Common.Color.parse('white');this._backgroundColorSwatch.setColor((backgroundColor));this._orientationField.textContent=stringProperty('orientation');this._displayField.textContent=stringProperty('display');var icons=parsedManifest['icons']||[];this._iconsSection.clearContent();for(var icon of icons){var title=(icon['sizes']||'')+'\n'+(icon['type']||'');var field=this._iconsSection.appendField(title);var imageElement=field.createChild('img');imageElement.style.maxWidth='200px';imageElement.style.maxHeight='200px';imageElement.src=Common.ParsedURL.completeURL(url,icon['src']);}
function stringProperty(name){var value=parsedManifest[name];if(typeof value!=='string')
return'';return value;}}
_addToHomescreen(event){var target=SDK.targetManager.mainTarget();if(target&&target.hasBrowserCapability()){target.pageAgent().requestAppBanner();Common.console.show();}}};;Resources.ApplicationCacheItemsView=class extends UI.SimpleView{constructor(model,frameId){super(Common.UIString('AppCache'));this._model=model;this.element.classList.add('storage-view','table');this._deleteButton=new UI.ToolbarButton(Common.UIString('Delete'),'largeicon-delete');this._deleteButton.setVisible(false);this._deleteButton.addEventListener(UI.ToolbarButton.Events.Click,this._deleteButtonClicked,this);this._connectivityIcon=createElement('label','dt-icon-label');this._connectivityIcon.style.margin='0 2px 0 5px';this._statusIcon=createElement('label','dt-icon-label');this._statusIcon.style.margin='0 2px 0 5px';this._frameId=frameId;this._emptyWidget=new UI.EmptyWidget(Common.UIString('No Application Cache information available.'));this._emptyWidget.show(this.element);this._markDirty();var status=this._model.frameManifestStatus(frameId);this.updateStatus(status);this.updateNetworkState(this._model.onLine);this._deleteButton.element.style.display='none';}
syncToolbarItems(){return[this._deleteButton,new UI.ToolbarItem(this._connectivityIcon),new UI.ToolbarSeparator(),new UI.ToolbarItem(this._statusIcon)];}
wasShown(){this._maybeUpdate();}
willHide(){this._deleteButton.setVisible(false);}
_maybeUpdate(){if(!this.isShowing()||!this._viewDirty)
return;this._update();this._viewDirty=false;}
_markDirty(){this._viewDirty=true;}
updateStatus(status){var oldStatus=this._status;this._status=status;var statusInformation={};statusInformation[applicationCache.UNCACHED]={type:'smallicon-red-ball',text:'UNCACHED'};statusInformation[applicationCache.IDLE]={type:'smallicon-green-ball',text:'IDLE'};statusInformation[applicationCache.CHECKING]={type:'smallicon-orange-ball',text:'CHECKING'};statusInformation[applicationCache.DOWNLOADING]={type:'smallicon-orange-ball',text:'DOWNLOADING'};statusInformation[applicationCache.UPDATEREADY]={type:'smallicon-green-ball',text:'UPDATEREADY'};statusInformation[applicationCache.OBSOLETE]={type:'smallicon-red-ball',text:'OBSOLETE'};var info=statusInformation[status]||statusInformation[applicationCache.UNCACHED];this._statusIcon.type=info.type;this._statusIcon.textContent=info.text;if(this.isShowing()&&this._status===applicationCache.IDLE&&(oldStatus===applicationCache.UPDATEREADY||!this._resources))
this._markDirty();this._maybeUpdate();}
updateNetworkState(isNowOnline){if(isNowOnline){this._connectivityIcon.type='smallicon-green-ball';this._connectivityIcon.textContent=Common.UIString('Online');}else{this._connectivityIcon.type='smallicon-red-ball';this._connectivityIcon.textContent=Common.UIString('Offline');}}
async _update(){var applicationCache=await this._model.requestApplicationCache(this._frameId);if(!applicationCache||!applicationCache.manifestURL){delete this._manifest;delete this._creationTime;delete this._updateTime;delete this._size;delete this._resources;this._emptyWidget.show(this.element);this._deleteButton.setVisible(false);if(this._dataGrid)
this._dataGrid.element.classList.add('hidden');return;}
this._manifest=applicationCache.manifestURL;this._creationTime=applicationCache.creationTime;this._updateTime=applicationCache.updateTime;this._size=applicationCache.size;this._resources=applicationCache.resources;if(!this._dataGrid)
this._createDataGrid();this._populateDataGrid();this._dataGrid.autoSizeColumns(20,80);this._dataGrid.element.classList.remove('hidden');this._emptyWidget.detach();this._deleteButton.setVisible(true);}
_createDataGrid(){var columns=([{id:'resource',title:Common.UIString('Resource'),sort:DataGrid.DataGrid.Order.Ascending,sortable:true},{id:'type',title:Common.UIString('Type'),sortable:true},{id:'size',title:Common.UIString('Size'),align:DataGrid.DataGrid.Align.Right,sortable:true}]);this._dataGrid=new DataGrid.DataGrid(columns);this._dataGrid.setStriped(true);this._dataGrid.asWidget().show(this.element);this._dataGrid.addEventListener(DataGrid.DataGrid.Events.SortingChanged,this._populateDataGrid,this);}
_populateDataGrid(){var selectedResource=this._dataGrid.selectedNode?this._dataGrid.selectedNode.resource:null;var sortDirection=this._dataGrid.isSortOrderAscending()?1:-1;function numberCompare(field,resource1,resource2){return sortDirection*(resource1[field]-resource2[field]);}
function localeCompare(field,resource1,resource2){return sortDirection*(resource1[field]+'').localeCompare(resource2[field]+'');}
var comparator;switch(this._dataGrid.sortColumnId()){case'resource':comparator=localeCompare.bind(null,'url');break;case'type':comparator=localeCompare.bind(null,'type');break;case'size':comparator=numberCompare.bind(null,'size');break;default:localeCompare.bind(null,'resource');}
this._resources.sort(comparator);this._dataGrid.rootNode().removeChildren();var nodeToSelect;for(var i=0;i<this._resources.length;++i){var data={};var resource=this._resources[i];data.resource=resource.url;data.type=resource.type;data.size=Number.bytesToString(resource.size);var node=new DataGrid.DataGridNode(data);node.resource=resource;node.selectable=true;this._dataGrid.rootNode().appendChild(node);if(resource===selectedResource){nodeToSelect=node;nodeToSelect.selected=true;}}
if(!nodeToSelect&&this._dataGrid.rootNode().children.length)
this._dataGrid.rootNode().children[0].selected=true;}
_deleteButtonClicked(event){if(!this._dataGrid||!this._dataGrid.selectedNode)
return;this._deleteCallback(this._dataGrid.selectedNode);}
_deleteCallback(node){}};;Resources.ClearStorageView=class extends UI.ThrottledWidget{constructor(){super(true,1000);var types=Protocol.Storage.StorageType;this._pieColors=new Map([[types.Appcache,'rgb(110, 161, 226)'],[types.Cache_storage,'rgb(229, 113, 113)'],[types.Cookies,'rgb(239, 196, 87)'],[types.Indexeddb,'rgb(155, 127, 230)'],[types.Local_storage,'rgb(116, 178, 102)'],[types.Service_workers,'rgb(255, 167, 36)'],[types.Websql,'rgb(203, 220, 56)'],]);this._reportView=new UI.ReportView(Common.UIString('Clear storage'));this._reportView.registerRequiredCSS('resources/clearStorageView.css');this._reportView.element.classList.add('clear-storage-header');this._reportView.show(this.contentElement);this._target=null;this._securityOrigin=null;this._settings=new Map();for(var type of
[types.Appcache,types.Cache_storage,types.Cookies,types.Indexeddb,types.Local_storage,types.Service_workers,types.Websql])
this._settings.set(type,Common.settings.createSetting('clear-storage-'+type,true));var quota=this._reportView.appendSection(Common.UIString('Usage'));this._quotaRow=quota.appendRow();this._quotaUsage=null;this._pieChart=new PerfUI.PieChart(110,Number.bytesToString,true);this._pieChartLegend=createElement('div');var usageBreakdownRow=quota.appendRow();usageBreakdownRow.classList.add('usage-breakdown-row');usageBreakdownRow.appendChild(this._pieChart.element);usageBreakdownRow.appendChild(this._pieChartLegend);var application=this._reportView.appendSection(Common.UIString('Application'));this._appendItem(application,Common.UIString('Unregister service workers'),'service_workers');var storage=this._reportView.appendSection(Common.UIString('Storage'));this._appendItem(storage,Common.UIString('Local and session storage'),'local_storage');this._appendItem(storage,Common.UIString('IndexedDB'),'indexeddb');this._appendItem(storage,Common.UIString('Web SQL'),'websql');this._appendItem(storage,Common.UIString('Cookies'),'cookies');var caches=this._reportView.appendSection(Common.UIString('Cache'));this._appendItem(caches,Common.UIString('Cache storage'),'cache_storage');this._appendItem(caches,Common.UIString('Application cache'),'appcache');SDK.targetManager.observeTargets(this,SDK.Target.Capability.Browser);var footer=this._reportView.appendSection('','clear-storage-button').appendRow();this._clearButton=UI.createTextButton(Common.UIString('Clear site data'),this._clear.bind(this),Common.UIString('Clear site data'));footer.appendChild(this._clearButton);}
_appendItem(section,title,settingName){var row=section.appendRow();row.appendChild(UI.SettingsUI.createSettingCheckbox(title,this._settings.get(settingName),true));}
targetAdded(target){if(this._target)
return;this._target=target;var securityOriginManager=target.model(SDK.SecurityOriginManager);this._updateOrigin(securityOriginManager.mainSecurityOrigin());securityOriginManager.addEventListener(SDK.SecurityOriginManager.Events.MainSecurityOriginChanged,this._originChanged,this);}
targetRemoved(target){if(this._target!==target)
return;var securityOriginManager=target.model(SDK.SecurityOriginManager);securityOriginManager.removeEventListener(SDK.SecurityOriginManager.Events.MainSecurityOriginChanged,this._originChanged,this);}
_originChanged(event){var origin=(event.data);this._updateOrigin(origin);}
_updateOrigin(url){this._securityOrigin=new Common.ParsedURL(url).securityOrigin();this._reportView.setSubtitle(this._securityOrigin);this.doUpdate();}
_clear(){if(!this._securityOrigin)
return;var storageTypes=[];for(var type of this._settings.keys()){if(this._settings.get(type).get())
storageTypes.push(type);}
this._target.storageAgent().clearDataForOrigin(this._securityOrigin,storageTypes.join(','));var set=new Set(storageTypes);var hasAll=set.has(Protocol.Storage.StorageType.All);if(set.has(Protocol.Storage.StorageType.Cookies)||hasAll){var cookieModel=this._target.model(SDK.CookieModel);if(cookieModel)
cookieModel.clear();}
if(set.has(Protocol.Storage.StorageType.Indexeddb)||hasAll){for(var target of SDK.targetManager.targets()){var indexedDBModel=target.model(Resources.IndexedDBModel);if(indexedDBModel)
indexedDBModel.clearForOrigin(this._securityOrigin);}}
if(set.has(Protocol.Storage.StorageType.Local_storage)||hasAll){var storageModel=this._target.model(Resources.DOMStorageModel);if(storageModel)
storageModel.clearForOrigin(this._securityOrigin);}
if(set.has(Protocol.Storage.StorageType.Websql)||hasAll){var databaseModel=this._target.model(Resources.DatabaseModel);if(databaseModel){databaseModel.disable();databaseModel.enable();}}
if(set.has(Protocol.Storage.StorageType.Cache_storage)||hasAll){var target=SDK.targetManager.mainTarget();var model=target&&target.model(SDK.ServiceWorkerCacheModel);if(model)
model.clearForOrigin(this._securityOrigin);}
if(set.has(Protocol.Storage.StorageType.Appcache)||hasAll){var appcacheModel=this._target.model(Resources.ApplicationCacheModel);if(appcacheModel)
appcacheModel.reset();}
this._clearButton.disabled=true;var label=this._clearButton.textContent;this._clearButton.textContent=Common.UIString('Clearing...');setTimeout(()=>{this._clearButton.disabled=false;this._clearButton.textContent=label;},500);}
async doUpdate(){if(!this._securityOrigin)
return;var securityOrigin=(this._securityOrigin);var response=await this._target.storageAgent().invoke_getUsageAndQuota({origin:securityOrigin});if(response[Protocol.Error]){this._quotaRow.textContent='';this._resetPieChart(0);return;}
this._quotaRow.textContent=Common.UIString('%s used out of %s storage quota',Number.bytesToString(response.usage),Number.bytesToString(response.quota));if(!this._quotaUsage||this._quotaUsage!==response.usage){this._quotaUsage=response.usage;this._resetPieChart(response.usage);for(var usageForType of response.usageBreakdown.sort((a,b)=>b.usage-a.usage)){var value=usageForType.usage;if(!value)
continue;var title=this._getStorageTypeName(usageForType.storageType);var color=this._pieColors.get(usageForType.storageType)||'#ccc';this._pieChart.addSlice(value,color);var rowElement=this._pieChartLegend.createChild('div','usage-breakdown-legend-row');rowElement.createChild('span','usage-breakdown-legend-value').textContent=Number.bytesToString(value);rowElement.createChild('span','usage-breakdown-legend-swatch').style.backgroundColor=color;rowElement.createChild('span','usage-breakdown-legend-title').textContent=title;}}
this._usageUpdatedForTest(response.usage,response.quota,response.usageBreakdown);this.update();}
_resetPieChart(total){this._pieChart.setTotal(total);this._pieChartLegend.removeChildren();}
_getStorageTypeName(type){switch(type){case Protocol.Storage.StorageType.File_systems:return Common.UIString('File System');case Protocol.Storage.StorageType.Websql:return Common.UIString('Web SQL');case Protocol.Storage.StorageType.Appcache:return Common.UIString('Application Cache');case Protocol.Storage.StorageType.Indexeddb:return Common.UIString('IndexedDB');case Protocol.Storage.StorageType.Cache_storage:return Common.UIString('Cache Storage');case Protocol.Storage.StorageType.Service_workers:return Common.UIString('Service Workers');default:return Common.UIString('Other');}}
_usageUpdatedForTest(usage,quota,usageBreakdown){}};;Resources.StorageItemsView=class extends UI.VBox{constructor(title,filterName){super(false);this._filterRegex=null;this._deleteAllButton=this._addButton(Common.UIString('Clear All'),'largeicon-clear',this.deleteAllItems);this._deleteSelectedButton=this._addButton(Common.UIString('Delete Selected'),'largeicon-delete',this.deleteSelectedItem);this._refreshButton=this._addButton(Common.UIString('Refresh'),'largeicon-refresh',this.refreshItems);this._mainToolbar=new UI.Toolbar('top-resources-toolbar',this.element);this._filterItem=new UI.ToolbarInput(Common.UIString('Filter'),0.4);this._filterItem.addEventListener(UI.ToolbarInput.Event.TextChanged,this._filterChanged,this);var toolbarItems=[this._refreshButton,this._deleteAllButton,this._deleteSelectedButton,this._filterItem];for(var item of toolbarItems)
this._mainToolbar.appendToolbarItem(item);this.element.addEventListener('contextmenu',this._showContextMenu.bind(this),true);}
_addButton(label,glyph,callback){var button=new UI.ToolbarButton(label,glyph);button.addEventListener(UI.ToolbarButton.Events.Click,callback,this);return button;}
_showContextMenu(event){var contextMenu=new UI.ContextMenu(event);contextMenu.defaultSection().appendItem(Common.UIString('Refresh'),this.refreshItems.bind(this));contextMenu.show();}
_filterChanged(event){var text=(event.data);this._filterRegex=text?new RegExp(text.escapeForRegExp(),'i'):null;this.refreshItems();}
filter(items,keyFunction){if(!this._filterRegex)
return items;return items.filter(item=>this._filterRegex.test(keyFunction(item)));}
wasShown(){this.refreshItems();}
setCanDeleteAll(enabled){this._deleteAllButton.setEnabled(enabled);}
setCanDeleteSelected(enabled){this._deleteSelectedButton.setEnabled(enabled);}
setCanRefresh(enabled){this._refreshButton.setEnabled(enabled);}
setCanFilter(enabled){this._filterItem.setEnabled(enabled);}
deleteAllItems(){}
deleteSelectedItem(){}
refreshItems(){}};;Resources.CookieItemsView=class extends Resources.StorageItemsView{constructor(model,cookieDomain){super(Common.UIString('Cookies'),'cookiesPanel');this.element.classList.add('storage-view');this._model=model;this._cookieDomain=cookieDomain;this._totalSize=0;this._cookiesTable=null;this._refreshThrottler=new Common.Throttler(300);this._eventDescriptors=[];this.setCookiesDomain(model,cookieDomain);}
setCookiesDomain(model,domain){this._model=model;this._cookieDomain=domain;this.refreshItems();Common.EventTarget.removeEventListeners(this._eventDescriptors);var networkManager=model.target().model(SDK.NetworkManager);this._eventDescriptors=[networkManager.addEventListener(SDK.NetworkManager.Events.ResponseReceived,this._onResponseReceived,this)];}
_saveCookie(newCookie,oldCookie){if(!this._model)
return Promise.resolve(false);if(oldCookie&&(newCookie.name()!==oldCookie.name()||newCookie.url()!==oldCookie.url()))
this._model.deleteCookie(oldCookie);return this._model.saveCookie(newCookie);}
_deleteCookie(cookie,callback){this._model.deleteCookie(cookie,callback);}
_updateWithCookies(allCookies){this._totalSize=allCookies.reduce((size,cookie)=>size+cookie.size(),0);if(!this._cookiesTable){this._cookiesTable=new CookieTable.CookiesTable(this._saveCookie.bind(this),this.refreshItems.bind(this),()=>this.setCanDeleteSelected(!!this._cookiesTable.selectedCookie()),this._deleteCookie.bind(this));}
const parsedURL=this._cookieDomain.asParsedURL();const host=parsedURL?parsedURL.host:'';this._cookiesTable.setCookieDomain(host);var shownCookies=this.filter(allCookies,cookie=>`${cookie.name()} ${cookie.value()} ${cookie.domain()}`);this._cookiesTable.setCookies(shownCookies);this._cookiesTable.show(this.element);this.setCanFilter(true);this.setCanDeleteAll(true);this.setCanDeleteSelected(!!this._cookiesTable.selectedCookie());}
deleteAllItems(){this._model.clear(this._cookieDomain,()=>this.refreshItems());}
deleteSelectedItem(){var selectedCookie=this._cookiesTable.selectedCookie();if(selectedCookie)
this._model.deleteCookie(selectedCookie,()=>this.refreshItems());}
refreshItems(){this._model.getCookiesForDomain(this._cookieDomain).then(this._updateWithCookies.bind(this));}
_onResponseReceived(){this._refreshThrottler.schedule(()=>Promise.resolve(this.refreshItems()));}};;Resources.Database=class{constructor(model,id,domain,name,version){this._model=model;this._id=id;this._domain=domain;this._name=name;this._version=version;}
get id(){return this._id;}
get name(){return this._name;}
set name(x){this._name=x;}
get version(){return this._version;}
set version(x){this._version=x;}
get domain(){return this._domain;}
set domain(x){this._domain=x;}
async tableNames(){var names=await this._model._agent.getDatabaseTableNames(this._id)||[];return names.sort();}
async executeSql(query,onSuccess,onError){var response=await this._model._agent.invoke_executeSQL({'databaseId':this._id,'query':query});var error=response[Protocol.Error];if(error){onError(error);return;}
var sqlError=response.sqlError;if(!sqlError){onSuccess(response.columnNames,response.values);return;}
var message;if(sqlError.message)
message=sqlError.message;else if(sqlError.code===2)
message=Common.UIString('Database no longer has expected version.');else
message=Common.UIString('An unexpected error %s occurred.',sqlError.code);onError(message);}};Resources.DatabaseModel=class extends SDK.SDKModel{constructor(target){super(target);this._databases=[];this._agent=target.databaseAgent();this.target().registerDatabaseDispatcher(new Resources.DatabaseDispatcher(this));}
enable(){if(this._enabled)
return;this._agent.enable();this._enabled=true;}
disable(){if(!this._enabled)
return;this._enabled=false;this._databases=[];this._agent.disable();this.dispatchEventToListeners(Resources.DatabaseModel.Events.DatabasesRemoved);}
databases(){var result=[];for(var database of this._databases)
result.push(database);return result;}
_addDatabase(database){this._databases.push(database);this.dispatchEventToListeners(Resources.DatabaseModel.Events.DatabaseAdded,database);}};SDK.SDKModel.register(Resources.DatabaseModel,SDK.Target.Capability.None,false);Resources.DatabaseModel.Events={DatabaseAdded:Symbol('DatabaseAdded'),DatabasesRemoved:Symbol('DatabasesRemoved'),};Resources.DatabaseDispatcher=class{constructor(model){this._model=model;}
addDatabase(payload){this._model._addDatabase(new Resources.Database(this._model,payload.id,payload.domain,payload.name,payload.version));}};Resources.DatabaseModel._symbol=Symbol('DatabaseModel');;Resources.DOMStorage=class extends Common.Object{constructor(model,securityOrigin,isLocalStorage){super();this._model=model;this._securityOrigin=securityOrigin;this._isLocalStorage=isLocalStorage;}
static storageId(securityOrigin,isLocalStorage){return{securityOrigin:securityOrigin,isLocalStorage:isLocalStorage};}
get id(){return Resources.DOMStorage.storageId(this._securityOrigin,this._isLocalStorage);}
get securityOrigin(){return this._securityOrigin;}
get isLocalStorage(){return this._isLocalStorage;}
getItems(){return this._model._agent.getDOMStorageItems(this.id);}
setItem(key,value){this._model._agent.setDOMStorageItem(this.id,key,value);}
removeItem(key){this._model._agent.removeDOMStorageItem(this.id,key);}
clear(){this._model._agent.clear(this.id);}};Resources.DOMStorage.Events={DOMStorageItemsCleared:Symbol('DOMStorageItemsCleared'),DOMStorageItemRemoved:Symbol('DOMStorageItemRemoved'),DOMStorageItemAdded:Symbol('DOMStorageItemAdded'),DOMStorageItemUpdated:Symbol('DOMStorageItemUpdated')};Resources.DOMStorageModel=class extends SDK.SDKModel{constructor(target){super(target);this._securityOriginManager=target.model(SDK.SecurityOriginManager);this._storages={};this._agent=target.domstorageAgent();}
enable(){if(this._enabled)
return;this.target().registerDOMStorageDispatcher(new Resources.DOMStorageDispatcher(this));this._securityOriginManager.addEventListener(SDK.SecurityOriginManager.Events.SecurityOriginAdded,this._securityOriginAdded,this);this._securityOriginManager.addEventListener(SDK.SecurityOriginManager.Events.SecurityOriginRemoved,this._securityOriginRemoved,this);for(var securityOrigin of this._securityOriginManager.securityOrigins())
this._addOrigin(securityOrigin);this._agent.enable();this._enabled=true;}
clearForOrigin(origin){if(!this._enabled)
return;for(var isLocal of[true,false]){var key=this._storageKey(origin,isLocal);var storage=this._storages[key];storage.clear();}
this._removeOrigin(origin);this._addOrigin(origin);}
_securityOriginAdded(event){this._addOrigin((event.data));}
_addOrigin(securityOrigin){for(var isLocal of[true,false]){var key=this._storageKey(securityOrigin,isLocal);console.assert(!this._storages[key]);var storage=new Resources.DOMStorage(this,securityOrigin,isLocal);this._storages[key]=storage;this.dispatchEventToListeners(Resources.DOMStorageModel.Events.DOMStorageAdded,storage);}}
_securityOriginRemoved(event){this._removeOrigin((event.data));}
_removeOrigin(securityOrigin){for(var isLocal of[true,false]){var key=this._storageKey(securityOrigin,isLocal);var storage=this._storages[key];console.assert(storage);delete this._storages[key];this.dispatchEventToListeners(Resources.DOMStorageModel.Events.DOMStorageRemoved,storage);}}
_storageKey(securityOrigin,isLocalStorage){return JSON.stringify(Resources.DOMStorage.storageId(securityOrigin,isLocalStorage));}
_domStorageItemsCleared(storageId){var domStorage=this.storageForId(storageId);if(!domStorage)
return;var eventData={};domStorage.dispatchEventToListeners(Resources.DOMStorage.Events.DOMStorageItemsCleared,eventData);}
_domStorageItemRemoved(storageId,key){var domStorage=this.storageForId(storageId);if(!domStorage)
return;var eventData={key:key};domStorage.dispatchEventToListeners(Resources.DOMStorage.Events.DOMStorageItemRemoved,eventData);}
_domStorageItemAdded(storageId,key,value){var domStorage=this.storageForId(storageId);if(!domStorage)
return;var eventData={key:key,value:value};domStorage.dispatchEventToListeners(Resources.DOMStorage.Events.DOMStorageItemAdded,eventData);}
_domStorageItemUpdated(storageId,key,oldValue,value){var domStorage=this.storageForId(storageId);if(!domStorage)
return;var eventData={key:key,oldValue:oldValue,value:value};domStorage.dispatchEventToListeners(Resources.DOMStorage.Events.DOMStorageItemUpdated,eventData);}
storageForId(storageId){return this._storages[JSON.stringify(storageId)];}
storages(){var result=[];for(var id in this._storages)
result.push(this._storages[id]);return result;}};SDK.SDKModel.register(Resources.DOMStorageModel,SDK.Target.Capability.None,false);Resources.DOMStorageModel.Events={DOMStorageAdded:Symbol('DOMStorageAdded'),DOMStorageRemoved:Symbol('DOMStorageRemoved')};Resources.DOMStorageDispatcher=class{constructor(model){this._model=model;}
domStorageItemsCleared(storageId){this._model._domStorageItemsCleared(storageId);}
domStorageItemRemoved(storageId,key){this._model._domStorageItemRemoved(storageId,key);}
domStorageItemAdded(storageId,key,value){this._model._domStorageItemAdded(storageId,key,value);}
domStorageItemUpdated(storageId,key,oldValue,value){this._model._domStorageItemUpdated(storageId,key,oldValue,value);}};Resources.DOMStorageModel._symbol=Symbol('DomStorage');;Resources.DOMStorageItemsView=class extends Resources.StorageItemsView{constructor(domStorage){super(Common.UIString('DOM Storage'),'domStoragePanel');this._domStorage=domStorage;this.element.classList.add('storage-view','table');var columns=([{id:'key',title:Common.UIString('Key'),sortable:false,editable:true,longText:true,weight:50},{id:'value',title:Common.UIString('Value'),sortable:false,editable:true,longText:true,weight:50}]);this._dataGrid=new DataGrid.DataGrid(columns,this._editingCallback.bind(this),this._deleteCallback.bind(this));this._dataGrid.setStriped(true);this._dataGrid.setName('DOMStorageItemsView');this._dataGrid.asWidget().show(this.element);this._eventListeners=[];this.setStorage(domStorage);}
setStorage(domStorage){Common.EventTarget.removeEventListeners(this._eventListeners);this._domStorage=domStorage;this._eventListeners=[this._domStorage.addEventListener(Resources.DOMStorage.Events.DOMStorageItemsCleared,this._domStorageItemsCleared,this),this._domStorage.addEventListener(Resources.DOMStorage.Events.DOMStorageItemRemoved,this._domStorageItemRemoved,this),this._domStorage.addEventListener(Resources.DOMStorage.Events.DOMStorageItemAdded,this._domStorageItemAdded,this),this._domStorage.addEventListener(Resources.DOMStorage.Events.DOMStorageItemUpdated,this._domStorageItemUpdated,this),];this.refreshItems();}
_domStorageItemsCleared(){if(!this.isShowing()||!this._dataGrid)
return;this._dataGrid.rootNode().removeChildren();this._dataGrid.addCreationNode(false);this.setCanDeleteSelected(false);}
_domStorageItemRemoved(event){if(!this.isShowing()||!this._dataGrid)
return;var storageData=event.data;var rootNode=this._dataGrid.rootNode();var children=rootNode.children;for(var i=0;i<children.length;++i){var childNode=children[i];if(childNode.data.key===storageData.key){rootNode.removeChild(childNode);this.setCanDeleteSelected(children.length>1);return;}}}
_domStorageItemAdded(event){if(!this.isShowing()||!this._dataGrid)
return;var storageData=event.data;var rootNode=this._dataGrid.rootNode();var children=rootNode.children;this.setCanDeleteSelected(true);for(var i=0;i<children.length;++i){if(children[i].data.key===storageData.key)
return;}
var childNode=new DataGrid.DataGridNode({key:storageData.key,value:storageData.value},false);rootNode.insertChild(childNode,children.length-1);}
_domStorageItemUpdated(event){if(!this.isShowing()||!this._dataGrid)
return;var storageData=event.data;var rootNode=this._dataGrid.rootNode();var children=rootNode.children;var keyFound=false;for(var i=0;i<children.length;++i){var childNode=children[i];if(childNode.data.key===storageData.key){if(keyFound){rootNode.removeChild(childNode);return;}
keyFound=true;if(childNode.data.value!==storageData.value){childNode.data.value=storageData.value;childNode.refresh();childNode.select();childNode.reveal();}
this.setCanDeleteSelected(true);}}}
_showDOMStorageItems(items){var rootNode=this._dataGrid.rootNode();var selectedKey=null;for(var node of rootNode.children){if(!node.selected)
continue;selectedKey=node.data.key;break;}
rootNode.removeChildren();var selectedNode=null;var filteredItems=item=>`${item[0]} ${item[1]}`;for(var item of this.filter(items,filteredItems)){var key=item[0];var value=item[1];var node=new DataGrid.DataGridNode({key:key,value:value},false);node.selectable=true;rootNode.appendChild(node);if(!selectedNode||key===selectedKey)
selectedNode=node;}
if(selectedNode)
selectedNode.selected=true;this._dataGrid.addCreationNode(false);this.setCanDeleteSelected(!!selectedNode);}
deleteSelectedItem(){if(!this._dataGrid||!this._dataGrid.selectedNode)
return;this._deleteCallback(this._dataGrid.selectedNode);}
refreshItems(){this._domStorage.getItems().then(items=>items&&this._showDOMStorageItems(items));}
deleteAllItems(){this._domStorage.clear();this._domStorageItemsCleared();}
_editingCallback(editingNode,columnIdentifier,oldText,newText){var domStorage=this._domStorage;if(columnIdentifier==='key'){if(typeof oldText==='string')
domStorage.removeItem(oldText);domStorage.setItem(newText,editingNode.data.value||'');this._removeDupes(editingNode);}else{domStorage.setItem(editingNode.data.key||'',newText);}}
_removeDupes(masterNode){var rootNode=this._dataGrid.rootNode();var children=rootNode.children;for(var i=children.length-1;i>=0;--i){var childNode=children[i];if((childNode.data.key===masterNode.data.key)&&(masterNode!==childNode))
rootNode.removeChild(childNode);}}
_deleteCallback(node){if(!node||node.isCreationNode)
return;if(this._domStorage)
this._domStorage.removeItem(node.data.key);}};;Resources.DatabaseQueryView=class extends UI.VBox{constructor(database){super();this.database=database;this.element.classList.add('storage-view','query','monospace');this.element.addEventListener('selectstart',this._selectStart.bind(this),false);this._promptContainer=this.element.createChild('div','database-query-prompt-container');this._promptContainer.appendChild(UI.Icon.create('smallicon-text-prompt','prompt-icon'));this._promptElement=this._promptContainer.createChild('div');this._promptElement.className='database-query-prompt';this._promptElement.addEventListener('keydown',this._promptKeyDown.bind(this),true);this._prompt=new UI.TextPrompt();this._prompt.initialize(this.completions.bind(this),' ');this._proxyElement=this._prompt.attach(this._promptElement);this.element.addEventListener('click',this._messagesClicked.bind(this),true);}
_messagesClicked(){if(!this._prompt.isCaretInsidePrompt()&&!this.element.hasSelection())
this._prompt.moveCaretToEndOfPrompt();}
async completions(expression,prefix,force){if(!prefix)
return[];prefix=prefix.toLowerCase();var tableNames=await this.database.tableNames();return tableNames.map(name=>name+' ').concat(Resources.DatabaseQueryView._SQL_BUILT_INS).filter(proposal=>proposal.toLowerCase().startsWith(prefix)).map(completion=>({text:completion}));}
_selectStart(event){if(this._selectionTimeout)
clearTimeout(this._selectionTimeout);this._prompt.clearAutocomplete();function moveBackIfOutside(){delete this._selectionTimeout;if(!this._prompt.isCaretInsidePrompt()&&!this.element.hasSelection())
this._prompt.moveCaretToEndOfPrompt();this._prompt.autoCompleteSoon();}
this._selectionTimeout=setTimeout(moveBackIfOutside.bind(this),100);}
_promptKeyDown(event){if(isEnterKey(event)){this._enterKeyPressed(event);return;}}
_enterKeyPressed(event){event.consume();this._prompt.clearAutocomplete();var query=this._prompt.text();if(!query.length)
return;this._prompt.setText('');this.database.executeSql(query,this._queryFinished.bind(this,query),this._queryError.bind(this,query));}
_queryFinished(query,columnNames,values){var dataGrid=DataGrid.SortableDataGrid.create(columnNames,values);var trimmedQuery=query.trim();if(dataGrid){dataGrid.setStriped(true);dataGrid.renderInline();this._appendViewQueryResult(trimmedQuery,dataGrid.asWidget());dataGrid.autoSizeColumns(5);}
if(trimmedQuery.match(/^create /i)||trimmedQuery.match(/^drop table /i))
this.dispatchEventToListeners(Resources.DatabaseQueryView.Events.SchemaUpdated,this.database);}
_queryError(query,errorMessage){this._appendErrorQueryResult(query,errorMessage);}
_appendViewQueryResult(query,view){var resultElement=this._appendQueryResult(query);view.show(resultElement);this._promptElement.scrollIntoView(false);}
_appendErrorQueryResult(query,errorText){var resultElement=this._appendQueryResult(query);resultElement.classList.add('error');resultElement.appendChild(UI.Icon.create('smallicon-error','prompt-icon'));resultElement.createTextChild(errorText);this._promptElement.scrollIntoView(false);}
_appendQueryResult(query){var element=createElement('div');element.className='database-user-query';element.appendChild(UI.Icon.create('smallicon-user-command','prompt-icon'));this.element.insertBefore(element,this._promptContainer);var commandTextElement=createElement('span');commandTextElement.className='database-query-text';commandTextElement.textContent=query;element.appendChild(commandTextElement);var resultElement=createElement('div');resultElement.className='database-query-result';element.appendChild(resultElement);return resultElement;}};Resources.DatabaseQueryView.Events={SchemaUpdated:Symbol('SchemaUpdated')};Resources.DatabaseQueryView._SQL_BUILT_INS=['SELECT ','FROM ','WHERE ','LIMIT ','DELETE FROM ','CREATE ','DROP ','TABLE ','INDEX ','UPDATE ','INSERT INTO ','VALUES ('];;Resources.DatabaseTableView=class extends UI.SimpleView{constructor(database,tableName){super(Common.UIString('Database'));this.database=database;this.tableName=tableName;this.element.classList.add('storage-view','table');this._visibleColumnsSetting=Common.settings.createSetting('databaseTableViewVisibleColumns',{});this.refreshButton=new UI.ToolbarButton(Common.UIString('Refresh'),'largeicon-refresh');this.refreshButton.addEventListener(UI.ToolbarButton.Events.Click,this._refreshButtonClicked,this);this._visibleColumnsInput=new UI.ToolbarInput(Common.UIString('Visible columns'),1);this._visibleColumnsInput.addEventListener(UI.ToolbarInput.Event.TextChanged,this._onVisibleColumnsChanged,this);}
wasShown(){this.update();}
syncToolbarItems(){return[this.refreshButton,this._visibleColumnsInput];}
_escapeTableName(tableName){return tableName.replace(/\"/g,'""');}
update(){this.database.executeSql('SELECT rowid, * FROM "'+this._escapeTableName(this.tableName)+'"',this._queryFinished.bind(this),this._queryError.bind(this));}
_queryFinished(columnNames,values){this.detachChildWidgets();this.element.removeChildren();this._dataGrid=DataGrid.SortableDataGrid.create(columnNames,values);this._visibleColumnsInput.setVisible(!!this._dataGrid);if(!this._dataGrid){this._emptyWidget=new UI.EmptyWidget(Common.UIString('The “%s”\ntable is empty.',this.tableName));this._emptyWidget.show(this.element);return;}
this._dataGrid.setStriped(true);this._dataGrid.asWidget().show(this.element);this._dataGrid.autoSizeColumns(5);this._columnsMap=new Map();for(var i=1;i<columnNames.length;++i)
this._columnsMap.set(columnNames[i],String(i));this._lastVisibleColumns='';var visibleColumnsText=this._visibleColumnsSetting.get()[this.tableName]||'';this._visibleColumnsInput.setValue(visibleColumnsText);this._onVisibleColumnsChanged();}
_onVisibleColumnsChanged(){if(!this._dataGrid)
return;var text=this._visibleColumnsInput.value();var parts=text.split(/[\s,]+/);var matches=new Set();var columnsVisibility={};columnsVisibility['0']=true;for(var i=0;i<parts.length;++i){var part=parts[i];if(this._columnsMap.has(part)){matches.add(part);columnsVisibility[this._columnsMap.get(part)]=true;}}
var newVisibleColumns=matches.valuesArray().sort().join(', ');if(newVisibleColumns.length===0){for(var v of this._columnsMap.values())
columnsVisibility[v]=true;}
if(newVisibleColumns===this._lastVisibleColumns)
return;var visibleColumnsRegistry=this._visibleColumnsSetting.get();visibleColumnsRegistry[this.tableName]=text;this._visibleColumnsSetting.set(visibleColumnsRegistry);this._dataGrid.setColumnsVisiblity(columnsVisibility);this._lastVisibleColumns=newVisibleColumns;}
_queryError(error){this.detachChildWidgets();this.element.removeChildren();var errorMsgElement=createElement('div');errorMsgElement.className='storage-table-error';errorMsgElement.textContent=Common.UIString('An error occurred trying to\nread the “%s” table.',this.tableName);this.element.appendChild(errorMsgElement);}
_refreshButtonClicked(event){this.update();}};;Resources.IndexedDBModel=class extends SDK.SDKModel{constructor(target){super(target);this._securityOriginManager=target.model(SDK.SecurityOriginManager);this._agent=target.indexedDBAgent();this._databases=new Map();this._databaseNamesBySecurityOrigin={};}
static keyFromIDBKey(idbKey){if(typeof(idbKey)==='undefined'||idbKey===null)
return undefined;var type;var key={};switch(typeof(idbKey)){case'number':key.number=idbKey;type=Resources.IndexedDBModel.KeyTypes.NumberType;break;case'string':key.string=idbKey;type=Resources.IndexedDBModel.KeyTypes.StringType;break;case'object':if(idbKey instanceof Date){key.date=idbKey.getTime();type=Resources.IndexedDBModel.KeyTypes.DateType;}else if(Array.isArray(idbKey)){key.array=[];for(var i=0;i<idbKey.length;++i)
key.array.push(Resources.IndexedDBModel.keyFromIDBKey(idbKey[i]));type=Resources.IndexedDBModel.KeyTypes.ArrayType;}
break;default:return undefined;}
key.type=(type);return key;}
static keyRangeFromIDBKeyRange(idbKeyRange){if(typeof idbKeyRange==='undefined'||idbKeyRange===null)
return null;var keyRange={};keyRange.lower=Resources.IndexedDBModel.keyFromIDBKey(idbKeyRange.lower);keyRange.upper=Resources.IndexedDBModel.keyFromIDBKey(idbKeyRange.upper);keyRange.lowerOpen=!!idbKeyRange.lowerOpen;keyRange.upperOpen=!!idbKeyRange.upperOpen;return keyRange;}
static idbKeyPathFromKeyPath(keyPath){var idbKeyPath;switch(keyPath.type){case Resources.IndexedDBModel.KeyPathTypes.NullType:idbKeyPath=null;break;case Resources.IndexedDBModel.KeyPathTypes.StringType:idbKeyPath=keyPath.string;break;case Resources.IndexedDBModel.KeyPathTypes.ArrayType:idbKeyPath=keyPath.array;break;}
return idbKeyPath;}
static keyPathStringFromIDBKeyPath(idbKeyPath){if(typeof idbKeyPath==='string')
return'"'+idbKeyPath+'"';if(idbKeyPath instanceof Array)
return'["'+idbKeyPath.join('", "')+'"]';return null;}
enable(){if(this._enabled)
return;this._agent.enable();this._securityOriginManager.addEventListener(SDK.SecurityOriginManager.Events.SecurityOriginAdded,this._securityOriginAdded,this);this._securityOriginManager.addEventListener(SDK.SecurityOriginManager.Events.SecurityOriginRemoved,this._securityOriginRemoved,this);for(var securityOrigin of this._securityOriginManager.securityOrigins())
this._addOrigin(securityOrigin);this._enabled=true;}
clearForOrigin(origin){if(!this._enabled)
return;this._removeOrigin(origin);this._addOrigin(origin);}
async deleteDatabase(databaseId){if(!this._enabled)
return;await this._agent.deleteDatabase(databaseId.securityOrigin,databaseId.name);this._loadDatabaseNames(databaseId.securityOrigin);}
async refreshDatabaseNames(){for(var securityOrigin in this._databaseNamesBySecurityOrigin)
await this._loadDatabaseNames(securityOrigin);this.dispatchEventToListeners(Resources.IndexedDBModel.Events.DatabaseNamesRefreshed);}
refreshDatabase(databaseId){this._loadDatabase(databaseId);}
clearObjectStore(databaseId,objectStoreName){return this._agent.clearObjectStore(databaseId.securityOrigin,databaseId.name,objectStoreName);}
_securityOriginAdded(event){var securityOrigin=(event.data);this._addOrigin(securityOrigin);}
_securityOriginRemoved(event){var securityOrigin=(event.data);this._removeOrigin(securityOrigin);}
_addOrigin(securityOrigin){console.assert(!this._databaseNamesBySecurityOrigin[securityOrigin]);this._databaseNamesBySecurityOrigin[securityOrigin]=[];this._loadDatabaseNames(securityOrigin);}
_removeOrigin(securityOrigin){console.assert(this._databaseNamesBySecurityOrigin[securityOrigin]);for(var i=0;i<this._databaseNamesBySecurityOrigin[securityOrigin].length;++i)
this._databaseRemoved(securityOrigin,this._databaseNamesBySecurityOrigin[securityOrigin][i]);delete this._databaseNamesBySecurityOrigin[securityOrigin];}
_updateOriginDatabaseNames(securityOrigin,databaseNames){var newDatabaseNames=new Set(databaseNames);var oldDatabaseNames=new Set(this._databaseNamesBySecurityOrigin[securityOrigin]);this._databaseNamesBySecurityOrigin[securityOrigin]=databaseNames;for(var databaseName of oldDatabaseNames){if(!newDatabaseNames.has(databaseName))
this._databaseRemoved(securityOrigin,databaseName);}
for(var databaseName of newDatabaseNames){if(!oldDatabaseNames.has(databaseName))
this._databaseAdded(securityOrigin,databaseName);}}
databases(){var result=[];for(var securityOrigin in this._databaseNamesBySecurityOrigin){var databaseNames=this._databaseNamesBySecurityOrigin[securityOrigin];for(var i=0;i<databaseNames.length;++i)
result.push(new Resources.IndexedDBModel.DatabaseId(securityOrigin,databaseNames[i]));}
return result;}
_databaseAdded(securityOrigin,databaseName){var databaseId=new Resources.IndexedDBModel.DatabaseId(securityOrigin,databaseName);this.dispatchEventToListeners(Resources.IndexedDBModel.Events.DatabaseAdded,{model:this,databaseId:databaseId});}
_databaseRemoved(securityOrigin,databaseName){var databaseId=new Resources.IndexedDBModel.DatabaseId(securityOrigin,databaseName);this.dispatchEventToListeners(Resources.IndexedDBModel.Events.DatabaseRemoved,{model:this,databaseId:databaseId});}
async _loadDatabaseNames(securityOrigin){var databaseNames=await this._agent.requestDatabaseNames(securityOrigin);if(!databaseNames)
return;if(!this._databaseNamesBySecurityOrigin[securityOrigin])
return;this._updateOriginDatabaseNames(securityOrigin,databaseNames);}
async _loadDatabase(databaseId){var databaseWithObjectStores=await this._agent.requestDatabase(databaseId.securityOrigin,databaseId.name);if(!databaseWithObjectStores)
return;if(!this._databaseNamesBySecurityOrigin[databaseId.securityOrigin])
return;var databaseModel=new Resources.IndexedDBModel.Database(databaseId,databaseWithObjectStores.version);this._databases.set(databaseId,databaseModel);for(var objectStore of databaseWithObjectStores.objectStores){var objectStoreIDBKeyPath=Resources.IndexedDBModel.idbKeyPathFromKeyPath(objectStore.keyPath);var objectStoreModel=new Resources.IndexedDBModel.ObjectStore(objectStore.name,objectStoreIDBKeyPath,objectStore.autoIncrement);for(var j=0;j<objectStore.indexes.length;++j){var index=objectStore.indexes[j];var indexIDBKeyPath=Resources.IndexedDBModel.idbKeyPathFromKeyPath(index.keyPath);var indexModel=new Resources.IndexedDBModel.Index(index.name,indexIDBKeyPath,index.unique,index.multiEntry);objectStoreModel.indexes[indexModel.name]=indexModel;}
databaseModel.objectStores[objectStoreModel.name]=objectStoreModel;}
this.dispatchEventToListeners(Resources.IndexedDBModel.Events.DatabaseLoaded,{model:this,database:databaseModel});}
loadObjectStoreData(databaseId,objectStoreName,idbKeyRange,skipCount,pageSize,callback){this._requestData(databaseId,databaseId.name,objectStoreName,'',idbKeyRange,skipCount,pageSize,callback);}
loadIndexData(databaseId,objectStoreName,indexName,idbKeyRange,skipCount,pageSize,callback){this._requestData(databaseId,databaseId.name,objectStoreName,indexName,idbKeyRange,skipCount,pageSize,callback);}
async _requestData(databaseId,databaseName,objectStoreName,indexName,idbKeyRange,skipCount,pageSize,callback){var keyRange=Resources.IndexedDBModel.keyRangeFromIDBKeyRange(idbKeyRange)||undefined;var response=await this._agent.invoke_requestData({securityOrigin:databaseId.securityOrigin,databaseName,objectStoreName,indexName,skipCount,pageSize,keyRange});if(response[Protocol.Error]){console.error('IndexedDBAgent error: '+response[Protocol.Error]);return;}
var runtimeModel=this.target().model(SDK.RuntimeModel);if(!runtimeModel||!this._databaseNamesBySecurityOrigin[databaseId.securityOrigin])
return;var dataEntries=response.objectStoreDataEntries;var entries=[];for(var dataEntry of dataEntries){var key=runtimeModel.createRemoteObject(dataEntry.key);var primaryKey=runtimeModel.createRemoteObject(dataEntry.primaryKey);var value=runtimeModel.createRemoteObject(dataEntry.value);entries.push(new Resources.IndexedDBModel.Entry(key,primaryKey,value));}
callback(entries,response.hasMore);}};SDK.SDKModel.register(Resources.IndexedDBModel,SDK.Target.Capability.None,false);Resources.IndexedDBModel.KeyTypes={NumberType:'number',StringType:'string',DateType:'date',ArrayType:'array'};Resources.IndexedDBModel.KeyPathTypes={NullType:'null',StringType:'string',ArrayType:'array'};Resources.IndexedDBModel.Events={DatabaseAdded:Symbol('DatabaseAdded'),DatabaseRemoved:Symbol('DatabaseRemoved'),DatabaseLoaded:Symbol('DatabaseLoaded'),DatabaseNamesRefreshed:Symbol('DatabaseNamesRefreshed')};Resources.IndexedDBModel.Entry=class{constructor(key,primaryKey,value){this.key=key;this.primaryKey=primaryKey;this.value=value;}};Resources.IndexedDBModel.DatabaseId=class{constructor(securityOrigin,name){this.securityOrigin=securityOrigin;this.name=name;}
equals(databaseId){return this.name===databaseId.name&&this.securityOrigin===databaseId.securityOrigin;}};Resources.IndexedDBModel.Database=class{constructor(databaseId,version){this.databaseId=databaseId;this.version=version;this.objectStores={};}};Resources.IndexedDBModel.ObjectStore=class{constructor(name,keyPath,autoIncrement){this.name=name;this.keyPath=keyPath;this.autoIncrement=autoIncrement;this.indexes={};}
get keyPathString(){return(Resources.IndexedDBModel.keyPathStringFromIDBKeyPath((this.keyPath)));}};Resources.IndexedDBModel.Index=class{constructor(name,keyPath,unique,multiEntry){this.name=name;this.keyPath=keyPath;this.unique=unique;this.multiEntry=multiEntry;}
get keyPathString(){return(Resources.IndexedDBModel.keyPathStringFromIDBKeyPath((this.keyPath)));}};;Resources.IDBDatabaseView=class extends UI.VBox{constructor(model,database){super();this._model=model;var databaseName=database?database.databaseId.name:Common.UIString('Loading\u2026');this._reportView=new UI.ReportView(databaseName);this._reportView.show(this.contentElement);var bodySection=this._reportView.appendSection('');this._securityOriginElement=bodySection.appendField(Common.UIString('Security origin'));this._versionElement=bodySection.appendField(Common.UIString('Version'));var footer=this._reportView.appendSection('').appendRow();this._clearButton=UI.createTextButton(Common.UIString('Delete database'),()=>this._deleteDatabase(),Common.UIString('Delete database'));footer.appendChild(this._clearButton);this._refreshButton=UI.createTextButton(Common.UIString('Refresh database'),()=>this._refreshDatabaseButtonClicked(),Common.UIString('Refresh database'));footer.appendChild(this._refreshButton);if(database)
this.update(database);}
_refreshDatabase(){this._securityOriginElement.textContent=this._database.databaseId.securityOrigin;this._versionElement.textContent=this._database.version;}
_refreshDatabaseButtonClicked(){this._model.refreshDatabase(this._database.databaseId);}
update(database){this._database=database;this._reportView.setTitle(this._database.databaseId.name);this._refreshDatabase();this._updatedForTests();}
_updatedForTests(){}
async _deleteDatabase(){var ok=await UI.ConfirmDialog.show(Common.UIString('Please confirm delete of "%s" database.',this._database.databaseId.name),this.element);if(ok)
this._model.deleteDatabase(this._database.databaseId);}};Resources.IDBDataView=class extends UI.SimpleView{constructor(model,databaseId,objectStore,index){super(Common.UIString('IDB'));this.registerRequiredCSS('resources/indexedDBViews.css');this._model=model;this._databaseId=databaseId;this._isIndex=!!index;this.element.classList.add('indexed-db-data-view');this._createEditorToolbar();this._refreshButton=new UI.ToolbarButton(Common.UIString('Refresh'),'largeicon-refresh');this._refreshButton.addEventListener(UI.ToolbarButton.Events.Click,this._refreshButtonClicked,this);this._clearButton=new UI.ToolbarButton(Common.UIString('Clear object store'),'largeicon-clear');this._clearButton.addEventListener(UI.ToolbarButton.Events.Click,this._clearButtonClicked,this);this._pageSize=50;this._skipCount=0;this.update(objectStore,index);this._entries=[];}
_createDataGrid(){var keyPath=this._isIndex?this._index.keyPath:this._objectStore.keyPath;var columns=([]);columns.push({id:'number',title:Common.UIString('#'),sortable:false,width:'50px'});columns.push({id:'key',titleDOMFragment:this._keyColumnHeaderFragment(Common.UIString('Key'),keyPath),sortable:false});if(this._isIndex){columns.push({id:'primaryKey',titleDOMFragment:this._keyColumnHeaderFragment(Common.UIString('Primary key'),this._objectStore.keyPath),sortable:false});}
columns.push({id:'value',title:Common.UIString('Value'),sortable:false});var dataGrid=new DataGrid.DataGrid(columns);dataGrid.setStriped(true);return dataGrid;}
_keyColumnHeaderFragment(prefix,keyPath){var keyColumnHeaderFragment=createDocumentFragment();keyColumnHeaderFragment.createTextChild(prefix);if(keyPath===null)
return keyColumnHeaderFragment;keyColumnHeaderFragment.createTextChild(' ('+Common.UIString('Key path: '));if(Array.isArray(keyPath)){keyColumnHeaderFragment.createTextChild('[');for(var i=0;i<keyPath.length;++i){if(i!==0)
keyColumnHeaderFragment.createTextChild(', ');keyColumnHeaderFragment.appendChild(this._keyPathStringFragment(keyPath[i]));}
keyColumnHeaderFragment.createTextChild(']');}else{var keyPathString=(keyPath);keyColumnHeaderFragment.appendChild(this._keyPathStringFragment(keyPathString));}
keyColumnHeaderFragment.createTextChild(')');return keyColumnHeaderFragment;}
_keyPathStringFragment(keyPathString){var keyPathStringFragment=createDocumentFragment();keyPathStringFragment.createTextChild('"');var keyPathSpan=keyPathStringFragment.createChild('span','source-code indexed-db-key-path');keyPathSpan.textContent=keyPathString;keyPathStringFragment.createTextChild('"');return keyPathStringFragment;}
_createEditorToolbar(){var editorToolbar=new UI.Toolbar('data-view-toolbar',this.element);this._pageBackButton=new UI.ToolbarButton(Common.UIString('Show previous page'),'largeicon-play-back');this._pageBackButton.addEventListener(UI.ToolbarButton.Events.Click,this._pageBackButtonClicked,this);editorToolbar.appendToolbarItem(this._pageBackButton);this._pageForwardButton=new UI.ToolbarButton(Common.UIString('Show next page'),'largeicon-play');this._pageForwardButton.setEnabled(false);this._pageForwardButton.addEventListener(UI.ToolbarButton.Events.Click,this._pageForwardButtonClicked,this);editorToolbar.appendToolbarItem(this._pageForwardButton);this._keyInputElement=UI.createInput('key-input');editorToolbar.element.appendChild(this._keyInputElement);this._keyInputElement.placeholder=Common.UIString('Start from key');this._keyInputElement.addEventListener('paste',this._keyInputChanged.bind(this),false);this._keyInputElement.addEventListener('cut',this._keyInputChanged.bind(this),false);this._keyInputElement.addEventListener('keypress',this._keyInputChanged.bind(this),false);this._keyInputElement.addEventListener('keydown',this._keyInputChanged.bind(this),false);}
_pageBackButtonClicked(event){this._skipCount=Math.max(0,this._skipCount-this._pageSize);this._updateData(false);}
_pageForwardButtonClicked(event){this._skipCount=this._skipCount+this._pageSize;this._updateData(false);}
_keyInputChanged(){window.setTimeout(this._updateData.bind(this,false),0);}
update(objectStore,index){this._objectStore=objectStore;this._index=index;if(this._dataGrid)
this._dataGrid.asWidget().detach();this._dataGrid=this._createDataGrid();this._dataGrid.asWidget().show(this.element);this._skipCount=0;this._updateData(true);}
_parseKey(keyString){var result;try{result=JSON.parse(keyString);}catch(e){result=keyString;}
return result;}
_updateData(force){var key=this._parseKey(this._keyInputElement.value);var pageSize=this._pageSize;var skipCount=this._skipCount;this._refreshButton.setEnabled(false);this._clearButton.setEnabled(!this._isIndex);if(!force&&this._lastKey===key&&this._lastPageSize===pageSize&&this._lastSkipCount===skipCount)
return;if(this._lastKey!==key||this._lastPageSize!==pageSize){skipCount=0;this._skipCount=0;}
this._lastKey=key;this._lastPageSize=pageSize;this._lastSkipCount=skipCount;function callback(entries,hasMore){this._refreshButton.setEnabled(true);this.clear();this._entries=entries;for(var i=0;i<entries.length;++i){var data={};data['number']=i+skipCount;data['key']=entries[i].key;data['primaryKey']=entries[i].primaryKey;data['value']=entries[i].value;var node=new Resources.IDBDataGridNode(data);this._dataGrid.rootNode().appendChild(node);}
this._pageBackButton.setEnabled(!!skipCount);this._pageForwardButton.setEnabled(hasMore);this._updatedDataForTests();}
var idbKeyRange=key?window.IDBKeyRange.lowerBound(key):null;if(this._isIndex){this._model.loadIndexData(this._databaseId,this._objectStore.name,this._index.name,idbKeyRange,skipCount,pageSize,callback.bind(this));}else{this._model.loadObjectStoreData(this._databaseId,this._objectStore.name,idbKeyRange,skipCount,pageSize,callback.bind(this));}}
_updatedDataForTests(){}
_refreshButtonClicked(event){this._updateData(true);}
async _clearButtonClicked(event){this._clearButton.setEnabled(false);await this._model.clearObjectStore(this._databaseId,this._objectStore.name);this._clearButton.setEnabled(true);this._updateData(true);}
syncToolbarItems(){return[this._refreshButton,this._clearButton];}
clear(){this._dataGrid.rootNode().removeChildren();this._entries=[];}};Resources.IDBDataGridNode=class extends DataGrid.DataGridNode{constructor(data){super(data,false);this.selectable=false;}
createCell(columnIdentifier){var cell=super.createCell(columnIdentifier);var value=(this.data[columnIdentifier]);switch(columnIdentifier){case'value':case'key':case'primaryKey':cell.removeChildren();var objectElement=ObjectUI.ObjectPropertiesSection.defaultObjectPresentation(value,undefined,true);cell.appendChild(objectElement);break;default:}
return cell;}};;Resources.ResourcesPanel=class extends UI.PanelWithSidebar{constructor(){super('resources');this.registerRequiredCSS('resources/resourcesPanel.css');this._resourcesLastSelectedItemSetting=Common.settings.createSetting('resourcesLastSelectedElementPath',[]);this.visibleView=null;this._pendingViewPromise=null;this._categoryView=null;var mainContainer=new UI.VBox();this.storageViews=mainContainer.element.createChild('div','vbox flex-auto');this._storageViewToolbar=new UI.Toolbar('resources-toolbar',mainContainer.element);this.splitWidget().setMainWidget(mainContainer);this._domStorageView=null;this._cookieView=null;this._emptyWidget=null;this._sidebar=new Resources.ApplicationPanelSidebar(this);this._sidebar.show(this.panelSidebarElement());}
static _instance(){return(self.runtime.sharedInstance(Resources.ResourcesPanel));}
static _shouldCloseOnReset(view){var viewClassesToClose=[SourceFrame.ResourceSourceFrame,SourceFrame.ImageView,SourceFrame.FontView,Resources.StorageItemsView,Resources.DatabaseQueryView,Resources.DatabaseTableView];return viewClassesToClose.some(type=>view instanceof type);}
focus(){this._sidebar.focus();}
lastSelectedItemPath(){return this._resourcesLastSelectedItemSetting.get();}
setLastSelectedItemPath(path){this._resourcesLastSelectedItemSetting.set(path);}
resetView(){if(this.visibleView&&Resources.ResourcesPanel._shouldCloseOnReset(this.visibleView))
this.showView(null);}
showView(view){this._pendingViewPromise=null;if(this.visibleView===view)
return;if(this.visibleView)
this.visibleView.detach();if(view)
view.show(this.storageViews);this.visibleView=view;this._storageViewToolbar.removeToolbarItems();var toolbarItems=(view instanceof UI.SimpleView&&view.syncToolbarItems())||[];for(var i=0;i<toolbarItems.length;++i)
this._storageViewToolbar.appendToolbarItem(toolbarItems[i]);this._storageViewToolbar.element.classList.toggle('hidden',!toolbarItems.length);}
async scheduleShowView(viewPromise){this._pendingViewPromise=viewPromise;var view=await viewPromise;if(this._pendingViewPromise!==viewPromise)
return null;this.showView(view);return view;}
showCategoryView(categoryName){if(!this._categoryView)
this._categoryView=new Resources.StorageCategoryView();this._categoryView.setText(categoryName);this.showView(this._categoryView);}
showDOMStorage(domStorage){if(!domStorage)
return;if(!this._domStorageView)
this._domStorageView=new Resources.DOMStorageItemsView(domStorage);else
this._domStorageView.setStorage(domStorage);this.showView(this._domStorageView);}
showCookies(cookieFrameTarget,cookieDomain){var model=cookieFrameTarget.model(SDK.CookieModel);if(!model)
return;if(!this._cookieView)
this._cookieView=new Resources.CookieItemsView(model,cookieDomain);else
this._cookieView.setCookiesDomain(model,cookieDomain);this.showView(this._cookieView);}
showEmptyWidget(text){if(!this._emptyWidget)
this._emptyWidget=new UI.EmptyWidget(text);else
this._emptyWidget.text=text;this.showView(this._emptyWidget);}
clearCookies(target,cookieDomain){var model=target.model(SDK.CookieModel);if(!model)
return;model.clear(cookieDomain,()=>{if(this._cookieView)
this._cookieView.refreshItems();});}};Resources.ResourcesPanel.ResourceRevealer=class{async reveal(resource){if(!(resource instanceof SDK.Resource))
return Promise.reject(new Error('Internal error: not a resource'));var sidebar=Resources.ResourcesPanel._instance()._sidebar;await UI.viewManager.showView('resources');await sidebar.showResource(resource);}};;Resources.ApplicationPanelSidebar=class extends UI.VBox{constructor(panel){super();this._panel=panel;this._sidebarTree=new UI.TreeOutlineInShadow();this._sidebarTree.element.classList.add('resources-sidebar');this._sidebarTree.registerRequiredCSS('resources/resourcesSidebar.css');this._sidebarTree.element.classList.add('filter-all');this._sidebarTree.addEventListener(UI.TreeOutline.Events.ElementAttached,this._treeElementAdded,this);this.contentElement.appendChild(this._sidebarTree.element);this._applicationTreeElement=this._addSidebarSection(Common.UIString('Application'));var manifestTreeElement=new Resources.AppManifestTreeElement(panel);this._applicationTreeElement.appendChild(manifestTreeElement);this.serviceWorkersTreeElement=new Resources.ServiceWorkersTreeElement(panel);this._applicationTreeElement.appendChild(this.serviceWorkersTreeElement);var clearStorageTreeElement=new Resources.ClearStorageTreeElement(panel);this._applicationTreeElement.appendChild(clearStorageTreeElement);var storageTreeElement=this._addSidebarSection(Common.UIString('Storage'));this.localStorageListTreeElement=new Resources.StorageCategoryTreeElement(panel,Common.UIString('Local Storage'),'LocalStorage');var localStorageIcon=UI.Icon.create('mediumicon-table','resource-tree-item');this.localStorageListTreeElement.setLeadingIcons([localStorageIcon]);storageTreeElement.appendChild(this.localStorageListTreeElement);this.sessionStorageListTreeElement=new Resources.StorageCategoryTreeElement(panel,Common.UIString('Session Storage'),'SessionStorage');var sessionStorageIcon=UI.Icon.create('mediumicon-table','resource-tree-item');this.sessionStorageListTreeElement.setLeadingIcons([sessionStorageIcon]);storageTreeElement.appendChild(this.sessionStorageListTreeElement);this.indexedDBListTreeElement=new Resources.IndexedDBTreeElement(panel);storageTreeElement.appendChild(this.indexedDBListTreeElement);this.databasesListTreeElement=new Resources.StorageCategoryTreeElement(panel,Common.UIString('Web SQL'),'Databases');var databaseIcon=UI.Icon.create('mediumicon-database','resource-tree-item');this.databasesListTreeElement.setLeadingIcons([databaseIcon]);storageTreeElement.appendChild(this.databasesListTreeElement);this.cookieListTreeElement=new Resources.StorageCategoryTreeElement(panel,Common.UIString('Cookies'),'Cookies');var cookieIcon=UI.Icon.create('mediumicon-cookie','resource-tree-item');this.cookieListTreeElement.setLeadingIcons([cookieIcon]);storageTreeElement.appendChild(this.cookieListTreeElement);var cacheTreeElement=this._addSidebarSection(Common.UIString('Cache'));this.cacheStorageListTreeElement=new Resources.ServiceWorkerCacheTreeElement(panel);cacheTreeElement.appendChild(this.cacheStorageListTreeElement);this.applicationCacheListTreeElement=new Resources.StorageCategoryTreeElement(panel,Common.UIString('Application Cache'),'ApplicationCache');var applicationCacheIcon=UI.Icon.create('mediumicon-table','resource-tree-item');this.applicationCacheListTreeElement.setLeadingIcons([applicationCacheIcon]);cacheTreeElement.appendChild(this.applicationCacheListTreeElement);this._resourcesSection=new Resources.ResourcesSection(panel,this._addSidebarSection(Common.UIString('Frames')));this._databaseTableViews=new Map();this._databaseQueryViews=new Map();this._databaseTreeElements=new Map();this._domStorageTreeElements=new Map();this._domains={};this._sidebarTree.contentElement.addEventListener('mousemove',this._onmousemove.bind(this),false);this._sidebarTree.contentElement.addEventListener('mouseleave',this._onmouseleave.bind(this),false);SDK.targetManager.observeTargets(this);SDK.targetManager.addModelListener(SDK.ResourceTreeModel,SDK.ResourceTreeModel.Events.FrameNavigated,this._frameNavigated,this);var selection=this._panel.lastSelectedItemPath();if(!selection.length)
manifestTreeElement.select();}
_addSidebarSection(title){var treeElement=new UI.TreeElement(title,true);treeElement.listItemElement.classList.add('storage-group-list-item');treeElement.setCollapsible(false);treeElement.selectable=false;this._sidebarTree.appendChild(treeElement);return treeElement;}
targetAdded(target){if(this._target)
return;this._target=target;this._databaseModel=target.model(Resources.DatabaseModel);this._databaseModel.addEventListener(Resources.DatabaseModel.Events.DatabaseAdded,this._databaseAdded,this);this._databaseModel.addEventListener(Resources.DatabaseModel.Events.DatabasesRemoved,this._resetWebSQL,this);var resourceTreeModel=target.model(SDK.ResourceTreeModel);if(!resourceTreeModel)
return;if(resourceTreeModel.cachedResourcesLoaded())
this._initialize();resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.CachedResourcesLoaded,this._initialize,this);resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.WillLoadCachedResources,this._resetWithFrames,this);}
targetRemoved(target){if(target!==this._target)
return;delete this._target;var resourceTreeModel=target.model(SDK.ResourceTreeModel);if(resourceTreeModel){resourceTreeModel.removeEventListener(SDK.ResourceTreeModel.Events.CachedResourcesLoaded,this._initialize,this);resourceTreeModel.removeEventListener(SDK.ResourceTreeModel.Events.WillLoadCachedResources,this._resetWithFrames,this);}
this._databaseModel.removeEventListener(Resources.DatabaseModel.Events.DatabaseAdded,this._databaseAdded,this);this._databaseModel.removeEventListener(Resources.DatabaseModel.Events.DatabasesRemoved,this._resetWebSQL,this);this._resetWithFrames();}
focus(){this._sidebarTree.focus();}
_initialize(){for(var frame of SDK.ResourceTreeModel.frames())
this._addCookieDocument(frame);this._databaseModel.enable();var indexedDBModel=this._target.model(Resources.IndexedDBModel);if(indexedDBModel)
indexedDBModel.enable();var cacheStorageModel=this._target.model(SDK.ServiceWorkerCacheModel);if(cacheStorageModel)
cacheStorageModel.enable();var resourceTreeModel=this._target.model(SDK.ResourceTreeModel);if(resourceTreeModel)
this._populateApplicationCacheTree(resourceTreeModel);var domStorageModel=this._target.model(Resources.DOMStorageModel);if(domStorageModel)
this._populateDOMStorageTree(domStorageModel);this.indexedDBListTreeElement._initialize();var serviceWorkerCacheModel=this._target.model(SDK.ServiceWorkerCacheModel);this.cacheStorageListTreeElement._initialize(serviceWorkerCacheModel);}
_resetWithFrames(){this._resourcesSection.reset();this._reset();}
_resetWebSQL(){var queryViews=this._databaseQueryViews.valuesArray();for(var i=0;i<queryViews.length;++i){queryViews[i].removeEventListener(Resources.DatabaseQueryView.Events.SchemaUpdated,this._updateDatabaseTables,this);}
this._databaseTableViews.clear();this._databaseQueryViews.clear();this._databaseTreeElements.clear();this.databasesListTreeElement.removeChildren();this.databasesListTreeElement.setExpandable(false);}
_resetAppCache(){for(var frameId of Object.keys(this._applicationCacheFrameElements))
this._applicationCacheFrameManifestRemoved({data:frameId});this.applicationCacheListTreeElement.setExpandable(false);}
_treeElementAdded(event){var selection=this._panel.lastSelectedItemPath();if(!selection.length)
return;var element=event.data;var index=selection.indexOf(element.itemURL);if(index<0)
return;for(var parent=element.parent;parent;parent=parent.parent)
parent.expand();if(index>0)
element.expand();element.select();}
_reset(){this._domains={};this._resetWebSQL();this.cookieListTreeElement.removeChildren();}
_frameNavigated(event){var frame=event.data;if(!frame.parentFrame)
this._reset();var applicationCacheFrameTreeElement=this._applicationCacheFrameElements[frame.id];if(applicationCacheFrameTreeElement)
applicationCacheFrameTreeElement.frameNavigated(frame);this._addCookieDocument(frame);}
_databaseAdded(event){var database=(event.data);var databaseTreeElement=new Resources.DatabaseTreeElement(this,database);this._databaseTreeElements.set(database,databaseTreeElement);this.databasesListTreeElement.appendChild(databaseTreeElement);}
_addCookieDocument(frame){var parsedURL=frame.url.asParsedURL();if(!parsedURL||(parsedURL.scheme!=='http'&&parsedURL.scheme!=='https'&&parsedURL.scheme!=='file'))
return;var domain=parsedURL.securityOrigin();if(!this._domains[domain]){this._domains[domain]=true;var cookieDomainTreeElement=new Resources.CookieTreeElement(this._panel,frame,domain);this.cookieListTreeElement.appendChild(cookieDomainTreeElement);}}
_domStorageAdded(event){var domStorage=(event.data);this._addDOMStorage(domStorage);}
_addDOMStorage(domStorage){console.assert(!this._domStorageTreeElements.get(domStorage));var domStorageTreeElement=new Resources.DOMStorageTreeElement(this._panel,domStorage);this._domStorageTreeElements.set(domStorage,domStorageTreeElement);if(domStorage.isLocalStorage)
this.localStorageListTreeElement.appendChild(domStorageTreeElement);else
this.sessionStorageListTreeElement.appendChild(domStorageTreeElement);}
_domStorageRemoved(event){var domStorage=(event.data);this._removeDOMStorage(domStorage);}
_removeDOMStorage(domStorage){var treeElement=this._domStorageTreeElements.get(domStorage);if(!treeElement)
return;var wasSelected=treeElement.selected;var parentListTreeElement=treeElement.parent;parentListTreeElement.removeChild(treeElement);if(wasSelected)
parentListTreeElement.select();this._domStorageTreeElements.remove(domStorage);}
selectDatabase(database){if(database){this._showDatabase(database);this._databaseTreeElements.get(database).select();}}
async showResource(resource,line,column){var resourceTreeElement=Resources.FrameResourceTreeElement.forResource(resource);if(resourceTreeElement)
await resourceTreeElement.revealResource(line,column);}
_showDatabase(database,tableName){if(!database)
return;var view;if(tableName){var tableViews=this._databaseTableViews.get(database);if(!tableViews){tableViews=({});this._databaseTableViews.set(database,tableViews);}
view=tableViews[tableName];if(!view){view=new Resources.DatabaseTableView(database,tableName);tableViews[tableName]=view;}}else{view=this._databaseQueryViews.get(database);if(!view){view=new Resources.DatabaseQueryView(database);this._databaseQueryViews.set(database,view);view.addEventListener(Resources.DatabaseQueryView.Events.SchemaUpdated,this._updateDatabaseTables,this);}}
this._innerShowView(view);}
_showApplicationCache(frameId){if(!this._applicationCacheViews[frameId]){this._applicationCacheViews[frameId]=new Resources.ApplicationCacheItemsView(this._applicationCacheModel,frameId);}
this._innerShowView(this._applicationCacheViews[frameId]);}
showFileSystem(view){this._innerShowView(view);}
_innerShowView(view){this._panel.showView(view);}
_updateDatabaseTables(event){var database=event.data;if(!database)
return;var databasesTreeElement=this._databaseTreeElements.get(database);if(!databasesTreeElement)
return;databasesTreeElement.invalidateChildren();var tableViews=this._databaseTableViews.get(database);if(!tableViews)
return;var tableNamesHash={};var panel=this._panel;function tableNamesCallback(tableNames){var tableNamesLength=tableNames.length;for(var i=0;i<tableNamesLength;++i)
tableNamesHash[tableNames[i]]=true;for(var tableName in tableViews){if(!(tableName in tableNamesHash)){if(panel.visibleView===tableViews[tableName])
panel.showView(null);delete tableViews[tableName];}}}
database.getTableNames(tableNamesCallback);}
_populateDOMStorageTree(domStorageModel){domStorageModel.enable();domStorageModel.storages().forEach(this._addDOMStorage.bind(this));domStorageModel.addEventListener(Resources.DOMStorageModel.Events.DOMStorageAdded,this._domStorageAdded,this);domStorageModel.addEventListener(Resources.DOMStorageModel.Events.DOMStorageRemoved,this._domStorageRemoved,this);}
_populateApplicationCacheTree(resourceTreeModel){this._applicationCacheModel=this._target.model(Resources.ApplicationCacheModel);this._applicationCacheViews={};this._applicationCacheFrameElements={};this._applicationCacheManifestElements={};this._applicationCacheModel.addEventListener(Resources.ApplicationCacheModel.Events.FrameManifestAdded,this._applicationCacheFrameManifestAdded,this);this._applicationCacheModel.addEventListener(Resources.ApplicationCacheModel.Events.FrameManifestRemoved,this._applicationCacheFrameManifestRemoved,this);this._applicationCacheModel.addEventListener(Resources.ApplicationCacheModel.Events.FrameManifestsReset,this._resetAppCache,this);this._applicationCacheModel.addEventListener(Resources.ApplicationCacheModel.Events.FrameManifestStatusUpdated,this._applicationCacheFrameManifestStatusChanged,this);this._applicationCacheModel.addEventListener(Resources.ApplicationCacheModel.Events.NetworkStateChanged,this._applicationCacheNetworkStateChanged,this);}
_applicationCacheFrameManifestAdded(event){var frameId=event.data;var manifestURL=this._applicationCacheModel.frameManifestURL(frameId);var manifestTreeElement=this._applicationCacheManifestElements[manifestURL];if(!manifestTreeElement){manifestTreeElement=new Resources.ApplicationCacheManifestTreeElement(this._panel,manifestURL);this.applicationCacheListTreeElement.appendChild(manifestTreeElement);this._applicationCacheManifestElements[manifestURL]=manifestTreeElement;}
var model=this._target.model(SDK.ResourceTreeModel);var frameTreeElement=new Resources.ApplicationCacheFrameTreeElement(this,model.frameForId(frameId),manifestURL);manifestTreeElement.appendChild(frameTreeElement);manifestTreeElement.expand();this._applicationCacheFrameElements[frameId]=frameTreeElement;}
_applicationCacheFrameManifestRemoved(event){var frameId=event.data;var frameTreeElement=this._applicationCacheFrameElements[frameId];if(!frameTreeElement)
return;var manifestURL=frameTreeElement.manifestURL;delete this._applicationCacheFrameElements[frameId];delete this._applicationCacheViews[frameId];frameTreeElement.parent.removeChild(frameTreeElement);var manifestTreeElement=this._applicationCacheManifestElements[manifestURL];if(manifestTreeElement.childCount())
return;delete this._applicationCacheManifestElements[manifestURL];manifestTreeElement.parent.removeChild(manifestTreeElement);}
_applicationCacheFrameManifestStatusChanged(event){var frameId=event.data;var status=this._applicationCacheModel.frameManifestStatus(frameId);if(this._applicationCacheViews[frameId])
this._applicationCacheViews[frameId].updateStatus(status);}
_applicationCacheNetworkStateChanged(event){var isNowOnline=event.data;for(var manifestURL in this._applicationCacheViews)
this._applicationCacheViews[manifestURL].updateNetworkState(isNowOnline);}
showView(view){if(view)
this.showResource(view.resource);}
_onmousemove(event){var nodeUnderMouse=event.target;if(!nodeUnderMouse)
return;var listNode=nodeUnderMouse.enclosingNodeOrSelfWithNodeName('li');if(!listNode)
return;var element=listNode.treeElement;if(this._previousHoveredElement===element)
return;if(this._previousHoveredElement){this._previousHoveredElement.hovered=false;delete this._previousHoveredElement;}
if(element instanceof Resources.FrameTreeElement){this._previousHoveredElement=element;element.hovered=true;}}
_onmouseleave(event){if(this._previousHoveredElement){this._previousHoveredElement.hovered=false;delete this._previousHoveredElement;}}};Resources.BaseStorageTreeElement=class extends UI.TreeElement{constructor(storagePanel,title,expandable){super(title,expandable);this._storagePanel=storagePanel;}
onselect(selectedByUser){if(!selectedByUser)
return false;var path=[];for(var el=this;el;el=el.parent){var url=el.itemURL;if(!url)
break;path.push(url);}
this._storagePanel.setLastSelectedItemPath(path);return false;}
showView(view){this._storagePanel.showView(view);}};Resources.StorageCategoryTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,categoryName,settingsKey){super(storagePanel,categoryName,false);this._expandedSetting=Common.settings.createSetting('resources'+settingsKey+'Expanded',settingsKey==='Frames');this._categoryName=categoryName;}
get itemURL(){return'category://'+this._categoryName;}
onselect(selectedByUser){super.onselect(selectedByUser);this._storagePanel.showCategoryView(this._categoryName);return false;}
onattach(){super.onattach();if(this._expandedSetting.get())
this.expand();}
onexpand(){this._expandedSetting.set(true);}
oncollapse(){this._expandedSetting.set(false);}};Resources.DatabaseTreeElement=class extends Resources.BaseStorageTreeElement{constructor(sidebar,database){super(sidebar._panel,database.name,true);this._sidebar=sidebar;this._database=database;var icon=UI.Icon.create('mediumicon-database','resource-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'database://'+encodeURI(this._database.name);}
onselect(selectedByUser){super.onselect(selectedByUser);this._sidebar._showDatabase(this._database);return false;}
onexpand(){this._updateChildren();}
async _updateChildren(){var tableNames=await this._database.tableNames();for(var tableName of tableNames)
this.appendChild(new Resources.DatabaseTableTreeElement(this._sidebar,this._database,tableName));}};Resources.DatabaseTableTreeElement=class extends Resources.BaseStorageTreeElement{constructor(sidebar,database,tableName){super(sidebar._panel,tableName,false);this._sidebar=sidebar;this._database=database;this._tableName=tableName;var icon=UI.Icon.create('mediumicon-table','resource-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'database://'+encodeURI(this._database.name)+'/'+encodeURI(this._tableName);}
onselect(selectedByUser){super.onselect(selectedByUser);this._sidebar._showDatabase(this._database,this._tableName);return false;}};Resources.ServiceWorkerCacheTreeElement=class extends Resources.StorageCategoryTreeElement{constructor(storagePanel){super(storagePanel,Common.UIString('Cache Storage'),'CacheStorage');var icon=UI.Icon.create('mediumicon-database','resource-tree-item');this.setLeadingIcons([icon]);this._swCacheModel=null;}
_initialize(model){this._swCacheTreeElements=[];this._swCacheModel=model;if(model){for(var cache of model.caches())
this._addCache(model,cache);}
SDK.targetManager.addModelListener(SDK.ServiceWorkerCacheModel,SDK.ServiceWorkerCacheModel.Events.CacheAdded,this._cacheAdded,this);SDK.targetManager.addModelListener(SDK.ServiceWorkerCacheModel,SDK.ServiceWorkerCacheModel.Events.CacheRemoved,this._cacheRemoved,this);}
onattach(){super.onattach();this.listItemElement.addEventListener('contextmenu',this._handleContextMenuEvent.bind(this),true);}
_handleContextMenuEvent(event){var contextMenu=new UI.ContextMenu(event);contextMenu.defaultSection().appendItem(Common.UIString('Refresh Caches'),this._refreshCaches.bind(this));contextMenu.show();}
_refreshCaches(){if(this._swCacheModel)
this._swCacheModel.refreshCacheNames();}
_cacheAdded(event){var cache=(event.data.cache);var model=(event.data.model);this._addCache(model,cache);}
_addCache(model,cache){var swCacheTreeElement=new Resources.SWCacheTreeElement(this._storagePanel,model,cache);this._swCacheTreeElements.push(swCacheTreeElement);this.appendChild(swCacheTreeElement);}
_cacheRemoved(event){var cache=(event.data.cache);var model=(event.data.model);var swCacheTreeElement=this._cacheTreeElement(model,cache);if(!swCacheTreeElement)
return;this.removeChild(swCacheTreeElement);this._swCacheTreeElements.remove(swCacheTreeElement);this.setExpandable(this.childCount()>0);}
_cacheTreeElement(model,cache){var index=-1;for(var i=0;i<this._swCacheTreeElements.length;++i){if(this._swCacheTreeElements[i]._cache.equals(cache)&&this._swCacheTreeElements[i]._model===model){index=i;break;}}
if(index!==-1)
return this._swCacheTreeElements[i];return null;}};Resources.SWCacheTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,model,cache){super(storagePanel,cache.cacheName+' - '+cache.securityOrigin,false);this._model=model;this._cache=cache;this._view=null;var icon=UI.Icon.create('mediumicon-table','resource-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'cache://'+this._cache.cacheId;}
onattach(){super.onattach();this.listItemElement.addEventListener('contextmenu',this._handleContextMenuEvent.bind(this),true);}
_handleContextMenuEvent(event){var contextMenu=new UI.ContextMenu(event);contextMenu.defaultSection().appendItem(Common.UIString('Delete'),this._clearCache.bind(this));contextMenu.show();}
_clearCache(){this._model.deleteCache(this._cache);}
update(cache){this._cache=cache;if(this._view)
this._view.update(cache);}
onselect(selectedByUser){super.onselect(selectedByUser);if(!this._view)
this._view=new Resources.ServiceWorkerCacheView(this._model,this._cache);this.showView(this._view);return false;}};Resources.ServiceWorkersTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel){super(storagePanel,Common.UIString('Service Workers'),false);var icon=UI.Icon.create('mediumicon-service-worker','resource-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'service-workers://';}
onselect(selectedByUser){super.onselect(selectedByUser);if(!this._view)
this._view=new Resources.ServiceWorkersView();this.showView(this._view);return false;}};Resources.AppManifestTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel){super(storagePanel,Common.UIString('Manifest'),false);var icon=UI.Icon.create('mediumicon-manifest','resource-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'manifest://';}
onselect(selectedByUser){super.onselect(selectedByUser);if(!this._view)
this._view=new Resources.AppManifestView();this.showView(this._view);return false;}};Resources.ClearStorageTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel){super(storagePanel,Common.UIString('Clear storage'),false);var icon=UI.Icon.create('mediumicon-clear-storage','resource-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'clear-storage://';}
onselect(selectedByUser){super.onselect(selectedByUser);if(!this._view)
this._view=new Resources.ClearStorageView();this.showView(this._view);return false;}};Resources.IndexedDBTreeElement=class extends Resources.StorageCategoryTreeElement{constructor(storagePanel){super(storagePanel,Common.UIString('IndexedDB'),'IndexedDB');var icon=UI.Icon.create('mediumicon-database','resource-tree-item');this.setLeadingIcons([icon]);}
_initialize(){SDK.targetManager.addModelListener(Resources.IndexedDBModel,Resources.IndexedDBModel.Events.DatabaseAdded,this._indexedDBAdded,this);SDK.targetManager.addModelListener(Resources.IndexedDBModel,Resources.IndexedDBModel.Events.DatabaseRemoved,this._indexedDBRemoved,this);SDK.targetManager.addModelListener(Resources.IndexedDBModel,Resources.IndexedDBModel.Events.DatabaseLoaded,this._indexedDBLoaded,this);this._idbDatabaseTreeElements=[];for(var indexedDBModel of SDK.targetManager.models(Resources.IndexedDBModel)){var databases=indexedDBModel.databases();for(var j=0;j<databases.length;++j)
this._addIndexedDB(indexedDBModel,databases[j]);}}
onattach(){super.onattach();this.listItemElement.addEventListener('contextmenu',this._handleContextMenuEvent.bind(this),true);}
_handleContextMenuEvent(event){var contextMenu=new UI.ContextMenu(event);contextMenu.defaultSection().appendItem(Common.UIString('Refresh IndexedDB'),this.refreshIndexedDB.bind(this));contextMenu.show();}
refreshIndexedDB(){for(var indexedDBModel of SDK.targetManager.models(Resources.IndexedDBModel))
indexedDBModel.refreshDatabaseNames();}
_indexedDBAdded(event){var databaseId=(event.data.databaseId);var model=(event.data.model);this._addIndexedDB(model,databaseId);}
_addIndexedDB(model,databaseId){var idbDatabaseTreeElement=new Resources.IDBDatabaseTreeElement(this._storagePanel,model,databaseId);this._idbDatabaseTreeElements.push(idbDatabaseTreeElement);this.appendChild(idbDatabaseTreeElement);model.refreshDatabase(databaseId);}
_indexedDBRemoved(event){var databaseId=(event.data.databaseId);var model=(event.data.model);var idbDatabaseTreeElement=this._idbDatabaseTreeElement(model,databaseId);if(!idbDatabaseTreeElement)
return;idbDatabaseTreeElement.clear();this.removeChild(idbDatabaseTreeElement);this._idbDatabaseTreeElements.remove(idbDatabaseTreeElement);this.setExpandable(this.childCount()>0);}
_indexedDBLoaded(event){var database=(event.data.database);var model=(event.data.model);var idbDatabaseTreeElement=this._idbDatabaseTreeElement(model,database.databaseId);if(!idbDatabaseTreeElement)
return;idbDatabaseTreeElement.update(database);}
_idbDatabaseTreeElement(model,databaseId){var index=-1;for(var i=0;i<this._idbDatabaseTreeElements.length;++i){if(this._idbDatabaseTreeElements[i]._databaseId.equals(databaseId)&&this._idbDatabaseTreeElements[i]._model===model){index=i;break;}}
if(index!==-1)
return this._idbDatabaseTreeElements[i];return null;}};Resources.IDBDatabaseTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,model,databaseId){super(storagePanel,databaseId.name+' - '+databaseId.securityOrigin,false);this._model=model;this._databaseId=databaseId;this._idbObjectStoreTreeElements={};var icon=UI.Icon.create('mediumicon-database','resource-tree-item');this.setLeadingIcons([icon]);this._model.addEventListener(Resources.IndexedDBModel.Events.DatabaseNamesRefreshed,this._refreshIndexedDB,this);}
get itemURL(){return'indexedDB://'+this._databaseId.securityOrigin+'/'+this._databaseId.name;}
onattach(){super.onattach();this.listItemElement.addEventListener('contextmenu',this._handleContextMenuEvent.bind(this),true);}
_handleContextMenuEvent(event){var contextMenu=new UI.ContextMenu(event);contextMenu.defaultSection().appendItem(Common.UIString('Refresh IndexedDB'),this._refreshIndexedDB.bind(this));contextMenu.show();}
_refreshIndexedDB(){this._model.refreshDatabase(this._databaseId);}
update(database){this._database=database;var objectStoreNames={};for(var objectStoreName in this._database.objectStores){var objectStore=this._database.objectStores[objectStoreName];objectStoreNames[objectStore.name]=true;if(!this._idbObjectStoreTreeElements[objectStore.name]){var idbObjectStoreTreeElement=new Resources.IDBObjectStoreTreeElement(this._storagePanel,this._model,this._databaseId,objectStore);this._idbObjectStoreTreeElements[objectStore.name]=idbObjectStoreTreeElement;this.appendChild(idbObjectStoreTreeElement);}
this._idbObjectStoreTreeElements[objectStore.name].update(objectStore);}
for(var objectStoreName in this._idbObjectStoreTreeElements){if(!objectStoreNames[objectStoreName])
this._objectStoreRemoved(objectStoreName);}
if(this._view)
this._view.update(database);this._updateTooltip();}
_updateTooltip(){this.tooltip=Common.UIString('Version')+': '+this._database.version;}
onselect(selectedByUser){super.onselect(selectedByUser);if(!this._view)
this._view=new Resources.IDBDatabaseView(this._model,this._database);this.showView(this._view);return false;}
_objectStoreRemoved(objectStoreName){var objectStoreTreeElement=this._idbObjectStoreTreeElements[objectStoreName];objectStoreTreeElement.clear();this.removeChild(objectStoreTreeElement);delete this._idbObjectStoreTreeElements[objectStoreName];}
clear(){for(var objectStoreName in this._idbObjectStoreTreeElements)
this._objectStoreRemoved(objectStoreName);}};Resources.IDBObjectStoreTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,model,databaseId,objectStore){super(storagePanel,objectStore.name,false);this._model=model;this._databaseId=databaseId;this._idbIndexTreeElements={};this._objectStore=objectStore;this._view=null;var icon=UI.Icon.create('mediumicon-table','resource-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'indexedDB://'+this._databaseId.securityOrigin+'/'+this._databaseId.name+'/'+
this._objectStore.name;}
onattach(){super.onattach();this.listItemElement.addEventListener('contextmenu',this._handleContextMenuEvent.bind(this),true);}
_handleContextMenuEvent(event){var contextMenu=new UI.ContextMenu(event);contextMenu.defaultSection().appendItem(Common.UIString('Clear'),this._clearObjectStore.bind(this));contextMenu.show();}
async _clearObjectStore(){await this._model.clearObjectStore(this._databaseId,this._objectStore.name);this.update(this._objectStore);}
update(objectStore){this._objectStore=objectStore;var indexNames={};for(var indexName in this._objectStore.indexes){var index=this._objectStore.indexes[indexName];indexNames[index.name]=true;if(!this._idbIndexTreeElements[index.name]){var idbIndexTreeElement=new Resources.IDBIndexTreeElement(this._storagePanel,this._model,this._databaseId,this._objectStore,index);this._idbIndexTreeElements[index.name]=idbIndexTreeElement;this.appendChild(idbIndexTreeElement);}
this._idbIndexTreeElements[index.name].update(index);}
for(var indexName in this._idbIndexTreeElements){if(!indexNames[indexName])
this._indexRemoved(indexName);}
for(var indexName in this._idbIndexTreeElements){if(!indexNames[indexName]){this.removeChild(this._idbIndexTreeElements[indexName]);delete this._idbIndexTreeElements[indexName];}}
if(this.childCount())
this.expand();if(this._view)
this._view.update(this._objectStore,null);this._updateTooltip();}
_updateTooltip(){var keyPathString=this._objectStore.keyPathString;var tooltipString=keyPathString!==null?(Common.UIString('Key path: ')+keyPathString):'';if(this._objectStore.autoIncrement)
tooltipString+='\n'+Common.UIString('autoIncrement');this.tooltip=tooltipString;}
onselect(selectedByUser){super.onselect(selectedByUser);if(!this._view)
this._view=new Resources.IDBDataView(this._model,this._databaseId,this._objectStore,null);this.showView(this._view);return false;}
_indexRemoved(indexName){var indexTreeElement=this._idbIndexTreeElements[indexName];indexTreeElement.clear();this.removeChild(indexTreeElement);delete this._idbIndexTreeElements[indexName];}
clear(){for(var indexName in this._idbIndexTreeElements)
this._indexRemoved(indexName);if(this._view)
this._view.clear();}};Resources.IDBIndexTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,model,databaseId,objectStore,index){super(storagePanel,index.name,false);this._model=model;this._databaseId=databaseId;this._objectStore=objectStore;this._index=index;}
get itemURL(){return'indexedDB://'+this._databaseId.securityOrigin+'/'+this._databaseId.name+'/'+
this._objectStore.name+'/'+this._index.name;}
update(index){this._index=index;if(this._view)
this._view.update(this._index);this._updateTooltip();}
_updateTooltip(){var tooltipLines=[];var keyPathString=this._index.keyPathString;tooltipLines.push(Common.UIString('Key path: ')+keyPathString);if(this._index.unique)
tooltipLines.push(Common.UIString('unique'));if(this._index.multiEntry)
tooltipLines.push(Common.UIString('multiEntry'));this.tooltip=tooltipLines.join('\n');}
onselect(selectedByUser){super.onselect(selectedByUser);if(!this._view)
this._view=new Resources.IDBDataView(this._model,this._databaseId,this._objectStore,this._index);this.showView(this._view);return false;}
clear(){if(this._view)
this._view.clear();}};Resources.DOMStorageTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,domStorage){super(storagePanel,domStorage.securityOrigin?domStorage.securityOrigin:Common.UIString('Local Files'),false);this._domStorage=domStorage;var icon=UI.Icon.create('mediumicon-table','resource-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'storage://'+this._domStorage.securityOrigin+'/'+
(this._domStorage.isLocalStorage?'local':'session');}
onselect(selectedByUser){super.onselect(selectedByUser);this._storagePanel.showDOMStorage(this._domStorage);return false;}
onattach(){super.onattach();this.listItemElement.addEventListener('contextmenu',this._handleContextMenuEvent.bind(this),true);}
_handleContextMenuEvent(event){var contextMenu=new UI.ContextMenu(event);contextMenu.defaultSection().appendItem(Common.UIString('Clear'),()=>this._domStorage.clear());contextMenu.show();}};Resources.CookieTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,frame,cookieDomain){super(storagePanel,cookieDomain?cookieDomain:Common.UIString('Local Files'),false);this._target=frame.resourceTreeModel().target();this._cookieDomain=cookieDomain;var icon=UI.Icon.create('mediumicon-cookie','resource-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'cookies://'+this._cookieDomain;}
onattach(){super.onattach();this.listItemElement.addEventListener('contextmenu',this._handleContextMenuEvent.bind(this),true);}
_handleContextMenuEvent(event){var contextMenu=new UI.ContextMenu(event);contextMenu.defaultSection().appendItem(Common.UIString('Clear'),()=>this._storagePanel.clearCookies(this._target,this._cookieDomain));contextMenu.show();}
onselect(selectedByUser){super.onselect(selectedByUser);this._storagePanel.showCookies(this._target,this._cookieDomain);return false;}};Resources.ApplicationCacheManifestTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,manifestURL){var title=new Common.ParsedURL(manifestURL).displayName;super(storagePanel,title,false);this.tooltip=manifestURL;this._manifestURL=manifestURL;}
get itemURL(){return'appcache://'+this._manifestURL;}
get manifestURL(){return this._manifestURL;}
onselect(selectedByUser){super.onselect(selectedByUser);this._storagePanel.showCategoryView(this._manifestURL);return false;}};Resources.ApplicationCacheFrameTreeElement=class extends Resources.BaseStorageTreeElement{constructor(sidebar,frame,manifestURL){super(sidebar._panel,'',false);this._sidebar=sidebar;this._frameId=frame.id;this._manifestURL=manifestURL;this._refreshTitles(frame);var icon=UI.Icon.create('largeicon-navigator-folder','navigator-tree-item');icon.classList.add('navigator-folder-tree-item');this.setLeadingIcons([icon]);}
get itemURL(){return'appcache://'+this._manifestURL+'/'+encodeURI(this.titleAsText());}
get frameId(){return this._frameId;}
get manifestURL(){return this._manifestURL;}
_refreshTitles(frame){this.title=frame.displayName();}
frameNavigated(frame){this._refreshTitles(frame);}
onselect(selectedByUser){super.onselect(selectedByUser);this._sidebar._showApplicationCache(this._frameId);return false;}};Resources.StorageCategoryView=class extends UI.VBox{constructor(){super();this.element.classList.add('storage-view');this._emptyWidget=new UI.EmptyWidget('');this._emptyWidget.show(this.element);}
setText(text){this._emptyWidget.text=text;}};;Resources.ResourcesSection=class{constructor(storagePanel,treeElement){this._panel=storagePanel;this._treeElement=treeElement;this._treeElementForFrameId=new Map();function addListener(eventType,handler,target){SDK.targetManager.addModelListener(SDK.ResourceTreeModel,eventType,event=>handler.call(target,event.data));}
addListener(SDK.ResourceTreeModel.Events.FrameAdded,this._frameAdded,this);addListener(SDK.ResourceTreeModel.Events.FrameNavigated,this._frameNavigated,this);addListener(SDK.ResourceTreeModel.Events.FrameDetached,this._frameDetached,this);addListener(SDK.ResourceTreeModel.Events.ResourceAdded,this._resourceAdded,this);var mainTarget=SDK.targetManager.mainTarget();var resourceTreeModel=mainTarget&&mainTarget.model(SDK.ResourceTreeModel);var mainFrame=resourceTreeModel&&resourceTreeModel.mainFrame;if(mainFrame)
this._populateFrame(mainFrame);}
static _getParentFrame(frame){var parentFrame=frame.parentFrame;if(parentFrame)
return parentFrame;var parentTarget=frame.resourceTreeModel().target().parentTarget();if(!parentTarget)
return null;return parentTarget.model(SDK.ResourceTreeModel).mainFrame;}
_frameAdded(frame){var parentFrame=Resources.ResourcesSection._getParentFrame(frame);var parentTreeElement=parentFrame?this._treeElementForFrameId.get(parentFrame.id):this._treeElement;if(!parentTreeElement){console.warn(`No frame to route ${frame.url} to.`);return;}
var frameTreeElement=new Resources.FrameTreeElement(this._panel,frame);this._treeElementForFrameId.set(frame.id,frameTreeElement);parentTreeElement.appendChild(frameTreeElement);}
_frameDetached(frame){var frameTreeElement=this._treeElementForFrameId.get(frame.id);if(!frameTreeElement)
return;this._treeElementForFrameId.remove(frame.id);if(frameTreeElement.parent)
frameTreeElement.parent.removeChild(frameTreeElement);}
_frameNavigated(frame){var frameTreeElement=this._treeElementForFrameId.get(frame.id);if(frameTreeElement)
frameTreeElement.frameNavigated(frame);}
_resourceAdded(resource){var statusCode=resource['statusCode'];if(statusCode>=301&&statusCode<=303)
return;var frameTreeElement=this._treeElementForFrameId.get(resource.frameId);if(!frameTreeElement){return;}
frameTreeElement.appendResource(resource);}
reset(){this._treeElement.removeChildren();this._treeElementForFrameId.clear();}
_populateFrame(frame){this._frameAdded(frame);for(var child of frame.childFrames)
this._populateFrame(child);for(var resource of frame.resources())
this._resourceAdded(resource);}};Resources.FrameTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,frame){super(storagePanel,'',false);this._panel=storagePanel;this._frame=frame;this._frameId=frame.id;this._categoryElements={};this._treeElementForResource={};this.frameNavigated(frame);var icon=UI.Icon.create('largeicon-navigator-frame','navigator-tree-item');icon.classList.add('navigator-frame-tree-item');this.setLeadingIcons([icon]);}
frameNavigated(frame){this.removeChildren();this._frameId=frame.id;this.title=frame.displayName();this._categoryElements={};this._treeElementForResource={};}
get itemURL(){return'frame://'+encodeURI(this.titleAsText());}
onselect(selectedByUser){super.onselect(selectedByUser);this._panel.showCategoryView(this.titleAsText());this.listItemElement.classList.remove('hovered');SDK.OverlayModel.hideDOMNodeHighlight();return false;}
set hovered(hovered){if(hovered){this.listItemElement.classList.add('hovered');this._frame.resourceTreeModel().domModel().overlayModel().highlightFrame(this._frameId);}else{this.listItemElement.classList.remove('hovered');SDK.OverlayModel.hideDOMNodeHighlight();}}
appendResource(resource){var resourceType=resource.resourceType();var categoryName=resourceType.name();var categoryElement=resourceType===Common.resourceTypes.Document?this:this._categoryElements[categoryName];if(!categoryElement){categoryElement=new Resources.StorageCategoryTreeElement(this._panel,resource.resourceType().category().title,categoryName);this._categoryElements[resourceType.name()]=categoryElement;this._insertInPresentationOrder(this,categoryElement);}
var resourceTreeElement=new Resources.FrameResourceTreeElement(this._panel,resource);this._insertInPresentationOrder(categoryElement,resourceTreeElement);this._treeElementForResource[resource.url]=resourceTreeElement;}
resourceByURL(url){var treeElement=this._treeElementForResource[url];return treeElement?treeElement._resource:null;}
appendChild(treeElement){this._insertInPresentationOrder(this,treeElement);}
_insertInPresentationOrder(parentTreeElement,childTreeElement){function typeWeight(treeElement){if(treeElement instanceof Resources.StorageCategoryTreeElement)
return 2;if(treeElement instanceof Resources.FrameTreeElement)
return 1;return 3;}
function compare(treeElement1,treeElement2){var typeWeight1=typeWeight(treeElement1);var typeWeight2=typeWeight(treeElement2);var result;if(typeWeight1>typeWeight2)
result=1;else if(typeWeight1<typeWeight2)
result=-1;else
result=treeElement1.titleAsText().localeCompare(treeElement2.titleAsText());return result;}
var childCount=parentTreeElement.childCount();var i;for(i=0;i<childCount;++i){if(compare(childTreeElement,parentTreeElement.childAt(i))<0)
break;}
parentTreeElement.insertChild(childTreeElement,i);}};Resources.FrameResourceTreeElement=class extends Resources.BaseStorageTreeElement{constructor(storagePanel,resource){super(storagePanel,resource.displayName,false);this._panel=storagePanel;this._resource=resource;this._previewPromise=null;this.tooltip=resource.url;this._resource[Resources.FrameResourceTreeElement._symbol]=this;var icon=UI.Icon.create('largeicon-navigator-file','navigator-tree-item');icon.classList.add('navigator-file-tree-item');icon.classList.add('navigator-'+resource.resourceType().name()+'-tree-item');this.setLeadingIcons([icon]);}
static forResource(resource){return resource[Resources.FrameResourceTreeElement._symbol];}
get itemURL(){return this._resource.url;}
_preparePreview(){if(this._previewPromise)
return this._previewPromise;var viewPromise=SourceFrame.PreviewFactory.createPreview(this._resource,this._resource.mimeType);this._previewPromise=viewPromise.then(view=>{if(view)
return view;return new UI.EmptyWidget(this._resource.url);});return this._previewPromise;}
onselect(selectedByUser){super.onselect(selectedByUser);this._panel.scheduleShowView(this._preparePreview());return false;}
ondblclick(event){InspectorFrontendHost.openInNewTab(this._resource.url);return false;}
onattach(){super.onattach();this.listItemElement.draggable=true;this.listItemElement.addEventListener('dragstart',this._ondragstart.bind(this),false);this.listItemElement.addEventListener('contextmenu',this._handleContextMenuEvent.bind(this),true);}
_ondragstart(event){event.dataTransfer.setData('text/plain',this._resource.content||'');event.dataTransfer.effectAllowed='copy';return true;}
_handleContextMenuEvent(event){var contextMenu=new UI.ContextMenu(event);contextMenu.appendApplicableItems(this._resource);contextMenu.show();}
async revealResource(line,column){this.revealAndSelect(true);var view=await this._panel.scheduleShowView(this._preparePreview());if(!(view instanceof SourceFrame.ResourceSourceFrame)||typeof line!=='number')
return;view.revealPosition(line,column,true);}};Resources.FrameResourceTreeElement._symbol=Symbol('treeElement');;Resources.ServiceWorkerCacheView=class extends UI.SimpleView{constructor(model,cache){super(Common.UIString('Cache'));this.registerRequiredCSS('resources/serviceWorkerCacheViews.css');this._model=model;this._entriesForTest=null;this.element.classList.add('service-worker-cache-data-view');this.element.classList.add('storage-view');var editorToolbar=new UI.Toolbar('data-view-toolbar',this.element);this._splitWidget=new UI.SplitWidget(false,false);this._splitWidget.show(this.element);this._previewPanel=new UI.VBox();var resizer=this._previewPanel.element.createChild('div','cache-preview-panel-resizer');this._splitWidget.setMainWidget(this._previewPanel);this._splitWidget.installResizer(resizer);this._preview=null;this._cache=cache;this._dataGrid=null;this._lastPageSize=null;this._lastSkipCount=null;this._refreshThrottler=new Common.Throttler(300);this._pageBackButton=new UI.ToolbarButton(Common.UIString('Show previous page'),'largeicon-play-back');this._pageBackButton.addEventListener(UI.ToolbarButton.Events.Click,this._pageBackButtonClicked,this);editorToolbar.appendToolbarItem(this._pageBackButton);this._pageForwardButton=new UI.ToolbarButton(Common.UIString('Show next page'),'largeicon-play');this._pageForwardButton.setEnabled(false);this._pageForwardButton.addEventListener(UI.ToolbarButton.Events.Click,this._pageForwardButtonClicked,this);editorToolbar.appendToolbarItem(this._pageForwardButton);this._refreshButton=new UI.ToolbarButton(Common.UIString('Refresh'),'largeicon-refresh');this._refreshButton.addEventListener(UI.ToolbarButton.Events.Click,this._refreshButtonClicked,this);editorToolbar.appendToolbarItem(this._refreshButton);this._deleteSelectedButton=new UI.ToolbarButton(Common.UIString('Delete Selected'),'largeicon-delete');this._deleteSelectedButton.addEventListener(UI.ToolbarButton.Events.Click,()=>this._deleteButtonClicked(null));editorToolbar.appendToolbarItem(this._deleteSelectedButton);this._pageSize=50;this._skipCount=0;this.update(cache);}
wasShown(){this._model.addEventListener(SDK.ServiceWorkerCacheModel.Events.CacheStorageContentUpdated,this._cacheContentUpdated,this);this._updateData(true);}
willHide(){this._model.removeEventListener(SDK.ServiceWorkerCacheModel.Events.CacheStorageContentUpdated,this._cacheContentUpdated,this);}
_showPreview(preview){if(this._preview===preview)
return;if(this._preview)
this._preview.detach();if(!preview)
preview=new UI.EmptyWidget(Common.UIString('Select a cache entry above to preview'));this._preview=preview;this._preview.show(this._previewPanel.element);}
_createDataGrid(){var columns=([{id:'path',title:Common.UIString('Path'),weight:4,sortable:true},{id:'contentType',title:Common.UIString('Content-Type'),weight:1,sortable:true},{id:'contentLength',title:Common.UIString('Content-Length'),weight:1,align:DataGrid.DataGrid.Align.Right,sortable:true},{id:'responseTime',title:Common.UIString('Time Cached'),width:'12em',weight:1,align:DataGrid.DataGrid.Align.Right,sortable:true}]);var dataGrid=new DataGrid.DataGrid(columns,undefined,this._deleteButtonClicked.bind(this),this._updateData.bind(this,true));dataGrid.addEventListener(DataGrid.DataGrid.Events.SortingChanged,this._sortingChanged,this);dataGrid.addEventListener(DataGrid.DataGrid.Events.SelectedNode,event=>this._previewCachedResponse(event.data.data),this);dataGrid.setStriped(true);return dataGrid;}
_sortingChanged(){if(!this._dataGrid)
return;var accending=this._dataGrid.isSortOrderAscending();var columnId=this._dataGrid.sortColumnId();var comparator;if(columnId==='path')
comparator=(a,b)=>a._path.localeCompare(b._path);else if(columnId==='contentType')
comparator=(a,b)=>a.data.mimeType.localeCompare(b.data.mimeType);else if(columnId==='contentLength')
comparator=(a,b)=>a.data.resourceSize-b.data.resourceSize;else if(columnId==='responseTime')
comparator=(a,b)=>a.data.endTime-b.data.endTime;var children=this._dataGrid.rootNode().children.slice();this._dataGrid.rootNode().removeChildren();children.sort((a,b)=>{var result=comparator(a,b);return accending?result:-result;});children.forEach(child=>this._dataGrid.rootNode().appendChild(child));}
_pageBackButtonClicked(event){this._skipCount=Math.max(0,this._skipCount-this._pageSize);this._updateData(false);}
_pageForwardButtonClicked(event){this._skipCount=this._skipCount+this._pageSize;this._updateData(false);}
async _deleteButtonClicked(node){if(!node){node=this._dataGrid&&this._dataGrid.selectedNode;if(!node)
return;}
await this._model.deleteCacheEntry(this._cache,(node.data.url()));node.remove();}
update(cache){this._cache=cache;if(this._dataGrid)
this._dataGrid.asWidget().detach();this._dataGrid=this._createDataGrid();this._splitWidget.setSidebarWidget(this._dataGrid.asWidget());this._skipCount=0;this._updateData(true);}
_updateDataCallback(skipCount,entries,hasMore){var selected=this._dataGrid.selectedNode&&this._dataGrid.selectedNode.data.url();this._refreshButton.setEnabled(true);this._entriesForTest=entries;var oldEntries=new Map();var rootNode=this._dataGrid.rootNode();for(var node of rootNode.children)
oldEntries.set(node.data.url,node);rootNode.removeChildren();var selectedNode=null;for(var entry of entries){var node=oldEntries.get(entry.requestURL);if(!node||node.data.responseTime!==entry.responseTime){node=new Resources.ServiceWorkerCacheView.DataGridNode(this._createRequest(entry));node.selectable=true;}
rootNode.appendChild(node);if(entry.requestURL===selected)
selectedNode=node;}
this._pageBackButton.setEnabled(!!skipCount);this._pageForwardButton.setEnabled(hasMore);if(!selectedNode)
this._showPreview(null);else
selectedNode.revealAndSelect();this._updatedForTest();}
_updateData(force){var pageSize=this._pageSize;var skipCount=this._skipCount;if(!force&&this._lastPageSize===pageSize&&this._lastSkipCount===skipCount)
return;this._refreshButton.setEnabled(false);if(this._lastPageSize!==pageSize){skipCount=0;this._skipCount=0;}
this._lastPageSize=pageSize;this._lastSkipCount=skipCount;this._model.loadCacheData(this._cache,skipCount,pageSize,this._updateDataCallback.bind(this,skipCount));}
_refreshButtonClicked(event){this._updateData(true);}
_cacheContentUpdated(event){var nameAndOrigin=event.data;if(this._cache.securityOrigin!==nameAndOrigin.origin||this._cache.cacheName!==nameAndOrigin.cacheName)
return;this._refreshThrottler.schedule(()=>Promise.resolve(this._updateData(true)),true);}
async _previewCachedResponse(request){var preview=request[Resources.ServiceWorkerCacheView._previewSymbol];if(!preview){preview=new Resources.ServiceWorkerCacheView.RequestView(request);request[Resources.ServiceWorkerCacheView._previewSymbol]=preview;}
if(request===this._dataGrid.selectedNode.data)
this._showPreview(preview);}
_createRequest(entry){var request=new SDK.NetworkRequest('cache-storage-'+entry.requestURL,entry.requestURL,'','','',null);request.requestMethod=entry.requestMethod;request.setRequestHeaders(entry.requestHeaders);request.statusCode=entry.responseStatus;request.statusText=entry.responseStatusText;request.protocol=new Common.ParsedURL(entry.requestURL).scheme;request.responseHeaders=entry.responseHeaders;request.setRequestHeadersText('');request.endTime=entry.responseTime;var header=entry.responseHeaders.find(header=>header.name.toLowerCase()==='content-type');var contentType=header?header.value:'text/plain';request.mimeType=contentType;header=entry.responseHeaders.find(header=>header.name.toLowerCase()==='content-length');request.resourceSize=(header&&header.value)|0;var resourceType=Common.ResourceType.fromMimeType(contentType);if(!resourceType)
resourceType=Common.ResourceType.fromURL(entry.requestURL)||Common.resourceTypes.Other;request.setResourceType(resourceType);request.setContentDataProvider(this._requestContent.bind(this,request));return request;}
async _requestContent(request){var isText=request.resourceType().isTextType();var contentData={error:null,content:null,encoded:!isText};var response=await this._cache.requestCachedResponse(request.url());if(response)
contentData.content=isText?window.atob(response.body):response.body;return contentData;}
_updatedForTest(){}};Resources.ServiceWorkerCacheView._previewSymbol=Symbol('preview');Resources.ServiceWorkerCacheView._RESPONSE_CACHE_SIZE=10;Resources.ServiceWorkerCacheView.DataGridNode=class extends DataGrid.DataGridNode{constructor(request){super(request);this._path=Common.ParsedURL.extractPath(request.url());if(!this._path)
this._path=request.url();if(this._path.length>1&&this._path.startsWith('/'))
this._path=this._path.substring(1);this._request=request;}
createCell(columnId){var cell=this.createTD(columnId);var value;if(columnId==='path')
value=this._path;else if(columnId==='contentType')
value=this._request.mimeType;else if(columnId==='contentLength')
value=(this._request.resourceSize|0).toLocaleString('en-US');else if(columnId==='responseTime')
value=new Date(this._request.endTime*1000).toLocaleString();DataGrid.DataGrid.setElementText(cell,value||'',true);return cell;}};Resources.ServiceWorkerCacheView.RequestView=class extends UI.VBox{constructor(request){super();this._tabbedPane=new UI.TabbedPane();this._tabbedPane.addEventListener(UI.TabbedPane.Events.TabSelected,this._tabSelected,this);this._resourceViewTabSetting=Common.settings.createSetting('cacheStorageViewTab','preview');this._tabbedPane.appendTab('headers',Common.UIString('Headers'),new Network.RequestHeadersView(request));this._tabbedPane.appendTab('preview',Common.UIString('Preview'),new Network.RequestPreviewView(request));this._tabbedPane.show(this.element);}
wasShown(){super.wasShown();this._selectTab();}
_selectTab(tabId){if(!tabId)
tabId=this._resourceViewTabSetting.get();if(!this._tabbedPane.selectTab(tabId))
this._tabbedPane.selectTab('headers');}
_tabSelected(event){if(!event.data.isUserGesture)
return;this._resourceViewTabSetting.set(event.data.tabId);}};;Resources.ServiceWorkersView=class extends UI.VBox{constructor(){super(true);this._reportView=new UI.ReportView(Common.UIString('Service Workers'));this._reportView.show(this.contentElement);this._toolbar=this._reportView.createToolbar();this._toolbar.makeWrappable(false,true);this._sections=new Map();this._manager=null;this._securityOriginManager=null;this._toolbar.appendToolbarItem(MobileThrottling.throttlingManager().createOfflineToolbarCheckbox());var updateOnReloadSetting=Common.settings.createSetting('serviceWorkerUpdateOnReload',false);updateOnReloadSetting.setTitle(Common.UIString('Update on reload'));var forceUpdate=new UI.ToolbarSettingCheckbox(updateOnReloadSetting,Common.UIString('Force update Service Worker on page reload'));this._toolbar.appendToolbarItem(forceUpdate);var bypassServiceWorkerSetting=Common.settings.createSetting('bypassServiceWorker',false);bypassServiceWorkerSetting.setTitle(Common.UIString('Bypass for network'));var fallbackToNetwork=new UI.ToolbarSettingCheckbox(bypassServiceWorkerSetting,Common.UIString('Bypass Service Worker and load resources from the network'));this._toolbar.appendToolbarItem(fallbackToNetwork);this._showAllCheckbox=new UI.ToolbarCheckbox(Common.UIString('Show all'),Common.UIString('Show all Service Workers regardless of the origin'));this._showAllCheckbox.setRightAligned(true);this._showAllCheckbox.inputElement.addEventListener('change',this._updateSectionVisibility.bind(this),false);this._toolbar.appendToolbarItem(this._showAllCheckbox);this._eventListeners=new Map();SDK.targetManager.observeModels(SDK.ServiceWorkerManager,this);}
modelAdded(serviceWorkerManager){if(this._manager)
return;this._manager=serviceWorkerManager;this._securityOriginManager=serviceWorkerManager.target().model(SDK.SecurityOriginManager);for(var registration of this._manager.registrations().values())
this._updateRegistration(registration);this._eventListeners.set(serviceWorkerManager,[this._manager.addEventListener(SDK.ServiceWorkerManager.Events.RegistrationUpdated,this._registrationUpdated,this),this._manager.addEventListener(SDK.ServiceWorkerManager.Events.RegistrationDeleted,this._registrationDeleted,this),this._securityOriginManager.addEventListener(SDK.SecurityOriginManager.Events.SecurityOriginAdded,this._updateSectionVisibility,this),this._securityOriginManager.addEventListener(SDK.SecurityOriginManager.Events.SecurityOriginRemoved,this._updateSectionVisibility,this),]);}
modelRemoved(serviceWorkerManager){if(!this._manager||this._manager!==serviceWorkerManager)
return;Common.EventTarget.removeEventListeners(this._eventListeners.get(serviceWorkerManager));this._eventListeners.delete(serviceWorkerManager);this._manager=null;this._securityOriginManager=null;}
_updateSectionVisibility(){var securityOrigins=new Set(this._securityOriginManager.securityOrigins());var matchingSections=new Set();for(var section of this._sections.values()){if(securityOrigins.has(section._registration.securityOrigin))
matchingSections.add(section._section);}
this._reportView.sortSections((a,b)=>{var aMatching=matchingSections.has(a);var bMatching=matchingSections.has(b);if(aMatching===bMatching)
return a.title().localeCompare(b.title());return aMatching?-1:1;});for(var section of this._sections.values()){if(this._showAllCheckbox.checked()||securityOrigins.has(section._registration.securityOrigin))
section._section.showWidget();else
section._section.hideWidget();}}
_registrationUpdated(event){var registration=(event.data);this._updateRegistration(registration);this._gcRegistrations();}
_gcRegistrations(){var hasNonDeletedRegistrations=false;var securityOrigins=new Set(this._securityOriginManager.securityOrigins());for(var registration of this._manager.registrations().values()){var visible=this._showAllCheckbox.checked()||securityOrigins.has(registration.securityOrigin);if(!visible)
continue;if(!registration.canBeRemoved()){hasNonDeletedRegistrations=true;break;}}
if(!hasNonDeletedRegistrations)
return;for(var registration of this._manager.registrations().values()){var visible=this._showAllCheckbox.checked()||securityOrigins.has(registration.securityOrigin);if(visible&&registration.canBeRemoved())
this._removeRegistrationFromList(registration);}}
_updateRegistration(registration){var section=this._sections.get(registration);if(!section){section=new Resources.ServiceWorkersView.Section((this._manager),this._reportView.appendSection(Resources.ServiceWorkersView._displayScopeURL(registration.scopeURL)),registration);this._sections.set(registration,section);}
this._updateSectionVisibility();section._scheduleUpdate();}
_registrationDeleted(event){var registration=(event.data);this._removeRegistrationFromList(registration);}
_removeRegistrationFromList(registration){var section=this._sections.get(registration);if(section)
section._section.detach();this._sections.delete(registration);}
static _displayScopeURL(scopeURL){var parsedURL=scopeURL.asParsedURL();var path=parsedURL.path;if(path.endsWith('/'))
path=path.substring(0,path.length-1);return parsedURL.host+path;}};Resources.ServiceWorkersView.Section=class{constructor(manager,section,registration){this._manager=manager;this._section=section;this._registration=registration;this._fingerprint=null;this._pushNotificationDataSetting=Common.settings.createLocalSetting('pushData',Common.UIString('Test push message from DevTools.'));this._syncTagNameSetting=Common.settings.createLocalSetting('syncTagName','test-tag-from-devtools');this._toolbar=section.createToolbar();this._toolbar.renderAsLinks();this._updateButton=new UI.ToolbarButton(Common.UIString('Update'),undefined,Common.UIString('Update'));this._updateButton.addEventListener(UI.ToolbarButton.Events.Click,this._updateButtonClicked,this);this._toolbar.appendToolbarItem(this._updateButton);this._deleteButton=new UI.ToolbarButton(Common.UIString('Unregister service worker'),undefined,Common.UIString('Unregister'));this._deleteButton.addEventListener(UI.ToolbarButton.Events.Click,this._unregisterButtonClicked,this);this._toolbar.appendToolbarItem(this._deleteButton);this._sourceField=this._wrapWidget(this._section.appendField(Common.UIString('Source')));this._statusField=this._wrapWidget(this._section.appendField(Common.UIString('Status')));this._clientsField=this._wrapWidget(this._section.appendField(Common.UIString('Clients')));this._createPushNotificationField();this._createSyncNotificationField();this._linkifier=new Components.Linkifier();this._clientInfoCache=new Map();this._throttler=new Common.Throttler(500);}
_createPushNotificationField(){var form=this._wrapWidget(this._section.appendField(Common.UIString('Push'))).createChild('form','service-worker-editor-with-button');var editorContainer=form.createChild('div','service-worker-notification-editor');var button=UI.createTextButton(Common.UIString('Push'));button.type='submit';form.appendChild(button);var editorOptions={lineNumbers:false,lineWrapping:true,autoHeight:true,padBottom:false,mimeType:'application/json'};var editor=new TextEditor.CodeMirrorTextEditor(editorOptions);editor.setText(this._pushNotificationDataSetting.get());editor.element.addEventListener('keydown',e=>{if(e.key==='Tab')
e.consume(false);},true);editor.show(editorContainer);form.addEventListener('submit',e=>{this._push(editor.text()||'');e.consume(true);});}
_createSyncNotificationField(){var form=this._wrapWidget(this._section.appendField(Common.UIString('Sync'))).createChild('form','service-worker-editor-with-button');var editor=form.createChild('input','source-code service-worker-notification-editor');var button=UI.createTextButton(Common.UIString('Sync'));button.type='submit';form.appendChild(button);editor.value=this._syncTagNameSetting.get();editor.placeholder=Common.UIString('Sync tag');form.addEventListener('submit',e=>{this._sync(true,editor.value||'');e.consume(true);});}
_scheduleUpdate(){if(Resources.ServiceWorkersView._noThrottle){this._update();return;}
this._throttler.schedule(this._update.bind(this));}
_targetForVersionId(versionId){var version=this._manager.findVersion(versionId);if(!version||!version.targetId)
return null;return SDK.targetManager.targetById(version.targetId);}
_addVersion(versionsStack,icon,label){var installingEntry=versionsStack.createChild('div','service-worker-version');installingEntry.createChild('div',icon);installingEntry.createChild('span').textContent=label;return installingEntry;}
_updateClientsField(version){this._clientsField.removeChildren();this._section.setFieldVisible(Common.UIString('Clients'),version.controlledClients.length);for(var client of version.controlledClients){var clientLabelText=this._clientsField.createChild('div','service-worker-client');if(this._clientInfoCache.has(client)){this._updateClientInfo(clientLabelText,(this._clientInfoCache.get(client)));}
this._manager.target().targetAgent().getTargetInfo(client).then(this._onClientInfo.bind(this,clientLabelText));}}
_updateSourceField(version){this._sourceField.removeChildren();var fileName=Common.ParsedURL.extractName(version.scriptURL);var name=this._sourceField.createChild('div','report-field-value-filename');name.appendChild(Components.Linkifier.linkifyURL(version.scriptURL,{text:fileName}));if(this._registration.errors.length){var errorsLabel=UI.createLabel(String(this._registration.errors.length),'smallicon-error');errorsLabel.classList.add('link');errorsLabel.addEventListener('click',()=>Common.console.show());name.appendChild(errorsLabel);}
this._sourceField.createChild('div','report-field-value-subtitle').textContent=Common.UIString('Received %s',new Date(version.scriptResponseTime*1000).toLocaleString());}
_update(){var fingerprint=this._registration.fingerprint();if(fingerprint===this._fingerprint)
return Promise.resolve();this._fingerprint=fingerprint;this._toolbar.setEnabled(!this._registration.isDeleted);var versions=this._registration.versionsByMode();var scopeURL=Resources.ServiceWorkersView._displayScopeURL(this._registration.scopeURL);var title=this._registration.isDeleted?Common.UIString('%s - deleted',scopeURL):scopeURL;this._section.setTitle(title);var active=versions.get(SDK.ServiceWorkerVersion.Modes.Active);var waiting=versions.get(SDK.ServiceWorkerVersion.Modes.Waiting);var installing=versions.get(SDK.ServiceWorkerVersion.Modes.Installing);var redundant=versions.get(SDK.ServiceWorkerVersion.Modes.Redundant);this._statusField.removeChildren();var versionsStack=this._statusField.createChild('div','service-worker-version-stack');versionsStack.createChild('div','service-worker-version-stack-bar');if(active){this._updateSourceField(active);var activeEntry=this._addVersion(versionsStack,'service-worker-active-circle',Common.UIString('#%s activated and is %s',active.id,active.runningStatus));if(active.isRunning()||active.isStarting()){createLink(activeEntry,Common.UIString('stop'),this._stopButtonClicked.bind(this,active.id));if(!this._targetForVersionId(active.id))
createLink(activeEntry,Common.UIString('inspect'),this._inspectButtonClicked.bind(this,active.id));}else if(active.isStartable()){createLink(activeEntry,Common.UIString('start'),this._startButtonClicked.bind(this));}
this._updateClientsField(active);}else if(redundant){this._updateSourceField(redundant);var activeEntry=this._addVersion(versionsStack,'service-worker-redundant-circle',Common.UIString('#%s is redundant',redundant.id));this._updateClientsField(redundant);}
if(waiting){var waitingEntry=this._addVersion(versionsStack,'service-worker-waiting-circle',Common.UIString('#%s waiting to activate',waiting.id));createLink(waitingEntry,Common.UIString('skipWaiting'),this._skipButtonClicked.bind(this));waitingEntry.createChild('div','service-worker-subtitle').textContent=Common.UIString('Received %s',new Date(waiting.scriptResponseTime*1000).toLocaleString());if(!this._targetForVersionId(waiting.id)&&(waiting.isRunning()||waiting.isStarting()))
createLink(waitingEntry,Common.UIString('inspect'),this._inspectButtonClicked.bind(this,waiting.id));}
if(installing){var installingEntry=this._addVersion(versionsStack,'service-worker-installing-circle',Common.UIString('#%s installing',installing.id));installingEntry.createChild('div','service-worker-subtitle').textContent=Common.UIString('Received %s',new Date(installing.scriptResponseTime*1000).toLocaleString());if(!this._targetForVersionId(installing.id)&&(installing.isRunning()||installing.isStarting()))
createLink(installingEntry,Common.UIString('inspect'),this._inspectButtonClicked.bind(this,installing.id));}
function createLink(parent,title,listener){var span=parent.createChild('span','link');span.textContent=title;span.addEventListener('click',listener,false);return span;}
return Promise.resolve();}
_unregisterButtonClicked(event){this._manager.deleteRegistration(this._registration.id);}
_updateButtonClicked(event){this._manager.updateRegistration(this._registration.id);}
_push(data){this._pushNotificationDataSetting.set(data);this._manager.deliverPushMessage(this._registration.id,data);}
_sync(lastChance,tag){this._syncTagNameSetting.set(tag);this._manager.dispatchSyncEvent(this._registration.id,tag,lastChance);}
_onClientInfo(element,targetInfo){if(!targetInfo)
return;this._clientInfoCache.set(targetInfo.targetId,targetInfo);this._updateClientInfo(element,targetInfo);}
_updateClientInfo(element,targetInfo){if(targetInfo.type!=='page'&&targetInfo.type==='iframe'){element.createTextChild(Common.UIString('Worker: %s',targetInfo.url));return;}
element.removeChildren();element.createTextChild(targetInfo.url);var focusLabel=element.createChild('label','link');focusLabel.createTextChild('focus');focusLabel.addEventListener('click',this._activateTarget.bind(this,targetInfo.targetId),true);}
_activateTarget(targetId){this._manager.target().targetAgent().activateTarget(targetId);}
_startButtonClicked(){this._manager.startWorker(this._registration.scopeURL);}
_skipButtonClicked(){this._manager.skipWaiting(this._registration.scopeURL);}
_stopButtonClicked(versionId){this._manager.stopWorker(versionId);}
_inspectButtonClicked(versionId){this._manager.inspectWorker(versionId);}
_wrapWidget(container){var shadowRoot=UI.createShadowRootWithCoreStyles(container);UI.appendStyle(shadowRoot,'resources/serviceWorkersView.css');var contentElement=createElement('div');shadowRoot.appendChild(contentElement);return contentElement;}};;Runtime.cachedResources["resources/appManifestView.css"]="/*\n * Copyright 2016 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n/*# sourceURL=resources/appManifestView.css */";Runtime.cachedResources["resources/clearStorageView.css"]="/*\n * Copyright 2016 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n.report-row {\n    display: flex;\n    align-items: center;\n}\n\n.clear-storage-button .report-row {\n    margin: 0 0 0 20px;\n    display: flex;\n}\n\n.link {\n    margin-left: 10px;\n    display: none;\n}\n\n.report-row:hover .link {\n    display: inline;\n}\n\n.usage-breakdown-row {\n    min-width: fit-content;\n}\n\n.usage-breakdown-legend-row {\n    margin: 5px auto;\n}\n\n.usage-breakdown-legend-title {\n    display: inline-block;\n}\n\n.usage-breakdown-legend-value {\n    display: inline-block;\n    width: 70px;\n    text-align: right;\n}\n\n.usage-breakdown-legend-swatch {\n    display: inline-block;\n    width: 11px;\n    height: 11px;\n    margin: 0 6px;\n    position: relative;\n    top: 1px;\n    border: 1px solid rgba(0, 0, 0, 0.2);\n}\n\n/*# sourceURL=resources/clearStorageView.css */";Runtime.cachedResources["resources/indexedDBViews.css"]="/*\n * Copyright (C) 2012 Google Inc. All rights reserved.\n *\n * Redistribution and use in source and binary forms, with or without\n * modification, are permitted provided that the following conditions are\n * met:\n *\n *     * Redistributions of source code must retain the above copyright\n * notice, this list of conditions and the following disclaimer.\n *     * Redistributions in binary form must reproduce the above\n * copyright notice, this list of conditions and the following disclaimer\n * in the documentation and/or other materials provided with the\n * distribution.\n *     * Neither the name of Google Inc. nor the names of its\n * contributors may be used to endorse or promote products derived from\n * this software without specific prior written permission.\n *\n * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n * \"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n */\n\n.indexed-db-database-view {\n    -webkit-user-select: text;\n    margin-top: 5px;\n}\n\n.indexed-db-data-view .data-view-toolbar {\n    position: relative;\n    background-color: #eee;\n    border-bottom: 1px solid #ccc;\n}\n\n.indexed-db-data-view .data-view-toolbar .key-input {\n    margin: auto 0;\n    width: 200px;\n}\n\n.indexed-db-data-view .data-grid {\n    flex: auto;\n}\n\n.indexed-db-data-view .data-grid .data-container tr:nth-last-child(1) {\n    background-color: white;\n}\n\n.indexed-db-data-view .data-grid .data-container tr:nth-last-child(1) td {\n    border: 0;\n}\n\n.indexed-db-data-view .data-grid .data-container tr:nth-last-child(2) td {\n    border-bottom: 1px solid #aaa;\n}\n\n.indexed-db-data-view .section,\n.indexed-db-data-view .section > .header,\n.indexed-db-data-view .section > .header .title {\n    margin: 0;\n    min-height: inherit;\n    line-height: inherit;\n}\n\n.indexed-db-data-view .primitive-value {\n    padding-top: 1px;\n}\n\n.indexed-db-data-view .data-grid .data-container td .section .header .title {\n    white-space: nowrap;\n    text-overflow: ellipsis;\n    overflow: hidden;\n}\n\n.indexed-db-key-path {\n    color: rgb(196, 26, 22);\n    white-space: pre-wrap;\n    unicode-bidi: -webkit-isolate;\n}\n\n/*# sourceURL=resources/indexedDBViews.css */";Runtime.cachedResources["resources/resourcesPanel.css"]="/*\n * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.\n * Copyright (C) 2009 Anthony Ricaud <rik@webkit.org>\n *\n * Redistribution and use in source and binary forms, with or without\n * modification, are permitted provided that the following conditions\n * are met:\n *\n * 1.  Redistributions of source code must retain the above copyright\n *     notice, this list of conditions and the following disclaimer.\n * 2.  Redistributions in binary form must reproduce the above copyright\n *     notice, this list of conditions and the following disclaimer in the\n *     documentation and/or other materials provided with the distribution.\n * 3.  Neither the name of Apple Computer, Inc. (\"Apple\") nor the names of\n *     its contributors may be used to endorse or promote products derived\n *     from this software without specific prior written permission.\n *\n * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS \"AS IS\" AND ANY\n * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY\n * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND\n * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF\n * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n */\n\n.resources-toolbar {\n    border-top: 1px solid #ccc;\n    background-color: #f3f3f3;\n}\n\n.top-resources-toolbar {\n    border-bottom: 1px solid #ccc;\n    background-color: #f3f3f3;\n}\n\nli.selected .base-storage-tree-element-subtitle {\n    color: white;\n}\n\n.base-storage-tree-element-subtitle {\n    padding-left: 2px;\n    color: rgb(80, 80, 80);\n    text-shadow: none;\n}\n\n.resources.panel .status {\n    float: right;\n    height: 16px;\n    margin-top: 1px;\n    margin-left: 4px;\n    line-height: 1em;\n}\n\n.storage-view {\n    display: flex;\n    overflow: hidden;\n}\n\n.storage-view {\n    overflow: hidden;\n}\n\n.storage-view .data-grid:not(.inline) {\n    border: none;\n    flex: auto;\n}\n\n.storage-view .storage-table-error {\n    color: rgb(66%, 33%, 33%);\n    font-size: 24px;\n    font-weight: bold;\n    padding: 10px;\n    display: flex;\n    align-items: center;\n    justify-content: center;\n}\n\n.storage-view.query {\n    padding: 2px 0;\n    overflow-y: overlay;\n    overflow-x: hidden;\n}\n\n.storage-view .filter-bar {\n    border-top: none;\n    border-bottom: 1px solid #dadada;\n}\n\n.database-query-prompt-container {\n    position: relative;\n    padding: 1px 22px 1px 24px;\n}\n\n.database-query-prompt {\n    min-height: 16px;\n    white-space: pre-wrap;\n}\n\n.prompt-icon {\n    position: absolute;\n    display: block;\n    left: 7px;\n    top: 9px;\n    margin-top: -7px;\n    -webkit-user-select: none;\n}\n\n.database-query-prompt-container .prompt-icon {\n    top: 10px;\n}\n\n.database-user-query {\n    position: relative;\n    border-bottom: 1px solid rgb(245, 245, 245);\n    padding: 1px 22px 1px 24px;\n    min-height: 16px;\n    flex-shrink: 0;\n}\n\n.database-query-text {\n    color: rgb(0, 128, 255);\n    -webkit-user-select: text;\n}\n\n.database-query-result {\n    position: relative;\n    padding: 1px 22px 1px 24px;\n    min-height: 16px;\n    margin-left: -24px;\n    padding-right: 0;\n}\n\n.database-query-result.error {\n    color: red;\n    -webkit-user-select: text;\n}\n\n.resource-sidebar-tree-item .icon {\n    content: url(Images/resourcePlainIconSmall.png);\n}\n\n.resource-sidebar-tree-item.resources-type-image .icon {\n    position: relative;\n    background-image: url(Images/resourcePlainIcon.png);\n    background-repeat: no-repeat;\n    content: \"\";\n}\n\n.resources-type-image .image-resource-icon-preview {\n    position: absolute;\n    margin: auto;\n    min-width: 1px;\n    min-height: 1px;\n    top: 2px;\n    bottom: 1px;\n    left: 3px;\n    right: 3px;\n    max-width: 8px;\n    max-height: 11px;\n    overflow: hidden;\n}\n\n.resources-sidebar {\n    padding: 0;\n}\n\n/*# sourceURL=resources/resourcesPanel.css */";Runtime.cachedResources["resources/resourcesSidebar.css"]="/*\n * Copyright 2016 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n.tree-outline {\n    padding-left: 0;\n    color: rgb(90, 90, 90);\n}\n\n.tree-outline > ol {\n    padding-bottom: 10px;\n}\n\n.tree-outline li {\n    min-height: 20px;\n}\n\nli.storage-group-list-item {\n    border-top: 1px solid rgb(230, 230, 230);\n    padding: 10px 8px 6px 8px;\n}\n\nli.storage-group-list-item::before {\n    display: none;\n}\n\n.navigator-tree-item {\n    margin: -3px -7px -3px -7px;\n}\n\n.navigator-file-tree-item {\n    background: linear-gradient(45deg, hsl(0, 0%, 50%), hsl(0, 0%, 70%));\n}\n\n.navigator-folder-tree-item {\n    background: linear-gradient(45deg, hsl(210, 82%, 65%), hsl(210, 82%, 80%));\n}\n\n.navigator-frame-tree-item {\n    background-color: #5a5a5a;\n}\n\n.navigator-script-tree-item {\n    background: linear-gradient(45deg, hsl(48, 70%, 50%), hsl(48, 70%, 70%));\n}\n\n.navigator-stylesheet-tree-item {\n    background: linear-gradient(45deg, hsl(256, 50%, 50%), hsl(256, 50%, 70%));\n}\n\n.navigator-image-tree-item,\n.navigator-font-tree-item {\n    background: linear-gradient(45deg, hsl(109, 33%, 50%), hsl(109, 33%, 70%));\n}\n\n.resource-tree-item {\n    background: rgba(90, 90, 90, .7);\n}\n\n/*# sourceURL=resources/resourcesSidebar.css */";Runtime.cachedResources["resources/serviceWorkerCacheViews.css"]="/*\n * Copyright 2014 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n.service-worker-cache-data-view .data-view-toolbar {\n    position: relative;\n    background-color: #eee;\n    border-bottom: 1px solid #ccc;\n    padding-right: 10px;\n}\n\n.service-worker-cache-data-view .data-view-toolbar .key-input {\n    margin: auto 0;\n    width: 200px;\n}\n\n.service-worker-cache-data-view .data-grid {\n    flex: auto;\n}\n\n.service-worker-cache-data-view .data-grid .data-container tr:nth-last-child(1) td {\n    border: 0;\n}\n\n.service-worker-cache-data-view .data-grid .data-container tr:nth-last-child(2) td {\n    border-bottom: 1px solid #aaa;\n}\n\n.service-worker-cache-data-view .data-grid .data-container tr.selected {\n    background-color: rgb(212, 212, 212);\n    color: inherit;\n}\n\n.service-worker-cache-data-view .data-grid:focus .data-container tr.selected {\n    background-color: rgb(56, 121, 217);\n    color: white;\n}\n\n.service-worker-cache-data-view .section,\n.service-worker-cache-data-view .section > .header,\n.service-worker-cache-data-view .section > .header .title {\n    margin: 0;\n    min-height: inherit;\n    line-height: inherit;\n}\n\n.service-worker-cache-data-view .primitive-value {\n    padding-top: 1px;\n}\n\n.service-worker-cache-data-view .data-grid .data-container td .section .header .title {\n    white-space: nowrap;\n    text-overflow: ellipsis;\n    overflow: hidden;\n}\n\n.cache-preview-panel-resizer {\n    background-color: #eee;\n    height: 4px;\n    border-bottom: 1px solid rgb(64%, 64%, 64%);\n}\n\n/*# sourceURL=resources/serviceWorkerCacheViews.css */";Runtime.cachedResources["resources/serviceWorkersView.css"]="/*\n * Copyright 2015 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n.service-worker-error-stack {\n    max-height: 200px;\n    overflow: auto;\n    display: flex;\n    flex-direction: column;\n    border: 1px solid #ccc;\n    background-color: #fff0f0;\n    color: red;\n    line-height: 18px;\n    margin: 10px 2px 0 -14px;\n    white-space: initial;\n}\n\n.service-worker-error-stack > div {\n    flex: none;\n    padding: 3px 4px;\n}\n\n.service-worker-error-stack > div:not(:last-child) {\n    border-bottom: 1px solid #ffd7d7;\n}\n\n.service-worker-error-stack label {\n    flex: auto;\n}\n\n.service-worker-error-stack .devtools-link {\n    float: right;\n    color: rgb(33%, 33%, 33%);\n    cursor: pointer;\n}\n\n.service-worker-version-stack {\n    position: relative;\n}\n\n.service-worker-version-stack-bar {\n    position: absolute;\n    top: 10px;\n    bottom: 20px;\n    left: 4px;\n    content: \"\";\n    border-left: 1px solid #888;\n    z-index: 0;\n}\n\n.service-worker-version:not(:last-child) {\n    margin-bottom: 7px;\n}\n\n.service-worker-active-circle,\n.service-worker-redundant-circle,\n.service-worker-waiting-circle,\n.service-worker-installing-circle {\n    position: relative;\n    display: inline-block;\n    width: 10px;\n    height: 10px;\n    z-index: 10;\n    margin-right: 5px;\n    border-radius: 50%;\n    border: 1px solid #555;\n}\n\n.service-worker-active-circle {\n    background-color: #50B04F;\n}\n.service-worker-waiting-circle {\n    background-color: #F38E24;\n\n}\n.service-worker-installing-circle {\n    background-color: white;\n}\n\n.service-worker-redundant-circle {\n    background-color: gray;\n}\n\n.service-worker-subtitle {\n    padding-left: 14px;\n    line-height: 14px;\n    color: #888;\n}\n\n.link {\n    margin-left: 10px;\n}\n\n.service-worker-editor-with-button {\n    align-items: baseline;\n    display: flex;\n}\n\n.service-worker-notification-editor {\n    border: solid 1px #d8d8d8;\n    display: flex;\n    flex: auto;\n    margin-right: 4px;\n    max-width: 400px;\n    min-width: 80px;\n}\n\n.service-worker-notification-editor.source-code {\n    /** Simulate CodeMirror that is shown above */\n    padding: 4px;\n}\n\n/*# sourceURL=resources/serviceWorkersView.css */";