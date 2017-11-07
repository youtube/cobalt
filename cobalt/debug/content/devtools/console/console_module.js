Console.ConsoleContextSelector=class{constructor(){this._items=new UI.ListModel();this._dropDown=new UI.SoftDropDown(this._items,this);this._dropDown.setRowHeight(36);this._toolbarItem=new UI.ToolbarItem(this._dropDown.element);this._badgePoolForExecutionContext=new Map();this._toolbarItem.element.classList.add('toolbar-has-dropdown');SDK.targetManager.addModelListener(SDK.RuntimeModel,SDK.RuntimeModel.Events.ExecutionContextCreated,this._onExecutionContextCreated,this);SDK.targetManager.addModelListener(SDK.RuntimeModel,SDK.RuntimeModel.Events.ExecutionContextChanged,this._onExecutionContextChanged,this);SDK.targetManager.addModelListener(SDK.RuntimeModel,SDK.RuntimeModel.Events.ExecutionContextDestroyed,this._onExecutionContextDestroyed,this);SDK.targetManager.addModelListener(SDK.ResourceTreeModel,SDK.ResourceTreeModel.Events.FrameNavigated,this._frameNavigated,this);UI.context.addFlavorChangeListener(SDK.ExecutionContext,this._executionContextChangedExternally,this);UI.context.addFlavorChangeListener(SDK.DebuggerModel.CallFrame,this._callFrameSelectedInUI,this);SDK.targetManager.observeModels(SDK.RuntimeModel,this);SDK.targetManager.addModelListener(SDK.DebuggerModel,SDK.DebuggerModel.Events.CallFrameSelected,this._callFrameSelectedInModel,this);}
toolbarItem(){return this._toolbarItem;}
highlightedItemChanged(from,to,fromElement,toElement){SDK.OverlayModel.hideDOMNodeHighlight();if(to&&to.frameId){var overlayModel=to.target().model(SDK.OverlayModel);if(overlayModel)
overlayModel.highlightFrame(to.frameId);}
if(fromElement)
fromElement.classList.remove('highlighted');if(toElement)
toElement.classList.add('highlighted');}
titleFor(executionContext){var target=executionContext.target();var label=executionContext.label()?target.decorateLabel(executionContext.label()):'';if(executionContext.frameId){var resourceTreeModel=target.model(SDK.ResourceTreeModel);var frame=resourceTreeModel&&resourceTreeModel.frameForId(executionContext.frameId);if(frame)
label=label||frame.displayName();}
label=label||executionContext.origin;return label;}
_depthFor(executionContext){var target=executionContext.target();var depth=0;if(!executionContext.isDefault)
depth++;if(executionContext.frameId){var resourceTreeModel=target.model(SDK.ResourceTreeModel);var frame=resourceTreeModel&&resourceTreeModel.frameForId(executionContext.frameId);while(frame&&frame.parentFrame){depth++;frame=frame.parentFrame;}}
var targetDepth=0;while(target.parentTarget()){if(target.parentTarget().hasJSCapability()){targetDepth++;}else{targetDepth=0;break;}
target=target.parentTarget();}
depth+=targetDepth;return depth;}
_badgeFor(executionContext){if(!executionContext.frameId||!executionContext.isDefault)
return null;var resourceTreeModel=executionContext.target().model(SDK.ResourceTreeModel);var frame=resourceTreeModel&&resourceTreeModel.frameForId(executionContext.frameId);if(!frame)
return null;var badgePool=new ProductRegistry.BadgePool();this._badgePoolForExecutionContext.set(executionContext,badgePool);return badgePool.badgeForFrame(frame);}
_disposeExecutionContextBadge(executionContext){var badgePool=this._badgePoolForExecutionContext.get(executionContext);if(!badgePool)
return;badgePool.reset();this._badgePoolForExecutionContext.delete(executionContext);}
_executionContextCreated(executionContext){if(!executionContext.target().hasJSCapability())
return;this._items.insertWithComparator(executionContext,executionContext.runtimeModel.executionContextComparator());if(executionContext===UI.context.flavor(SDK.ExecutionContext))
this._dropDown.selectItem(executionContext);}
_onExecutionContextCreated(event){var executionContext=(event.data);this._executionContextCreated(executionContext);}
_onExecutionContextChanged(event){var executionContext=(event.data);if(this._items.indexOf(executionContext)===-1)
return;this._executionContextDestroyed(executionContext);this._executionContextCreated(executionContext);}
_executionContextDestroyed(executionContext){var index=this._items.indexOf(executionContext);if(index===-1)
return;this._disposeExecutionContextBadge(executionContext);this._items.remove(index);}
_onExecutionContextDestroyed(event){var executionContext=(event.data);this._executionContextDestroyed(executionContext);}
_executionContextChangedExternally(event){var executionContext=(event.data);this._dropDown.selectItem(executionContext);}
_isTopContext(executionContext){if(!executionContext||!executionContext.isDefault)
return false;var resourceTreeModel=executionContext.target().model(SDK.ResourceTreeModel);var frame=executionContext.frameId&&resourceTreeModel&&resourceTreeModel.frameForId(executionContext.frameId);if(!frame)
return false;return frame.isMainFrame();}
_hasTopContext(){return this._items.some(executionContext=>this._isTopContext(executionContext));}
modelAdded(runtimeModel){runtimeModel.executionContexts().forEach(this._executionContextCreated,this);}
modelRemoved(runtimeModel){for(var i=0;i<this._items.length;i++){if(this._items.at(i).runtimeModel===runtimeModel)
this._executionContextDestroyed(this._items.at(i));}}
createElementForItem(item){var element=createElementWithClass('div');var shadowRoot=UI.createShadowRootWithCoreStyles(element,'console/consoleContextSelector.css');var title=shadowRoot.createChild('div','title');title.createTextChild(this.titleFor(item).trimEnd(100));var subTitle=shadowRoot.createChild('div','subtitle');var badgeElement=this._badgeFor(item);if(badgeElement){badgeElement.classList.add('badge');subTitle.appendChild(badgeElement);}
subTitle.createTextChild(this._subtitleFor(item));element.style.paddingLeft=(8+this._depthFor(item)*15)+'px';return element;}
_subtitleFor(executionContext){var target=executionContext.target();if(executionContext.frameId){var resourceTreeModel=target.model(SDK.ResourceTreeModel);var frame=resourceTreeModel&&resourceTreeModel.frameForId(executionContext.frameId);}
if(!frame||!frame.parentFrame||frame.parentFrame.securityOrigin!==executionContext.origin){var url=executionContext&&executionContext.origin&&executionContext.origin.asParsedURL();if(url)
return url.domain();}
if(frame){var callFrame=frame.findCreationCallFrame(callFrame=>!!callFrame.url);if(callFrame)
return new Common.ParsedURL(callFrame.url).domain();return Common.UIString('IFrame');}
return'';}
isItemSelectable(item){var callFrame=item.debuggerModel.selectedCallFrame();var callFrameContext=callFrame&&callFrame.script.executionContext();return!callFrameContext||item===callFrameContext;}
itemSelected(item){this._toolbarItem.element.classList.toggle('warning',!this._isTopContext(item)&&this._hasTopContext());UI.context.setFlavor(SDK.ExecutionContext,item);}
_callFrameSelectedInUI(){var callFrame=UI.context.flavor(SDK.DebuggerModel.CallFrame);var callFrameContext=callFrame&&callFrame.script.executionContext();if(callFrameContext)
UI.context.setFlavor(SDK.ExecutionContext,callFrameContext);}
_callFrameSelectedInModel(event){var debuggerModel=(event.data);for(var executionContext of this._items){if(executionContext.debuggerModel===debuggerModel){this._disposeExecutionContextBadge(executionContext);this._dropDown.refreshItem(executionContext);}}}
_frameNavigated(event){var frame=(event.data);var runtimeModel=frame.resourceTreeModel().target().model(SDK.RuntimeModel);if(!runtimeModel)
return;for(var executionContext of runtimeModel.executionContexts()){if(frame.id===executionContext.frameId){this._disposeExecutionContextBadge(executionContext);this._dropDown.refreshItem(executionContext);}}}};;Console.ConsoleFilter=class{constructor(name,parsedFilters,executionContext,levelsMask){this.name=name;this.parsedFilters=parsedFilters;this.executionContext=executionContext;this.levelsMask=levelsMask||Console.ConsoleFilter.defaultLevelsFilterValue();}
static allLevelsFilterValue(){var result={};for(var name of Object.values(ConsoleModel.ConsoleMessage.MessageLevel))
result[name]=true;return result;}
static defaultLevelsFilterValue(){var result=Console.ConsoleFilter.allLevelsFilterValue();result[ConsoleModel.ConsoleMessage.MessageLevel.Verbose]=false;return result;}
static singleLevelMask(level){var result={};result[level]=true;return result;}
clone(){var parsedFilters=this.parsedFilters.map(TextUtils.FilterParser.cloneFilter);var levelsMask=Object.assign({},this.levelsMask);return new Console.ConsoleFilter(this.name,parsedFilters,this.executionContext,levelsMask);}
shouldBeVisible(viewMessage){var message=viewMessage.consoleMessage();if(this.executionContext&&(this.executionContext.runtimeModel!==message.runtimeModel()||this.executionContext.id!==message.executionContextId))
return false;if(message.type===ConsoleModel.ConsoleMessage.MessageType.Command||message.type===ConsoleModel.ConsoleMessage.MessageType.Result||message.isGroupMessage())
return true;if(message.level&&!this.levelsMask[(message.level)])
return false;for(var filter of this.parsedFilters){if(!filter.key){if(filter.regex&&viewMessage.matchesFilterRegex(filter.regex)===filter.negative)
return false;if(filter.text&&viewMessage.matchesFilterText(filter.text)===filter.negative)
return false;}else{switch(filter.key){case Console.ConsoleFilter.FilterType.Context:if(!passesFilter(filter,message.context,false))
return false;break;case Console.ConsoleFilter.FilterType.Source:var sourceNameForMessage=message.source?ConsoleModel.ConsoleMessage.MessageSourceDisplayName.get((message.source)):message.source;if(!passesFilter(filter,sourceNameForMessage,true))
return false;break;case Console.ConsoleFilter.FilterType.Url:if(!passesFilter(filter,message.url,false))
return false;break;}}}
return true;function passesFilter(filter,value,exactMatch){if(!filter.text)
return!!value===filter.negative;if(!value)
return!filter.text===!filter.negative;var filterText=(filter.text).toLowerCase();var lowerCaseValue=value.toLowerCase();if(exactMatch&&(lowerCaseValue===filterText)===filter.negative)
return false;if(!exactMatch&&lowerCaseValue.includes(filterText)===filter.negative)
return false;return true;}}};Console.ConsoleFilter.FilterType={Context:'context',Source:'source',Url:'url'};;Console.ConsoleSidebar=class extends UI.VBox{constructor(badgePool){super(true);this.setMinimumSize(125,0);this._enabled=Runtime.experiments.isEnabled('logManagement');this._tree=new UI.TreeOutlineInShadow();this._tree.registerRequiredCSS('console/consoleSidebar.css');this._tree.addEventListener(UI.TreeOutline.Events.ElementSelected,this._selectionChanged.bind(this));this.contentElement.appendChild(this._tree.element);this._selectedTreeElement=null;this._treeElements=[];var Levels=ConsoleModel.ConsoleMessage.MessageLevel;var consoleAPIParsedFilters=[{key:Console.ConsoleFilter.FilterType.Source,text:ConsoleModel.ConsoleMessage.MessageSource.ConsoleAPI,negative:false}];this._appendGroup(Console.ConsoleSidebar._groupSingularName.All,[],Console.ConsoleFilter.allLevelsFilterValue(),UI.Icon.create('mediumicon-list'),badgePool);this._appendGroup(Console.ConsoleSidebar._groupSingularName.ConsoleAPI,consoleAPIParsedFilters,Console.ConsoleFilter.allLevelsFilterValue(),UI.Icon.create('mediumicon-account-circle'),badgePool);this._appendGroup(Console.ConsoleSidebar._groupSingularName.Error,[],Console.ConsoleFilter.singleLevelMask(Levels.Error),UI.Icon.create('mediumicon-error-circle'),badgePool);this._appendGroup(Console.ConsoleSidebar._groupSingularName.Warning,[],Console.ConsoleFilter.singleLevelMask(Levels.Warning),UI.Icon.create('mediumicon-warning-triangle'),badgePool);this._appendGroup(Console.ConsoleSidebar._groupSingularName.Info,[],Console.ConsoleFilter.singleLevelMask(Levels.Info),UI.Icon.create('mediumicon-info-circle'),badgePool);this._appendGroup(Console.ConsoleSidebar._groupSingularName.Verbose,[],Console.ConsoleFilter.singleLevelMask(Levels.Verbose),UI.Icon.create('mediumicon-bug'),badgePool);this._treeElements[0].select();}
_appendGroup(name,parsedFilters,levelsMask,icon,badgePool){var filter=new Console.ConsoleFilter(name,parsedFilters,null,levelsMask);var treeElement=new Console.ConsoleSidebar.FilterTreeElement(filter,icon,badgePool);this._tree.appendChild(treeElement);this._treeElements.push(treeElement);}
clear(){if(!this._enabled)
return;for(var treeElement of this._treeElements)
treeElement.clear();}
onMessageAdded(viewMessage){if(!this._enabled)
return;for(var treeElement of this._treeElements)
treeElement.onMessageAdded(viewMessage);}
shouldBeVisible(viewMessage){if(!this._enabled||!this._selectedTreeElement)
return true;return this._selectedTreeElement._filter.shouldBeVisible(viewMessage);}
_selectionChanged(event){this._selectedTreeElement=(event.data);this.dispatchEventToListeners(Console.ConsoleSidebar.Events.FilterSelected);}};Console.ConsoleSidebar.Events={FilterSelected:Symbol('FilterSelected')};Console.ConsoleSidebar.URLGroupTreeElement=class extends UI.TreeElement{constructor(filter,badge){super(filter.name);this._filter=filter;this._countElement=this.listItemElement.createChild('span','count');var leadingIcons=[UI.Icon.create('largeicon-navigator-file')];if(badge)
leadingIcons.push(badge);this.setLeadingIcons(leadingIcons);this._messageCount=0;}
incrementAndUpdateCounter(){this._messageCount++;this._countElement.textContent=this._messageCount;}};Console.ConsoleSidebar.FilterTreeElement=class extends UI.TreeElement{constructor(filter,icon,badgePool){super(filter.name,true);this._filter=filter;this._badgePool=badgePool;this._urlTreeElements=new Map();this.setLeadingIcons([icon]);this._messageCount=0;this._updateCounter();}
clear(){this._urlTreeElements.clear();this.removeChildren();this._messageCount=0;this._updateCounter();}
_updateCounter(){var prefix=this._messageCount?this._messageCount:Common.UIString('No');var pluralizedName=this._messageCount===1?this._filter.name:Console.ConsoleSidebar._groupPluralNameMap.get(this._filter.name);this.title=`${prefix} ${pluralizedName}`;}
onMessageAdded(viewMessage){var message=viewMessage.consoleMessage();var shouldIncrementCounter=message.type!==ConsoleModel.ConsoleMessage.MessageType.Command&&message.type!==ConsoleModel.ConsoleMessage.MessageType.Result&&!message.isGroupMessage();if(!this._filter.shouldBeVisible(viewMessage)||!shouldIncrementCounter)
return;var child=this._childElement(message.url);child.incrementAndUpdateCounter();this._messageCount++;this._updateCounter();}
_childElement(url){var urlValue=url||null;var child=this._urlTreeElements.get(urlValue);if(child)
return child;var filter=this._filter.clone();var parsedURL=urlValue?urlValue.asParsedURL():null;if(urlValue)
filter.name=parsedURL?parsedURL.displayName:urlValue;else
filter.name=Common.UIString('<other>');filter.parsedFilters.push({key:Console.ConsoleFilter.FilterType.Url,text:urlValue,negative:false});var badge=parsedURL?this._badgePool.badgeForURL(parsedURL):null;child=new Console.ConsoleSidebar.URLGroupTreeElement(filter,badge);if(urlValue)
child.tooltip=urlValue;this._urlTreeElements.set(urlValue,child);this.appendChild(child);return child;}};Console.ConsoleSidebar._groupSingularName={ConsoleAPI:Common.UIString('user message'),All:Common.UIString('message'),Error:Common.UIString('error'),Warning:Common.UIString('warning'),Info:Common.UIString('info'),Verbose:Common.UIString('verbose')};Console.ConsoleSidebar._groupPluralNameMap=new Map([[Console.ConsoleSidebar._groupSingularName.ConsoleAPI,Common.UIString('user messages')],[Console.ConsoleSidebar._groupSingularName.All,Common.UIString('messages')],[Console.ConsoleSidebar._groupSingularName.Error,Common.UIString('errors')],[Console.ConsoleSidebar._groupSingularName.Warning,Common.UIString('warnings')],[Console.ConsoleSidebar._groupSingularName.Info,Common.UIString('info')],[Console.ConsoleSidebar._groupSingularName.Verbose,Common.UIString('verbose')]]);;Console.ConsoleViewport=class{constructor(provider){this.element=createElement('div');this.element.style.overflow='auto';this._topGapElement=this.element.createChild('div');this._topGapElement.style.height='0px';this._topGapElement.style.color='transparent';this._contentElement=this.element.createChild('div');this._bottomGapElement=this.element.createChild('div');this._bottomGapElement.style.height='0px';this._bottomGapElement.style.color='transparent';this._topGapElement.textContent='\uFEFF';this._bottomGapElement.textContent='\uFEFF';this._provider=provider;this.element.addEventListener('scroll',this._onScroll.bind(this),false);this.element.addEventListener('copy',this._onCopy.bind(this),false);this.element.addEventListener('dragstart',this._onDragStart.bind(this),false);this._firstActiveIndex=0;this._lastActiveIndex=-1;this._renderedItems=[];this._anchorSelection=null;this._headSelection=null;this._itemCount=0;this._cumulativeHeights=new Int32Array(0);this._observer=new MutationObserver(this.refresh.bind(this));this._observerConfig={childList:true,subtree:true};}
stickToBottom(){return this._stickToBottom;}
setStickToBottom(value){this._stickToBottom=value;if(this._stickToBottom)
this._observer.observe(this._contentElement,this._observerConfig);else
this._observer.disconnect();}
_onCopy(event){var text=this._selectedText();if(!text)
return;event.preventDefault();event.clipboardData.setData('text/plain',text);}
_onDragStart(event){var text=this._selectedText();if(!text)
return false;event.dataTransfer.clearData();event.dataTransfer.setData('text/plain',text);event.dataTransfer.effectAllowed='copy';return true;}
contentElement(){return this._contentElement;}
invalidate(){delete this._cachedProviderElements;this._itemCount=this._provider.itemCount();this._rebuildCumulativeHeights();this.refresh();}
_providerElement(index){if(!this._cachedProviderElements)
this._cachedProviderElements=new Array(this._itemCount);var element=this._cachedProviderElements[index];if(!element){element=this._provider.itemElement(index);this._cachedProviderElements[index]=element;}
return element;}
_rebuildCumulativeHeights(){var firstActiveIndex=this._firstActiveIndex;var lastActiveIndex=this._lastActiveIndex;var height=0;this._cumulativeHeights=new Int32Array(this._itemCount);for(var i=0;i<this._itemCount;++i){if(firstActiveIndex<=i&&i-firstActiveIndex<this._renderedItems.length&&i<=lastActiveIndex)
height+=this._renderedItems[i-firstActiveIndex].element().offsetHeight;else
height+=this._provider.fastHeight(i);this._cumulativeHeights[i]=height;}}
_cachedItemHeight(index){return index===0?this._cumulativeHeights[0]:this._cumulativeHeights[index]-this._cumulativeHeights[index-1];}
_isSelectionBackwards(selection){if(!selection||!selection.rangeCount)
return false;var range=document.createRange();range.setStart(selection.anchorNode,selection.anchorOffset);range.setEnd(selection.focusNode,selection.focusOffset);return range.collapsed;}
_createSelectionModel(itemIndex,node,offset){return{item:itemIndex,node:node,offset:offset};}
_updateSelectionModel(selection){var range=selection&&selection.rangeCount?selection.getRangeAt(0):null;if(!range||selection.isCollapsed||!this.element.hasSelection()){this._headSelection=null;this._anchorSelection=null;return false;}
var firstSelected=Number.MAX_VALUE;var lastSelected=-1;var hasVisibleSelection=false;for(var i=0;i<this._renderedItems.length;++i){if(range.intersectsNode(this._renderedItems[i].element())){var index=i+this._firstActiveIndex;firstSelected=Math.min(firstSelected,index);lastSelected=Math.max(lastSelected,index);hasVisibleSelection=true;}}
if(hasVisibleSelection){firstSelected=this._createSelectionModel(firstSelected,(range.startContainer),range.startOffset);lastSelected=this._createSelectionModel(lastSelected,(range.endContainer),range.endOffset);}
var topOverlap=range.intersectsNode(this._topGapElement)&&this._topGapElement._active;var bottomOverlap=range.intersectsNode(this._bottomGapElement)&&this._bottomGapElement._active;if(!topOverlap&&!bottomOverlap&&!hasVisibleSelection){this._headSelection=null;this._anchorSelection=null;return false;}
if(!this._anchorSelection||!this._headSelection){this._anchorSelection=this._createSelectionModel(0,this.element,0);this._headSelection=this._createSelectionModel(this._itemCount-1,this.element,this.element.children.length);this._selectionIsBackward=false;}
var isBackward=this._isSelectionBackwards(selection);var startSelection=this._selectionIsBackward?this._headSelection:this._anchorSelection;var endSelection=this._selectionIsBackward?this._anchorSelection:this._headSelection;if(topOverlap&&bottomOverlap&&hasVisibleSelection){firstSelected=firstSelected.item<startSelection.item?firstSelected:startSelection;lastSelected=lastSelected.item>endSelection.item?lastSelected:endSelection;}else if(!hasVisibleSelection){firstSelected=startSelection;lastSelected=endSelection;}else if(topOverlap){firstSelected=isBackward?this._headSelection:this._anchorSelection;}else if(bottomOverlap){lastSelected=isBackward?this._anchorSelection:this._headSelection;}
if(isBackward){this._anchorSelection=lastSelected;this._headSelection=firstSelected;}else{this._anchorSelection=firstSelected;this._headSelection=lastSelected;}
this._selectionIsBackward=isBackward;return true;}
_restoreSelection(selection){var anchorElement=null;var anchorOffset;if(this._firstActiveIndex<=this._anchorSelection.item&&this._anchorSelection.item<=this._lastActiveIndex){anchorElement=this._anchorSelection.node;anchorOffset=this._anchorSelection.offset;}else{if(this._anchorSelection.item<this._firstActiveIndex)
anchorElement=this._topGapElement;else if(this._anchorSelection.item>this._lastActiveIndex)
anchorElement=this._bottomGapElement;anchorOffset=this._selectionIsBackward?1:0;}
var headElement=null;var headOffset;if(this._firstActiveIndex<=this._headSelection.item&&this._headSelection.item<=this._lastActiveIndex){headElement=this._headSelection.node;headOffset=this._headSelection.offset;}else{if(this._headSelection.item<this._firstActiveIndex)
headElement=this._topGapElement;else if(this._headSelection.item>this._lastActiveIndex)
headElement=this._bottomGapElement;headOffset=this._selectionIsBackward?0:1;}
selection.setBaseAndExtent(anchorElement,anchorOffset,headElement,headOffset);}
refresh(){this._observer.disconnect();this._innerRefresh();if(this._stickToBottom)
this._observer.observe(this._contentElement,this._observerConfig);}
_innerRefresh(){if(!this._visibleHeight())
return;if(!this._itemCount){for(var i=0;i<this._renderedItems.length;++i)
this._renderedItems[i].willHide();this._renderedItems=[];this._contentElement.removeChildren();this._topGapElement.style.height='0px';this._bottomGapElement.style.height='0px';this._firstActiveIndex=-1;this._lastActiveIndex=-1;return;}
var selection=this.element.getComponentSelection();var shouldRestoreSelection=this._updateSelectionModel(selection);var visibleFrom=this.element.scrollTop;var visibleHeight=this._visibleHeight();for(var i=0;i<this._renderedItems.length;++i){var cachedItemHeight=this._cachedItemHeight(this._firstActiveIndex+i);if(Math.abs(cachedItemHeight-this._renderedItems[i].element().offsetHeight)>1){this._rebuildCumulativeHeights();break;}}
var activeHeight=visibleHeight*2;if(this._stickToBottom){this._firstActiveIndex=Math.max(this._itemCount-Math.ceil(activeHeight/this._provider.minimumRowHeight()),0);this._lastActiveIndex=this._itemCount-1;}else{this._firstActiveIndex=Math.max(Int32Array.prototype.lowerBound.call(this._cumulativeHeights,visibleFrom+1-(activeHeight-visibleHeight)/2),0);this._lastActiveIndex=this._firstActiveIndex+Math.ceil(activeHeight/this._provider.minimumRowHeight())-1;this._lastActiveIndex=Math.min(this._lastActiveIndex,this._itemCount-1);}
var topGapHeight=this._cumulativeHeights[this._firstActiveIndex-1]||0;var bottomGapHeight=this._cumulativeHeights[this._cumulativeHeights.length-1]-this._cumulativeHeights[this._lastActiveIndex];function prepare(){this._topGapElement.style.height=topGapHeight+'px';this._bottomGapElement.style.height=bottomGapHeight+'px';this._topGapElement._active=!!topGapHeight;this._bottomGapElement._active=!!bottomGapHeight;this._contentElement.style.setProperty('height','10000000px');}
this._partialViewportUpdate(prepare.bind(this));this._contentElement.style.removeProperty('height');if(shouldRestoreSelection)
this._restoreSelection(selection);if(this._stickToBottom)
this.element.scrollTop=10000000;}
_partialViewportUpdate(prepare){var itemsToRender=new Set();for(var i=this._firstActiveIndex;i<=this._lastActiveIndex;++i)
itemsToRender.add(this._providerElement(i));var willBeHidden=this._renderedItems.filter(item=>!itemsToRender.has(item));for(var i=0;i<willBeHidden.length;++i)
willBeHidden[i].willHide();prepare();for(var i=0;i<willBeHidden.length;++i)
willBeHidden[i].element().remove();var wasShown=[];var anchor=this._contentElement.firstChild;for(var viewportElement of itemsToRender){var element=viewportElement.element();if(element!==anchor){var shouldCallWasShown=!element.parentElement;if(shouldCallWasShown)
wasShown.push(viewportElement);this._contentElement.insertBefore(element,anchor);}else{anchor=anchor.nextSibling;}}
for(var i=0;i<wasShown.length;++i)
wasShown[i].wasShown();this._renderedItems=Array.from(itemsToRender);}
_selectedText(){this._updateSelectionModel(this.element.getComponentSelection());if(!this._headSelection||!this._anchorSelection)
return null;var startSelection=null;var endSelection=null;if(this._selectionIsBackward){startSelection=this._headSelection;endSelection=this._anchorSelection;}else{startSelection=this._anchorSelection;endSelection=this._headSelection;}
var textLines=[];for(var i=startSelection.item;i<=endSelection.item;++i){var element=this._providerElement(i).element();var lineContent=element.childTextNodes().map(Components.Linkifier.untruncatedNodeText).join('');textLines.push(lineContent);}
var endSelectionElement=this._providerElement(endSelection.item).element();if(endSelection.node&&endSelection.node.isSelfOrDescendant(endSelectionElement)){var itemTextOffset=this._textOffsetInNode(endSelectionElement,endSelection.node,endSelection.offset);textLines[textLines.length-1]=textLines.peekLast().substring(0,itemTextOffset);}
var startSelectionElement=this._providerElement(startSelection.item).element();if(startSelection.node&&startSelection.node.isSelfOrDescendant(startSelectionElement)){var itemTextOffset=this._textOffsetInNode(startSelectionElement,startSelection.node,startSelection.offset);textLines[0]=textLines[0].substring(itemTextOffset);}
return textLines.join('\n');}
_textOffsetInNode(itemElement,selectionNode,offset){if(selectionNode.nodeType!==Node.TEXT_NODE){if(offset<selectionNode.childNodes.length){selectionNode=(selectionNode.childNodes.item(offset));offset=0;}else{offset=selectionNode.textContent.length;}}
var chars=0;var node=itemElement;while((node=node.traverseNextNode(itemElement))&&node!==selectionNode){if(node.nodeType!==Node.TEXT_NODE||node.parentElement.nodeName==='STYLE'||node.parentElement.nodeName==='SCRIPT')
continue;chars+=Components.Linkifier.untruncatedNodeText(node).length;}
var untruncatedContainerLength=Components.Linkifier.untruncatedNodeText(selectionNode).length;if(offset>0&&untruncatedContainerLength!==selectionNode.textContent.length)
offset=untruncatedContainerLength;return chars+offset;}
_onScroll(event){this.refresh();}
firstVisibleIndex(){var firstVisibleIndex=Math.max(Int32Array.prototype.lowerBound.call(this._cumulativeHeights,this.element.scrollTop+1),0);return Math.max(firstVisibleIndex,this._firstActiveIndex);}
lastVisibleIndex(){var lastVisibleIndex;if(this._stickToBottom){lastVisibleIndex=this._itemCount-1;}else{lastVisibleIndex=this.firstVisibleIndex()+Math.ceil(this._visibleHeight()/this._provider.minimumRowHeight())-1;}
return Math.min(lastVisibleIndex,this._lastActiveIndex);}
renderedElementAt(index){if(index<this._firstActiveIndex)
return null;if(index>this._lastActiveIndex)
return null;return this._renderedItems[index-this._firstActiveIndex].element();}
scrollItemIntoView(index,makeLast){var firstVisibleIndex=this.firstVisibleIndex();var lastVisibleIndex=this.lastVisibleIndex();if(index>firstVisibleIndex&&index<lastVisibleIndex)
return;if(makeLast)
this.forceScrollItemToBeLast(index);else if(index<=firstVisibleIndex)
this.forceScrollItemToBeFirst(index);else if(index>=lastVisibleIndex)
this.forceScrollItemToBeLast(index);}
forceScrollItemToBeFirst(index){this.setStickToBottom(false);this.element.scrollTop=index>0?this._cumulativeHeights[index-1]:0;if(this.element.isScrolledToBottom())
this.setStickToBottom(true);this.refresh();}
forceScrollItemToBeLast(index){this.setStickToBottom(false);this.element.scrollTop=this._cumulativeHeights[index]-this._visibleHeight();if(this.element.isScrolledToBottom())
this.setStickToBottom(true);this.refresh();}
_visibleHeight(){return this.element.offsetHeight;}};Console.ConsoleViewportProvider=function(){};Console.ConsoleViewportProvider.prototype={fastHeight(index){return 0;},itemCount(){return 0;},minimumRowHeight(){return 0;},itemElement(index){return null;}};Console.ConsoleViewportElement=function(){};Console.ConsoleViewportElement.prototype={willHide(){},wasShown(){},element(){},};;Console.ConsoleViewMessage=class{constructor(consoleMessage,linkifier,badgePool,nestingLevel){this._message=consoleMessage;this._linkifier=linkifier;this._badgePool=badgePool;this._repeatCount=1;this._closeGroupDecorationCount=0;this._nestingLevel=nestingLevel;this._dataGrid=null;this._previewFormatter=new ObjectUI.RemoteObjectPreviewFormatter();this._searchRegex=null;this._messageLevelIcon=null;}
element(){return this.toMessageElement();}
wasShown(){if(this._dataGrid)
this._dataGrid.updateWidths();this._isVisible=true;}
onResize(){if(!this._isVisible)
return;if(this._dataGrid)
this._dataGrid.onResize();}
willHide(){this._isVisible=false;this._cachedHeight=this.contentElement().offsetHeight;}
fastHeight(){if(this._cachedHeight)
return this._cachedHeight;const defaultConsoleRowHeight=19;if(this._message.type===ConsoleModel.ConsoleMessage.MessageType.Table){var table=this._message.parameters[0];if(table&&table.preview)
return defaultConsoleRowHeight*table.preview.properties.length;}
return defaultConsoleRowHeight;}
consoleMessage(){return this._message;}
_buildTableMessage(){var formattedMessage=createElement('span');UI.appendStyle(formattedMessage,'object_ui/objectValue.css');formattedMessage.className='source-code';var anchorElement=this._buildMessageAnchor();if(anchorElement)
formattedMessage.appendChild(anchorElement);var badgeElement=this._buildMessageBadge();if(badgeElement)
formattedMessage.appendChild(badgeElement);var table=this._message.parameters&&this._message.parameters.length?this._message.parameters[0]:null;if(table)
table=this._parameterToRemoteObject(table);if(!table||!table.preview)
return formattedMessage;var rawValueColumnSymbol=Symbol('rawValueColumn');var columnNames=[];var preview=table.preview;var rows=[];for(var i=0;i<preview.properties.length;++i){var rowProperty=preview.properties[i];var rowSubProperties;if(rowProperty.valuePreview)
rowSubProperties=rowProperty.valuePreview.properties;else if(rowProperty.value)
rowSubProperties=[{name:rawValueColumnSymbol,type:rowProperty.type,value:rowProperty.value}];else
continue;var rowValue={};const maxColumnsToRender=20;for(var j=0;j<rowSubProperties.length;++j){var cellProperty=rowSubProperties[j];var columnRendered=columnNames.indexOf(cellProperty.name)!==-1;if(!columnRendered){if(columnNames.length===maxColumnsToRender)
continue;columnRendered=true;columnNames.push(cellProperty.name);}
if(columnRendered){var cellElement=this._renderPropertyPreviewOrAccessor(table,[rowProperty,cellProperty]);cellElement.classList.add('console-message-nowrap-below');rowValue[cellProperty.name]=cellElement;}}
rows.push([rowProperty.name,rowValue]);}
var flatValues=[];for(var i=0;i<rows.length;++i){var rowName=rows[i][0];var rowValue=rows[i][1];flatValues.push(rowName);for(var j=0;j<columnNames.length;++j)
flatValues.push(rowValue[columnNames[j]]);}
columnNames.unshift(Common.UIString('(index)'));var columnDisplayNames=columnNames.map(name=>name===rawValueColumnSymbol?Common.UIString('Value'):name);if(flatValues.length){this._dataGrid=DataGrid.SortableDataGrid.create(columnDisplayNames,flatValues);this._dataGrid.setStriped(true);var formattedResult=createElementWithClass('span','console-message-text');var tableElement=formattedResult.createChild('div','console-message-formatted-table');var dataGridContainer=tableElement.createChild('span');tableElement.appendChild(this._formatParameter(table,true,false));dataGridContainer.appendChild(this._dataGrid.element);formattedMessage.appendChild(formattedResult);this._dataGrid.renderInline();}
return formattedMessage;}
_buildMessage(){var messageElement;var messageText=this._message.messageText;if(this._message.source===ConsoleModel.ConsoleMessage.MessageSource.ConsoleAPI){switch(this._message.type){case ConsoleModel.ConsoleMessage.MessageType.Trace:messageElement=this._format(this._message.parameters||['console.trace']);break;case ConsoleModel.ConsoleMessage.MessageType.Clear:messageElement=createElementWithClass('span','console-info');if(Common.moduleSetting('preserveConsoleLog').get())
messageElement.textContent=Common.UIString('console.clear() was prevented due to \'Preserve log\'');else
messageElement.textContent=Common.UIString('Console was cleared');messageElement.title=Common.UIString('Clear all messages with '+UI.shortcutRegistry.shortcutTitleForAction('console.clear'));break;case ConsoleModel.ConsoleMessage.MessageType.Assert:var args=[Common.UIString('Assertion failed:')];if(this._message.parameters)
args=args.concat(this._message.parameters);messageElement=this._format(args);break;case ConsoleModel.ConsoleMessage.MessageType.Dir:var obj=this._message.parameters?this._message.parameters[0]:undefined;var args=['%O',obj];messageElement=this._format(args);break;case ConsoleModel.ConsoleMessage.MessageType.Profile:case ConsoleModel.ConsoleMessage.MessageType.ProfileEnd:messageElement=this._format([messageText]);break;default:if(this._message.parameters&&this._message.parameters.length===1&&this._message.parameters[0].type==='string')
messageElement=this._tryFormatAsError((this._message.parameters[0].value));var args=this._message.parameters||[messageText];messageElement=messageElement||this._format(args);}}else if(this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Network){var request=this._message.request;if(request){messageElement=createElement('span');if(this._message.level===ConsoleModel.ConsoleMessage.MessageLevel.Error){messageElement.createTextChild(request.requestMethod+' ');messageElement.appendChild(Components.Linkifier.linkifyRevealable(request,request.url(),request.url()));if(request.failed)
messageElement.createTextChildren(' ',request.localizedFailDescription);else
messageElement.createTextChildren(' ',String(request.statusCode),' (',request.statusText,')');}else{var fragment=Console.ConsoleViewMessage._linkifyWithCustomLinkifier(messageText,title=>Components.Linkifier.linkifyRevealable((request),title,request.url()));messageElement.appendChild(fragment);}}else{messageElement=this._format([messageText]);}}else{var messageInParameters=this._message.parameters&&messageText===(this._message.parameters[0]);if(this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Violation)
messageText=Common.UIString('[Violation] %s',messageText);else if(this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Intervention)
messageText=Common.UIString('[Intervention] %s',messageText);else if(this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Deprecation)
messageText=Common.UIString('[Deprecation] %s',messageText);var args=this._message.parameters||[messageText];if(messageInParameters)
args[0]=messageText;messageElement=this._format(args);}
messageElement.classList.add('console-message-text');var formattedMessage=createElement('span');UI.appendStyle(formattedMessage,'object_ui/objectValue.css');formattedMessage.className='source-code';var anchorElement=this._buildMessageAnchor();if(anchorElement)
formattedMessage.appendChild(anchorElement);var badgeElement=this._buildMessageBadge();if(badgeElement)
formattedMessage.appendChild(badgeElement);formattedMessage.appendChild(messageElement);return formattedMessage;}
_buildMessageAnchor(){var anchorElement=null;if(this._message.source!==ConsoleModel.ConsoleMessage.MessageSource.Network||this._message.request){if(this._message.scriptId){anchorElement=this._linkifyScriptId(this._message.scriptId,this._message.url||'',this._message.line,this._message.column);}else if(this._message.stackTrace&&this._message.stackTrace.callFrames.length){anchorElement=this._linkifyStackTraceTopFrame(this._message.stackTrace);}else if(this._message.url&&this._message.url!=='undefined'){anchorElement=this._linkifyLocation(this._message.url,this._message.line,this._message.column);}}else if(this._message.url){anchorElement=Components.Linkifier.linkifyURL(this._message.url,{maxLength:Console.ConsoleViewMessage.MaxLengthForLinks});}
if(anchorElement){var anchorWrapperElement=createElementWithClass('span','console-message-anchor');anchorWrapperElement.appendChild(anchorElement);anchorWrapperElement.createTextChild(' ');return anchorWrapperElement;}
return null;}
_buildMessageBadge(){var badgeElement=this._badgeElement();if(!badgeElement)
return null;badgeElement.classList.add('console-message-badge');return badgeElement;}
_badgeElement(){if(this._message._url)
return this._badgePool.badgeForURL(new Common.ParsedURL(this._message._url));if(this._message.stackTrace){var stackTrace=this._message.stackTrace;while(stackTrace){for(var callFrame of this._message.stackTrace.callFrames){if(callFrame.url)
return this._badgePool.badgeForURL(new Common.ParsedURL(callFrame.url));}
stackTrace=stackTrace.parent;}}
if(!this._message.executionContextId)
return null;var runtimeModel=this._message.runtimeModel();if(!runtimeModel)
return null;var executionContext=runtimeModel.executionContext(this._message.executionContextId);if(!executionContext||!executionContext.frameId)
return null;var resourceTreeModel=executionContext.target().model(SDK.ResourceTreeModel);if(!resourceTreeModel)
return null;var frame=resourceTreeModel.frameForId(executionContext.frameId);if(!frame||!frame.parentFrame)
return null;return this._badgePool.badgeForFrame(frame);}
_buildMessageWithStackTrace(){var toggleElement=createElementWithClass('div','console-message-stack-trace-toggle');var contentElement=toggleElement.createChild('div','console-message-stack-trace-wrapper');var messageElement=this._buildMessage();var icon=UI.Icon.create('smallicon-triangle-right','console-message-expand-icon');var clickableElement=contentElement.createChild('div');clickableElement.appendChild(icon);clickableElement.appendChild(messageElement);var stackTraceElement=contentElement.createChild('div');var stackTracePreview=Components.DOMPresentationUtils.buildStackTracePreviewContents(this._message.runtimeModel().target(),this._linkifier,this._message.stackTrace);stackTraceElement.appendChild(stackTracePreview);stackTraceElement.classList.add('hidden');function expandStackTrace(expand){icon.setIconType(expand?'smallicon-triangle-down':'smallicon-triangle-right');stackTraceElement.classList.toggle('hidden',!expand);}
function toggleStackTrace(event){if(event.target.hasSelection())
return;expandStackTrace(stackTraceElement.classList.contains('hidden'));event.consume();}
clickableElement.addEventListener('click',toggleStackTrace,false);if(this._message.type===ConsoleModel.ConsoleMessage.MessageType.Trace)
expandStackTrace(true);toggleElement._expandStackTraceForTest=expandStackTrace.bind(null,true);return toggleElement;}
_linkifyLocation(url,lineNumber,columnNumber){if(!this._message.runtimeModel())
return null;return this._linkifier.linkifyScriptLocation(this._message.runtimeModel().target(),null,url,lineNumber,columnNumber);}
_linkifyStackTraceTopFrame(stackTrace){if(!this._message.runtimeModel())
return null;return this._linkifier.linkifyStackTraceTopFrame(this._message.runtimeModel().target(),stackTrace);}
_linkifyScriptId(scriptId,url,lineNumber,columnNumber){if(!this._message.runtimeModel())
return null;return this._linkifier.linkifyScriptLocation(this._message.runtimeModel().target(),scriptId,url,lineNumber,columnNumber);}
_parameterToRemoteObject(parameter){if(parameter instanceof SDK.RemoteObject)
return parameter;var runtimeModel=this._message.runtimeModel();if(!runtimeModel)
return SDK.RemoteObject.fromLocalObject(parameter);if(typeof parameter==='object')
return runtimeModel.createRemoteObject(parameter);return runtimeModel.createRemoteObjectFromPrimitiveValue(parameter);}
_format(rawParameters){var formattedResult=createElement('span');if(!rawParameters.length)
return formattedResult;var parameters=[];for(var i=0;i<rawParameters.length;++i)
parameters[i]=this._parameterToRemoteObject(rawParameters[i]);var shouldFormatMessage=SDK.RemoteObject.type(((parameters))[0])==='string'&&(this._message.type!==ConsoleModel.ConsoleMessage.MessageType.Result||this._message.level===ConsoleModel.ConsoleMessage.MessageLevel.Error);if(shouldFormatMessage){var result=this._formatWithSubstitutionString((parameters[0].description),parameters.slice(1),formattedResult);parameters=result.unusedSubstitutions;if(parameters.length)
formattedResult.createTextChild(' ');}
for(var i=0;i<parameters.length;++i){if(shouldFormatMessage&&parameters[i].type==='string')
formattedResult.appendChild(Console.ConsoleViewMessage._linkifyStringAsFragment(parameters[i].description));else
formattedResult.appendChild(this._formatParameter(parameters[i],false,true));if(i<parameters.length-1)
formattedResult.createTextChild(' ');}
return formattedResult;}
_formatParameter(output,forceObjectFormat,includePreview){if(output.customPreview())
return(new ObjectUI.CustomPreviewComponent(output)).element;var type=forceObjectFormat?'object':(output.subtype||output.type);var element;switch(type){case'error':element=this._formatParameterAsError(output);break;case'function':element=this._formatParameterAsFunction(output,includePreview);break;case'array':case'arraybuffer':case'blob':case'dataview':case'generator':case'iterator':case'map':case'object':case'promise':case'proxy':case'set':case'typedarray':case'weakmap':case'weakset':element=this._formatParameterAsObject(output,includePreview);break;case'node':element=output.isNode()?this._formatParameterAsNode(output):this._formatParameterAsObject(output,false);break;case'string':element=this._formatParameterAsString(output);break;case'boolean':case'date':case'null':case'number':case'regexp':case'symbol':case'undefined':element=this._formatParameterAsValue(output);break;default:element=this._formatParameterAsValue(output);console.error('Tried to format remote object of unknown type.');}
element.classList.add('object-value-'+type);element.classList.add('source-code');return element;}
_formatParameterAsValue(obj){var result=createElement('span');result.createTextChild(obj.description||'');if(obj.objectId)
result.addEventListener('contextmenu',this._contextMenuEventFired.bind(this,obj),false);return result;}
_formatParameterAsObject(obj,includePreview){var titleElement=createElement('span');if(includePreview&&obj.preview){titleElement.classList.add('console-object-preview');this._previewFormatter.appendObjectPreview(titleElement,obj.preview,false);}else if(obj.type==='function'){ObjectUI.ObjectPropertiesSection.formatObjectAsFunction(obj,titleElement,false);titleElement.classList.add('object-value-function');}else{titleElement.createTextChild(obj.description||'');}
if(!obj.hasChildren||obj.customPreview())
return titleElement;var note=titleElement.createChild('span','object-state-note');note.classList.add('info-note');note.title=Common.UIString('Value below was evaluated just now.');var section=new ObjectUI.ObjectPropertiesSection(obj,titleElement,this._linkifier);section.element.classList.add('console-view-object-properties-section');section.enableContextMenu();return section.element;}
_formatParameterAsFunction(func,includePreview){var result=createElement('span');SDK.RemoteFunction.objectAsFunction(func).targetFunction().then(formatTargetFunction.bind(this));return result;function formatTargetFunction(targetFunction){var functionElement=createElement('span');ObjectUI.ObjectPropertiesSection.formatObjectAsFunction(targetFunction,functionElement,true,includePreview);result.appendChild(functionElement);if(targetFunction!==func){var note=result.createChild('span','object-info-state-note');note.title=Common.UIString('Function was resolved from bound function.');}
result.addEventListener('contextmenu',this._contextMenuEventFired.bind(this,targetFunction),false);}}
_contextMenuEventFired(obj,event){var contextMenu=new UI.ContextMenu(event);contextMenu.appendApplicableItems(obj);contextMenu.show();}
_renderPropertyPreviewOrAccessor(object,propertyPath){var property=propertyPath.peekLast();if(property.type==='accessor')
return this._formatAsAccessorProperty(object,propertyPath.map(property=>property.name),false);return this._previewFormatter.renderPropertyPreview(property.type,(property.subtype),property.value);}
_formatParameterAsNode(remoteObject){var result=createElement('span');var domModel=remoteObject.runtimeModel().target().model(SDK.DOMModel);if(!domModel)
return result;domModel.pushObjectAsNodeToFrontend(remoteObject).then(node=>{if(!node){result.appendChild(this._formatParameterAsObject(remoteObject,false));return;}
Common.Renderer.renderPromise(node).then(rendererElement=>{result.appendChild(rendererElement);this._formattedParameterAsNodeForTest();});});return result;}
_formattedParameterAsNodeForTest(){}
_formatParameterAsString(output){var span=createElement('span');span.appendChild(Console.ConsoleViewMessage._linkifyStringAsFragment(output.description||''));var result=createElement('span');result.createChild('span','object-value-string-quote').textContent='"';result.appendChild(span);result.createChild('span','object-value-string-quote').textContent='"';return result;}
_formatParameterAsError(output){var result=createElement('span');var errorSpan=this._tryFormatAsError(output.description||'');result.appendChild(errorSpan?errorSpan:Console.ConsoleViewMessage._linkifyStringAsFragment(output.description||''));return result;}
_formatAsArrayEntry(output){return this._previewFormatter.renderPropertyPreview(output.type,output.subtype,output.description);}
_formatAsAccessorProperty(object,propertyPath,isArrayEntry){var rootElement=ObjectUI.ObjectPropertyTreeElement.createRemoteObjectAccessorPropertySpan(object,propertyPath,onInvokeGetterClick.bind(this));function onInvokeGetterClick(result,wasThrown){if(!result)
return;rootElement.removeChildren();if(wasThrown){var element=rootElement.createChild('span');element.textContent=Common.UIString('<exception>');element.title=(result.description);}else if(isArrayEntry){rootElement.appendChild(this._formatAsArrayEntry(result));}else{const maxLength=100;var type=result.type;var subtype=result.subtype;var description='';if(type!=='function'&&result.description){if(type==='string'||subtype==='regexp')
description=result.description.trimMiddle(maxLength);else
description=result.description.trimEnd(maxLength);}
rootElement.appendChild(this._previewFormatter.renderPropertyPreview(type,subtype,description));}}
return rootElement;}
_formatWithSubstitutionString(format,parameters,formattedResult){var formatters={};function parameterFormatter(force,includePreview,obj){return this._formatParameter(obj,force,includePreview);}
function stringFormatter(obj){return obj.description;}
function floatFormatter(obj){if(typeof obj.value!=='number')
return'NaN';return obj.value;}
function integerFormatter(obj){if(typeof obj.value!=='number')
return'NaN';return Math.floor(obj.value);}
function bypassFormatter(obj){return(obj instanceof Node)?obj:'';}
var currentStyle=null;function styleFormatter(obj){currentStyle={};var buffer=createElement('span');buffer.setAttribute('style',obj.description);for(var i=0;i<buffer.style.length;i++){var property=buffer.style[i];if(isWhitelistedProperty(property))
currentStyle[property]=buffer.style[property];}}
function isWhitelistedProperty(property){var prefixes=['background','border','color','font','line','margin','padding','text','-webkit-background','-webkit-border','-webkit-font','-webkit-margin','-webkit-padding','-webkit-text'];for(var i=0;i<prefixes.length;i++){if(property.startsWith(prefixes[i]))
return true;}
return false;}
formatters.o=parameterFormatter.bind(this,false,true);formatters.s=stringFormatter;formatters.f=floatFormatter;formatters.i=integerFormatter;formatters.d=integerFormatter;formatters.c=styleFormatter;formatters.O=parameterFormatter.bind(this,true,false);formatters._=bypassFormatter;function append(a,b){if(b instanceof Node){a.appendChild(b);}else if(typeof b!=='undefined'){var toAppend=Console.ConsoleViewMessage._linkifyStringAsFragment(String(b));if(currentStyle){var wrapper=createElement('span');wrapper.appendChild(toAppend);applyCurrentStyle(wrapper);for(var i=0;i<wrapper.children.length;++i)
applyCurrentStyle(wrapper.children[i]);toAppend=wrapper;}
a.appendChild(toAppend);}
return a;}
function applyCurrentStyle(element){for(var key in currentStyle)
element.style[key]=currentStyle[key];}
return String.format(format,parameters,formatters,formattedResult,append);}
matchesFilterRegex(regexObject){regexObject.lastIndex=0;var text=this.contentElement().deepTextContent();return regexObject.test(text);}
matchesFilterText(filter){var text=this.contentElement().deepTextContent();return text.toLowerCase().includes(filter.toLowerCase());}
updateTimestamp(){if(!this._contentElement)
return;if(Common.moduleSetting('consoleTimestampsEnabled').get()){if(!this._timestampElement)
this._timestampElement=createElementWithClass('span','console-timestamp');this._timestampElement.textContent=formatTimestamp(this._message.timestamp,false)+' ';this._timestampElement.title=formatTimestamp(this._message.timestamp,true);this._contentElement.insertBefore(this._timestampElement,this._contentElement.firstChild);}else if(this._timestampElement){this._timestampElement.remove();delete this._timestampElement;}
function formatTimestamp(timestamp,full){var date=new Date(timestamp);var yymmdd=date.getFullYear()+'-'+leadZero(date.getMonth()+1,2)+'-'+leadZero(date.getDate(),2);var hhmmssfff=leadZero(date.getHours(),2)+':'+leadZero(date.getMinutes(),2)+':'+
leadZero(date.getSeconds(),2)+'.'+leadZero(date.getMilliseconds(),3);return full?(yymmdd+' '+hhmmssfff):hhmmssfff;function leadZero(value,length){var valueString=value.toString();var padding=length-valueString.length;return padding<=0?valueString:'0'.repeat(padding)+valueString;}}}
nestingLevel(){return this._nestingLevel;}
setInSimilarGroup(inSimilarGroup,isLast){this._inSimilarGroup=inSimilarGroup;this._lastInSimilarGroup=inSimilarGroup&&!!isLast;if(this._similarGroupMarker&&!inSimilarGroup){this._similarGroupMarker.remove();this._similarGroupMarker=null;}else if(this._element&&!this._similarGroupMarker&&inSimilarGroup){this._similarGroupMarker=createElementWithClass('div','nesting-level-marker');this._element.insertBefore(this._similarGroupMarker,this._element.firstChild);this._similarGroupMarker.classList.toggle('group-closed',this._lastInSimilarGroup);}}
isLastInSimilarGroup(){return this._inSimilarGroup&&this._lastInSimilarGroup;}
resetCloseGroupDecorationCount(){if(!this._closeGroupDecorationCount)
return;this._closeGroupDecorationCount=0;this._updateCloseGroupDecorations();}
incrementCloseGroupDecorationCount(){++this._closeGroupDecorationCount;this._updateCloseGroupDecorations();}
_updateCloseGroupDecorations(){if(!this._nestingLevelMarkers)
return;for(var i=0,n=this._nestingLevelMarkers.length;i<n;++i){var marker=this._nestingLevelMarkers[i];marker.classList.toggle('group-closed',n-i<=this._closeGroupDecorationCount);}}
contentElement(){if(this._contentElement)
return this._contentElement;var contentElement=createElementWithClass('div','console-message');if(this._messageLevelIcon)
contentElement.appendChild(this._messageLevelIcon);this._contentElement=contentElement;var formattedMessage;var shouldIncludeTrace=!!this._message.stackTrace&&(this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Network||this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Violation||this._message.level===ConsoleModel.ConsoleMessage.MessageLevel.Error||this._message.level===ConsoleModel.ConsoleMessage.MessageLevel.Warning||this._message.type===ConsoleModel.ConsoleMessage.MessageType.Trace);if(this._message.runtimeModel()&&shouldIncludeTrace)
formattedMessage=this._buildMessageWithStackTrace();else if(this._message.type===ConsoleModel.ConsoleMessage.MessageType.Table)
formattedMessage=this._buildTableMessage();else
formattedMessage=this._buildMessage();contentElement.appendChild(formattedMessage);this.updateTimestamp();return this._contentElement;}
toMessageElement(){if(this._element)
return this._element;this._element=createElement('div');this.updateMessageElement();return this._element;}
updateMessageElement(){if(!this._element)
return;this._element.className='console-message-wrapper';this._element.removeChildren();if(this._message.isGroupStartMessage())
this._element.classList.add('console-group-title');if(this._message.source===ConsoleModel.ConsoleMessage.MessageSource.ConsoleAPI)
this._element.classList.add('console-from-api');if(this._inSimilarGroup){this._similarGroupMarker=this._element.createChild('div','nesting-level-marker');this._similarGroupMarker.classList.toggle('group-closed',this._lastInSimilarGroup);}
this._nestingLevelMarkers=[];for(var i=0;i<this._nestingLevel;++i)
this._nestingLevelMarkers.push(this._element.createChild('div','nesting-level-marker'));this._updateCloseGroupDecorations();this._element.message=this;switch(this._message.level){case ConsoleModel.ConsoleMessage.MessageLevel.Verbose:this._element.classList.add('console-verbose-level');this._updateMessageLevelIcon('');break;case ConsoleModel.ConsoleMessage.MessageLevel.Info:this._element.classList.add('console-info-level');break;case ConsoleModel.ConsoleMessage.MessageLevel.Warning:this._element.classList.add('console-warning-level');this._updateMessageLevelIcon('smallicon-warning');break;case ConsoleModel.ConsoleMessage.MessageLevel.Error:this._element.classList.add('console-error-level');this._updateMessageLevelIcon('smallicon-error');break;}
if(this._shouldRenderAsWarning())
this._element.classList.add('console-warning-level');this._element.appendChild(this.contentElement());if(this._repeatCount>1)
this._showRepeatCountElement();}
_shouldRenderAsWarning(){return(this._message.level===ConsoleModel.ConsoleMessage.MessageLevel.Verbose||this._message.level===ConsoleModel.ConsoleMessage.MessageLevel.Info)&&(this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Violation||this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Deprecation||this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Intervention||this._message.source===ConsoleModel.ConsoleMessage.MessageSource.Recommendation);}
_updateMessageLevelIcon(iconType){if(!iconType&&!this._messageLevelIcon)
return;if(iconType&&!this._messageLevelIcon){this._messageLevelIcon=UI.Icon.create('','message-level-icon');if(this._contentElement)
this._contentElement.insertBefore(this._messageLevelIcon,this._contentElement.firstChild);}
this._messageLevelIcon.setIconType(iconType);}
repeatCount(){return this._repeatCount||1;}
resetIncrementRepeatCount(){this._repeatCount=1;if(!this._repeatCountElement)
return;this._repeatCountElement.remove();if(this._contentElement)
this._contentElement.classList.remove('repeated-message');delete this._repeatCountElement;}
incrementRepeatCount(){this._repeatCount++;this._showRepeatCountElement();}
setRepeatCount(repeatCount){this._repeatCount=repeatCount;this._showRepeatCountElement();}
_showRepeatCountElement(){if(!this._element)
return;if(!this._repeatCountElement){this._repeatCountElement=createElementWithClass('label','console-message-repeat-count','dt-small-bubble');switch(this._message.level){case ConsoleModel.ConsoleMessage.MessageLevel.Warning:this._repeatCountElement.type='warning';break;case ConsoleModel.ConsoleMessage.MessageLevel.Error:this._repeatCountElement.type='error';break;case ConsoleModel.ConsoleMessage.MessageLevel.Verbose:this._repeatCountElement.type='verbose';break;default:this._repeatCountElement.type='info';}
if(this._shouldRenderAsWarning())
this._repeatCountElement.type='warning';this._element.insertBefore(this._repeatCountElement,this._contentElement);this._contentElement.classList.add('repeated-message');}
this._repeatCountElement.textContent=this._repeatCount;}
get text(){return this._message.messageText;}
toExportString(){var lines=[];var nodes=this.contentElement().childTextNodes();var messageContent=nodes.map(Components.Linkifier.untruncatedNodeText).join('');for(var i=0;i<this.repeatCount();++i)
lines.push(messageContent);return lines.join('\n');}
setSearchRegex(regex){if(this._searchHiglightNodeChanges&&this._searchHiglightNodeChanges.length)
UI.revertDomChanges(this._searchHiglightNodeChanges);this._searchRegex=regex;this._searchHighlightNodes=[];this._searchHiglightNodeChanges=[];if(!this._searchRegex)
return;var text=this.contentElement().deepTextContent();var match;this._searchRegex.lastIndex=0;var sourceRanges=[];while((match=this._searchRegex.exec(text))&&match[0])
sourceRanges.push(new TextUtils.SourceRange(match.index,match[0].length));if(sourceRanges.length){this._searchHighlightNodes=UI.highlightSearchResults(this.contentElement(),sourceRanges,this._searchHiglightNodeChanges);}}
searchRegex(){return this._searchRegex;}
searchCount(){return this._searchHighlightNodes.length;}
searchHighlightNode(index){return this._searchHighlightNodes[index];}
_tryFormatAsError(string){function startsWith(prefix){return string.startsWith(prefix);}
var errorPrefixes=['EvalError','ReferenceError','SyntaxError','TypeError','RangeError','Error','URIError'];if(!this._message.runtimeModel()||!errorPrefixes.some(startsWith))
return null;var debuggerModel=this._message.runtimeModel().debuggerModel();var lines=string.split('\n');var links=[];var position=0;for(var i=0;i<lines.length;++i){position+=i>0?lines[i-1].length+1:0;var isCallFrameLine=/^\s*at\s/.test(lines[i]);if(!isCallFrameLine&&links.length)
return null;if(!isCallFrameLine)
continue;var openBracketIndex=-1;var closeBracketIndex=-1;var match=/\([^\)\(]+\)/.exec(lines[i]);if(match){openBracketIndex=match.index;closeBracketIndex=match.index+match[0].length-1;}
var hasOpenBracket=openBracketIndex!==-1;var left=hasOpenBracket?openBracketIndex+1:lines[i].indexOf('at')+3;var right=hasOpenBracket?closeBracketIndex:lines[i].length;var linkCandidate=lines[i].substring(left,right);var splitResult=Common.ParsedURL.splitLineAndColumn(linkCandidate);if(!splitResult)
return null;var parsed=splitResult.url.asParsedURL();var url;if(parsed)
url=parsed.url;else if(debuggerModel.scriptsForSourceURL(splitResult.url).length)
url=splitResult.url;else if(splitResult.url==='<anonymous>')
continue;else
return null;links.push({url:url,positionLeft:position+left,positionRight:position+right,lineNumber:splitResult.lineNumber,columnNumber:splitResult.columnNumber});}
if(!links.length)
return null;var formattedResult=createElement('span');var start=0;for(var i=0;i<links.length;++i){formattedResult.appendChild(Console.ConsoleViewMessage._linkifyStringAsFragment(string.substring(start,links[i].positionLeft)));formattedResult.appendChild(this._linkifier.linkifyScriptLocation(debuggerModel.target(),null,links[i].url,links[i].lineNumber,links[i].columnNumber));start=links[i].positionRight;}
if(start!==string.length)
formattedResult.appendChild(Console.ConsoleViewMessage._linkifyStringAsFragment(string.substring(start)));return formattedResult;}
static _linkifyWithCustomLinkifier(string,linkifier){var container=createDocumentFragment();var tokens=this._tokenizeMessageText(string);for(var token of tokens){switch(token.type){case'url':{var realURL=(token.text.startsWith('www.')?'http://'+token.text:token.text);var splitResult=Common.ParsedURL.splitLineAndColumn(realURL);var linkNode;if(splitResult)
linkNode=linkifier(token.text,splitResult.url,splitResult.lineNumber,splitResult.columnNumber);else
linkNode=linkifier(token.text,token.value);container.appendChild(linkNode);break;}
default:container.appendChild(createTextNode(token.text));break;}}
return container;}
static _linkifyStringAsFragment(string){return Console.ConsoleViewMessage._linkifyWithCustomLinkifier(string,(text,url,lineNumber,columnNumber)=>{return Components.Linkifier.linkifyURL(url,{text,lineNumber,columnNumber});});}
static _tokenizeMessageText(string){if(!Console.ConsoleViewMessage._tokenizerRegexes){var linkStringRegex=/(?:[a-zA-Z][a-zA-Z0-9+.-]{2,}:\/\/|data:|www\.)[\w$\-_+*'=\|\/\\(){}[\]^%@&#~,:;.!?]{2,}[\w$\-_+*=\|\/\\({^%@&#~]/;var pathLineRegex=/(?:\/[\w\.-]*)+\:[\d]+/;var timeRegex=/took [\d]+ms/;var eventRegex=/'\w+' event/;var milestoneRegex=/\sM[6-7]\d/;var autofillRegex=/\(suggested: \"[\w-]+\"\)/;var handlers=new Map();handlers.set(linkStringRegex,'url');handlers.set(pathLineRegex,'url');handlers.set(timeRegex,'time');handlers.set(eventRegex,'event');handlers.set(milestoneRegex,'milestone');handlers.set(autofillRegex,'autofill');Console.ConsoleViewMessage._tokenizerRegexes=Array.from(handlers.keys());Console.ConsoleViewMessage._tokenizerTypes=Array.from(handlers.values());}
var results=TextUtils.TextUtils.splitStringByRegexes(string,Console.ConsoleViewMessage._tokenizerRegexes);return results.map(result=>({text:result.value,type:Console.ConsoleViewMessage._tokenizerTypes[result.regexIndex]}));}
groupKey(){if(!this._groupKey)
this._groupKey=this._message.groupCategoryKey()+':'+this.groupTitle();return this._groupKey;}
groupTitle(){var tokens=Console.ConsoleViewMessage._tokenizeMessageText(this._message.messageText);var result=tokens.reduce((acc,token)=>{var text=token.text;if(token.type==='url')
text=Common.UIString('<URL>');else if(token.type==='time')
text=Common.UIString('took <N>ms');else if(token.type==='event')
text=Common.UIString('<some> event');else if(token.type==='milestone')
text=Common.UIString('M<XX>');else if(token.type==='autofill')
text=Common.UIString('<attribute>');return acc+text;},'');return result.replace(/[%]o/g,'');}};Console.ConsoleGroupViewMessage=class extends Console.ConsoleViewMessage{constructor(consoleMessage,linkifier,badgePool,nestingLevel){console.assert(consoleMessage.isGroupStartMessage());super(consoleMessage,linkifier,badgePool,nestingLevel);this._collapsed=consoleMessage.type===ConsoleModel.ConsoleMessage.MessageType.StartGroupCollapsed;this._expandGroupIcon=null;}
setCollapsed(collapsed){this._collapsed=collapsed;if(this._expandGroupIcon)
this._expandGroupIcon.setIconType(this._collapsed?'smallicon-triangle-right':'smallicon-triangle-down');}
collapsed(){return this._collapsed;}
toMessageElement(){if(!this._element){super.toMessageElement();this._expandGroupIcon=UI.Icon.create('','expand-group-icon');if(this._repeatCountElement)
this._repeatCountElement.insertBefore(this._expandGroupIcon,this._repeatCountElement.firstChild);else
this._element.insertBefore(this._expandGroupIcon,this._contentElement);this.setCollapsed(this._collapsed);}
return this._element;}
_showRepeatCountElement(){super._showRepeatCountElement();if(this._repeatCountElement&&this._expandGroupIcon)
this._repeatCountElement.insertBefore(this._expandGroupIcon,this._repeatCountElement.firstChild);}};Console.ConsoleViewMessage.MaxLengthForLinks=40;;Console.ConsolePrompt=class extends UI.Widget{constructor(){super();this._addCompletionsFromHistory=true;this._history=new Console.ConsoleHistoryManager();this._initialText='';this._editor=null;this.element.tabIndex=0;self.runtime.extension(UI.TextEditorFactory).instance().then(gotFactory.bind(this));function gotFactory(factory){this._editor=factory.createEditor({lineNumbers:false,lineWrapping:true,mimeType:'javascript',autoHeight:true});this._editor.configureAutocomplete({substituteRangeCallback:this._substituteRange.bind(this),suggestionsCallback:this._wordsWithQuery.bind(this),captureEnter:true});this._editor.widget().element.addEventListener('keydown',this._editorKeyDown.bind(this),true);this._editor.widget().show(this.element);this._editor.addEventListener(UI.TextEditor.Events.TextChanged,this._onTextChanged,this);this.setText(this._initialText);delete this._initialText;if(this.hasFocus())
this.focus();this.element.tabIndex=-1;this._editorSetForTest();}}
_onTextChanged(){this.dispatchEventToListeners(Console.ConsolePrompt.Events.TextChanged);}
history(){return this._history;}
clearAutocomplete(){if(this._editor)
this._editor.clearAutocomplete();}
_isCaretAtEndOfPrompt(){return!!this._editor&&this._editor.selection().collapseToEnd().equal(this._editor.fullRange().collapseToEnd());}
moveCaretToEndOfPrompt(){if(this._editor)
this._editor.setSelection(TextUtils.TextRange.createFromLocation(Infinity,Infinity));}
setText(text){if(this._editor)
this._editor.setText(text);else
this._initialText=text;this.dispatchEventToListeners(Console.ConsolePrompt.Events.TextChanged);}
text(){return this._editor?this._editor.text():this._initialText;}
setAddCompletionsFromHistory(value){this._addCompletionsFromHistory=value;}
_editorKeyDown(event){var keyboardEvent=(event);var newText;var isPrevious;switch(keyboardEvent.keyCode){case UI.KeyboardShortcut.Keys.Up.code:if(keyboardEvent.shiftKey||this._editor.selection().endLine>0)
break;newText=this._history.previous(this.text());isPrevious=true;break;case UI.KeyboardShortcut.Keys.Down.code:if(keyboardEvent.shiftKey||this._editor.selection().endLine<this._editor.fullRange().endLine)
break;newText=this._history.next();break;case UI.KeyboardShortcut.Keys.P.code:if(Host.isMac()&&keyboardEvent.ctrlKey&&!keyboardEvent.metaKey&&!keyboardEvent.altKey&&!keyboardEvent.shiftKey){newText=this._history.previous(this.text());isPrevious=true;}
break;case UI.KeyboardShortcut.Keys.N.code:if(Host.isMac()&&keyboardEvent.ctrlKey&&!keyboardEvent.metaKey&&!keyboardEvent.altKey&&!keyboardEvent.shiftKey)
newText=this._history.next();break;case UI.KeyboardShortcut.Keys.Enter.code:this._enterKeyPressed(keyboardEvent);break;case UI.KeyboardShortcut.Keys.Tab.code:if(!this.text())
keyboardEvent.consume();break;}
if(newText===undefined)
return;keyboardEvent.consume(true);this.setText(newText);if(isPrevious)
this._editor.setSelection(TextUtils.TextRange.createFromLocation(0,Infinity));else
this.moveCaretToEndOfPrompt();}
async _enterKeyPressed(event){if(event.altKey||event.ctrlKey||event.shiftKey)
return;event.consume(true);this.clearAutocomplete();var str=this.text();if(!str.length)
return;var currentExecutionContext=UI.context.flavor(SDK.ExecutionContext);if(!this._isCaretAtEndOfPrompt()||!currentExecutionContext){this._appendCommand(str,true);return;}
var result=await currentExecutionContext.runtimeModel.compileScript(str,'',false,currentExecutionContext.id);if(str!==this.text())
return;var exceptionDetails=result && result.exceptionDetails;if(exceptionDetails&&(exceptionDetails.exception.description.startsWith('SyntaxError: Unexpected end of input')||exceptionDetails.exception.description.startsWith('SyntaxError: Unterminated template literal'))){this._editor.newlineAndIndent();this._enterProcessedForTest();return;}
await this._appendCommand(str,true);this._enterProcessedForTest();}
async _appendCommand(text,useCommandLineAPI){this.setText('');var currentExecutionContext=UI.context.flavor(SDK.ExecutionContext);if(currentExecutionContext){var executionContext=currentExecutionContext;var message=ConsoleModel.consoleModel.addCommandMessage(executionContext,text);text=SDK.RuntimeModel.wrapObjectLiteralExpressionIfNeeded(text);var preprocessed=false;if(text.indexOf('await')!==-1){var preprocessedText=await Formatter.formatterWorkerPool().preprocessTopLevelAwaitExpressions(text);preprocessed=!!preprocessedText;text=preprocessedText||text;}
ConsoleModel.consoleModel.evaluateCommandInConsole(executionContext,message,text,useCommandLineAPI,preprocessed);if(Console.ConsolePanel.instance().isShowing())
Host.userMetrics.actionTaken(Host.UserMetrics.Action.CommandEvaluatedInConsolePanel);}}
_enterProcessedForTest(){}
_historyCompletions(prefix,force){if(!this._addCompletionsFromHistory||!this._isCaretAtEndOfPrompt()||(!prefix&&!force))
return[];var result=[];var text=this.text();var set=new Set();var data=this._history.historyData();for(var i=data.length-1;i>=0&&result.length<50;--i){var item=data[i];if(!item.startsWith(text))
continue;if(set.has(item))
continue;set.add(item);result.push({text:item.substring(text.length-prefix.length),iconType:'smallicon-text-prompt',isSecondary:true});}
return result;}
focus(){if(this._editor)
this._editor.widget().focus();else
this.element.focus();}
_substituteRange(lineNumber,columnNumber){var token=this._editor.tokenAtTextPosition(lineNumber,columnNumber);if(token&&token.type==='js-string')
return new TextUtils.TextRange(lineNumber,token.startColumn,lineNumber,columnNumber);var lineText=this._editor.line(lineNumber);var index;for(index=columnNumber-1;index>=0;index--){if(' =:[({;,!+-*/&|^<>.\t\r\n'.indexOf(lineText.charAt(index))!==-1)
break;}
return new TextUtils.TextRange(lineNumber,index+1,lineNumber,columnNumber);}
_wordsWithQuery(queryRange,substituteRange,force){var query=this._editor.text(queryRange);var before=this._editor.text(new TextUtils.TextRange(0,0,queryRange.startLine,queryRange.startColumn));var historyWords=this._historyCompletions(query,force);var token=this._editor.tokenAtTextPosition(substituteRange.startLine,substituteRange.startColumn);if(token){var excludedTokens=new Set(['js-comment','js-string-2','js-def']);var trimmedBefore=before.trim();if(!trimmedBefore.endsWith('[')&&!trimmedBefore.match(/\.\s*(get|set|delete)\s*\(\s*$/))
excludedTokens.add('js-string');if(!trimmedBefore.endsWith('.'))
excludedTokens.add('js-property');if(excludedTokens.has(token.type))
return Promise.resolve(historyWords);}
return ObjectUI.JavaScriptAutocomplete.completionsForTextInCurrentContext(before,query,force).then(words=>words.concat(historyWords));}
_editorSetForTest(){}};Console.ConsoleHistoryManager=class{constructor(){this._data=[];this._historyOffset=1;}
historyData(){return this._data;}
setHistoryData(data){this._data=data.slice();this._historyOffset=1;}
pushHistoryItem(text){if(this._uncommittedIsTop){this._data.pop();delete this._uncommittedIsTop;}
this._historyOffset=1;if(text===this._currentHistoryItem())
return;this._data.push(text);}
_pushCurrentText(currentText){if(this._uncommittedIsTop)
this._data.pop();this._uncommittedIsTop=true;this._data.push(currentText);}
previous(currentText){if(this._historyOffset>this._data.length)
return undefined;if(this._historyOffset===1)
this._pushCurrentText(currentText);++this._historyOffset;return this._currentHistoryItem();}
next(){if(this._historyOffset===1)
return undefined;--this._historyOffset;return this._currentHistoryItem();}
_currentHistoryItem(){return this._data[this._data.length-this._historyOffset];}};Console.ConsolePrompt.Events={TextChanged:Symbol('TextChanged')};;Console.ConsoleView=class extends UI.VBox{constructor(){super();this.setMinimumSize(0,35);this.registerRequiredCSS('console/consoleView.css');this._searchableView=new UI.SearchableView(this);this._searchableView.setPlaceholder(Common.UIString('Find string in logs'));this._searchableView.setMinimalSearchQuerySize(0);this._badgePool=new ProductRegistry.BadgePool();this._sidebar=new Console.ConsoleSidebar(this._badgePool);this._sidebar.addEventListener(Console.ConsoleSidebar.Events.FilterSelected,this._updateMessageList.bind(this));this._isSidebarOpen=false;var toolbar=new UI.Toolbar('',this.element);var isLogManagementEnabled=Runtime.experiments.isEnabled('logManagement');if(isLogManagementEnabled){this._splitWidget=new UI.SplitWidget(true,false,'console.sidebar.width',100);this._splitWidget.setMainWidget(this._searchableView);this._splitWidget.setSidebarWidget(this._sidebar);this._splitWidget.hideSidebar();this._splitWidget.show(this.element);toolbar.appendToolbarItem(this._splitWidget.createShowHideSidebarButton('console sidebar'));this._splitWidget.addEventListener(UI.SplitWidget.Events.ShowModeChanged,event=>{this._isSidebarOpen=event.data===UI.SplitWidget.ShowMode.Both;this._filter._levelMenuButton.setEnabled(!this._isSidebarOpen);this._onFilterChanged();});}else{this._searchableView.show(this.element);}
this._contentsElement=this._searchableView.element;this.element.classList.add('console-view');this._visibleViewMessages=[];this._urlToMessageCount={};this._hiddenByFilterCount=0;this._groupableMessages=new Map();this._groupableMessageTitle=new Map();this._regexMatchRanges=[];this._filter=new Console.ConsoleViewFilter(this._onFilterChanged.bind(this));this._consoleContextSelector=new Console.ConsoleContextSelector();this._filterStatusText=new UI.ToolbarText();this._filterStatusText.element.classList.add('dimmed');this._showSettingsPaneSetting=Common.settings.createSetting('consoleShowSettingsToolbar',false);this._showSettingsPaneButton=new UI.ToolbarSettingToggle(this._showSettingsPaneSetting,'largeicon-settings-gear',Common.UIString('Console settings'));this._progressToolbarItem=new UI.ToolbarItem(createElement('div'));this._groupSimilarSetting=Common.settings.moduleSetting('consoleGroupSimilar');this._groupSimilarSetting.addChangeListener(()=>this._updateMessageList());var groupSimilarToggle=new UI.ToolbarSettingCheckbox(this._groupSimilarSetting,Common.UIString('Group similar'));toolbar.appendToolbarItem(UI.Toolbar.createActionButton((UI.actionRegistry.action('console.clear'))));toolbar.appendSeparator();toolbar.appendToolbarItem(this._consoleContextSelector.toolbarItem());toolbar.appendSeparator();toolbar.appendToolbarItem(this._filter._textFilterUI);toolbar.appendToolbarItem(this._filter._levelMenuButton);toolbar.appendToolbarItem(groupSimilarToggle);toolbar.appendToolbarItem(this._progressToolbarItem);toolbar.appendSpacer();toolbar.appendToolbarItem(this._filterStatusText);toolbar.appendSeparator();toolbar.appendToolbarItem(this._showSettingsPaneButton);this._preserveLogCheckbox=new UI.ToolbarSettingCheckbox(Common.moduleSetting('preserveConsoleLog'),Common.UIString('Do not clear log on page reload / navigation'),Common.UIString('Preserve log'));this._hideNetworkMessagesCheckbox=new UI.ToolbarSettingCheckbox(this._filter._hideNetworkMessagesSetting,this._filter._hideNetworkMessagesSetting.title(),Common.UIString('Hide network'));var filterByExecutionContextCheckbox=new UI.ToolbarSettingCheckbox(this._filter._filterByExecutionContextSetting,Common.UIString('Only show messages from the current context (top, iframe, worker, extension)'),Common.UIString('Selected context only'));var filterConsoleAPICheckbox=new UI.ToolbarSettingCheckbox(Common.moduleSetting('consoleAPIFilterEnabled'),Common.UIString('Only show messages from console API methods'),Common.UIString('User messages only'));var monitoringXHREnabledSetting=Common.moduleSetting('monitoringXHREnabled');this._timestampsSetting=Common.moduleSetting('consoleTimestampsEnabled');this._consoleHistoryAutocompleteSetting=Common.moduleSetting('consoleHistoryAutocomplete');var settingsPane=new UI.HBox();settingsPane.show(this._contentsElement);settingsPane.element.classList.add('console-settings-pane');var settingsToolbarLeft=new UI.Toolbar('',settingsPane.element);settingsToolbarLeft.makeVertical();settingsToolbarLeft.appendToolbarItem(this._hideNetworkMessagesCheckbox);settingsToolbarLeft.appendToolbarItem(this._preserveLogCheckbox);settingsToolbarLeft.appendToolbarItem(filterByExecutionContextCheckbox);if(!isLogManagementEnabled)
settingsToolbarLeft.appendToolbarItem(filterConsoleAPICheckbox);var settingsToolbarRight=new UI.Toolbar('',settingsPane.element);settingsToolbarRight.makeVertical();settingsToolbarRight.appendToolbarItem(new UI.ToolbarSettingCheckbox(monitoringXHREnabledSetting));settingsToolbarRight.appendToolbarItem(new UI.ToolbarSettingCheckbox(this._timestampsSetting));settingsToolbarRight.appendToolbarItem(new UI.ToolbarSettingCheckbox(this._consoleHistoryAutocompleteSetting));if(!this._showSettingsPaneSetting.get())
settingsPane.element.classList.add('hidden');this._showSettingsPaneSetting.addChangeListener(()=>settingsPane.element.classList.toggle('hidden',!this._showSettingsPaneSetting.get()));this._viewport=new Console.ConsoleViewport(this);this._viewport.setStickToBottom(true);this._viewport.contentElement().classList.add('console-group','console-group-messages');this._contentsElement.appendChild(this._viewport.element);this._messagesElement=this._viewport.element;this._messagesElement.id='console-messages';this._messagesElement.classList.add('monospace');this._messagesElement.addEventListener('click',this._messagesClicked.bind(this),true);this._viewportThrottler=new Common.Throttler(50);this._topGroup=Console.ConsoleGroup.createTopGroup();this._currentGroup=this._topGroup;this._promptElement=this._messagesElement.createChild('div','source-code');var promptIcon=UI.Icon.create('smallicon-text-prompt','console-prompt-icon');this._promptElement.appendChild(promptIcon);this._promptElement.id='console-prompt';var selectAllFixer=this._messagesElement.createChild('div','console-view-fix-select-all');selectAllFixer.textContent='.';this._registerShortcuts();this._messagesElement.addEventListener('contextmenu',this._handleContextMenuEvent.bind(this),false);this._linkifier=new Components.Linkifier(Console.ConsoleViewMessage.MaxLengthForLinks);this._consoleMessages=[];this._viewMessageSymbol=Symbol('viewMessage');this._consoleHistorySetting=Common.settings.createLocalSetting('consoleHistory',[]);this._prompt=new Console.ConsolePrompt();this._prompt.show(this._promptElement);this._prompt.element.addEventListener('keydown',this._promptKeyDown.bind(this),true);this._prompt.addEventListener(Console.ConsolePrompt.Events.TextChanged,this._promptTextChanged,this);this._consoleHistoryAutocompleteSetting.addChangeListener(this._consoleHistoryAutocompleteChanged,this);var historyData=this._consoleHistorySetting.get();this._prompt.history().setHistoryData(historyData);this._consoleHistoryAutocompleteChanged();this._updateFilterStatus();this._timestampsSetting.addChangeListener(this._consoleTimestampsSettingChanged,this);this._registerWithMessageSink();UI.context.addFlavorChangeListener(SDK.ExecutionContext,this._executionContextChanged,this);this._messagesElement.addEventListener('mousedown',this._updateStickToBottomOnMouseDown.bind(this),false);this._messagesElement.addEventListener('mouseup',this._updateStickToBottomOnMouseUp.bind(this),false);this._messagesElement.addEventListener('mouseleave',this._updateStickToBottomOnMouseUp.bind(this),false);this._messagesElement.addEventListener('wheel',this._updateStickToBottomOnWheel.bind(this),false);ConsoleModel.consoleModel.addEventListener(ConsoleModel.ConsoleModel.Events.ConsoleCleared,this._consoleCleared,this);ConsoleModel.consoleModel.addEventListener(ConsoleModel.ConsoleModel.Events.MessageAdded,this._onConsoleMessageAdded,this);ConsoleModel.consoleModel.addEventListener(ConsoleModel.ConsoleModel.Events.MessageUpdated,this._onConsoleMessageUpdated,this);ConsoleModel.consoleModel.addEventListener(ConsoleModel.ConsoleModel.Events.CommandEvaluated,this._commandEvaluated,this);ConsoleModel.consoleModel.messages().forEach(this._addConsoleMessage,this);}
static instance(){if(!Console.ConsoleView._instance)
Console.ConsoleView._instance=new Console.ConsoleView();return Console.ConsoleView._instance;}
static clearConsole(){ConsoleModel.consoleModel.requestClearMessages();}
_onFilterChanged(){this._filter._currentFilter.levelsMask=this._isSidebarOpen?Console.ConsoleFilter.allLevelsFilterValue():this._filter._messageLevelFiltersSetting.get();this._updateMessageList();}
searchableView(){return this._searchableView;}
_clearHistory(){this._consoleHistorySetting.set([]);this._prompt.history().setHistoryData([]);}
_consoleHistoryAutocompleteChanged(){this._prompt.setAddCompletionsFromHistory(this._consoleHistoryAutocompleteSetting.get());}
itemCount(){return this._visibleViewMessages.length;}
itemElement(index){return this._visibleViewMessages[index];}
fastHeight(index){return this._visibleViewMessages[index].fastHeight();}
minimumRowHeight(){return 16;}
_registerWithMessageSink(){Common.console.messages().forEach(this._addSinkMessage,this);Common.console.addEventListener(Common.Console.Events.MessageAdded,messageAdded,this);function messageAdded(event){this._addSinkMessage((event.data));}}
_addSinkMessage(message){var level=ConsoleModel.ConsoleMessage.MessageLevel.Verbose;switch(message.level){case Common.Console.MessageLevel.Info:level=ConsoleModel.ConsoleMessage.MessageLevel.Info;break;case Common.Console.MessageLevel.Error:level=ConsoleModel.ConsoleMessage.MessageLevel.Error;break;case Common.Console.MessageLevel.Warning:level=ConsoleModel.ConsoleMessage.MessageLevel.Warning;break;}
var consoleMessage=new ConsoleModel.ConsoleMessage(null,ConsoleModel.ConsoleMessage.MessageSource.Other,level,message.text,ConsoleModel.ConsoleMessage.MessageType.System,undefined,undefined,undefined,undefined,undefined,undefined,message.timestamp);this._addConsoleMessage(consoleMessage);}
_consoleTimestampsSettingChanged(){this._updateMessageList();this._consoleMessages.forEach(viewMessage=>viewMessage.updateTimestamp());this._groupableMessageTitle.forEach(viewMessage=>viewMessage.updateTimestamp());}
_executionContextChanged(){this._prompt.clearAutocomplete();}
willHide(){this._hidePromptSuggestBox();}
wasShown(){this._viewport.refresh();}
focus(){if(!this._prompt.hasFocus())
this._prompt.focus();}
restoreScrollPositions(){if(this._viewport.stickToBottom())
this._immediatelyScrollToBottom();else
super.restoreScrollPositions();}
onResize(){this._scheduleViewportRefresh();this._hidePromptSuggestBox();if(this._viewport.stickToBottom())
this._immediatelyScrollToBottom();for(var i=0;i<this._visibleViewMessages.length;++i)
this._visibleViewMessages[i].onResize();}
_hidePromptSuggestBox(){this._prompt.clearAutocomplete();}
_invalidateViewport(){if(this._muteViewportUpdates){this._maybeDirtyWhileMuted=true;return Promise.resolve();}
if(this._needsFullUpdate){this._updateMessageList();delete this._needsFullUpdate;}else{this._viewport.invalidate();}
return Promise.resolve();}
_scheduleViewportRefresh(){if(this._muteViewportUpdates){this._maybeDirtyWhileMuted=true;this._scheduleViewportRefreshForTest(true);return;}else{this._scheduleViewportRefreshForTest(false);}
this._viewportThrottler.schedule(this._invalidateViewport.bind(this));}
_scheduleViewportRefreshForTest(muted){}
_immediatelyScrollToBottom(){this._viewport.setStickToBottom(true);this._promptElement.scrollIntoView(true);}
_updateFilterStatus(){if(this._hiddenByFilterCount===this._lastShownHiddenByFilterCount)
return;this._filterStatusText.setText(Common.UIString(this._hiddenByFilterCount+' hidden'));this._filterStatusText.setVisible(!!this._hiddenByFilterCount);this._lastShownHiddenByFilterCount=this._hiddenByFilterCount;}
_onConsoleMessageAdded(event){var message=(event.data);this._addConsoleMessage(message);}
_addConsoleMessage(message){var viewMessage=this._createViewMessage(message);message[this._viewMessageSymbol]=viewMessage;if(message.type===ConsoleModel.ConsoleMessage.MessageType.Command||message.type===ConsoleModel.ConsoleMessage.MessageType.Result){var lastMessage=this._consoleMessages.peekLast();viewMessage[Console.ConsoleView._messageSortingTimeSymbol]=lastMessage?lastMessage[Console.ConsoleView._messageSortingTimeSymbol]:0;}else{viewMessage[Console.ConsoleView._messageSortingTimeSymbol]=viewMessage.consoleMessage().timestamp;}
var insertAt;if(!this._consoleMessages.length||timeComparator(viewMessage,this._consoleMessages[this._consoleMessages.length-1])>0)
insertAt=this._consoleMessages.length;else
insertAt=this._consoleMessages.upperBound(viewMessage,timeComparator);var insertedInMiddle=insertAt<this._consoleMessages.length;this._consoleMessages.splice(insertAt,0,viewMessage);if(this._urlToMessageCount[message.url])
++this._urlToMessageCount[message.url];else
this._urlToMessageCount[message.url]=1;this._filter.onMessageAdded(message);this._sidebar.onMessageAdded(viewMessage);var shouldGoIntoGroup=false;if(message.isGroupable()){var groupKey=viewMessage.groupKey();shouldGoIntoGroup=this._groupSimilarSetting.get()&&this._groupableMessages.has(groupKey);var list=this._groupableMessages.get(groupKey);if(!list){list=[];this._groupableMessages.set(groupKey,list);}
list.push(viewMessage);}
if(!shouldGoIntoGroup&&!insertedInMiddle){this._appendMessageToEnd(viewMessage);this._updateFilterStatus();this._searchableView.updateSearchMatchesCount(this._regexMatchRanges.length);}else{this._needsFullUpdate=true;}
this._scheduleViewportRefresh();this._consoleMessageAddedForTest(viewMessage);function timeComparator(viewMessage1,viewMessage2){return viewMessage1[Console.ConsoleView._messageSortingTimeSymbol]-
viewMessage2[Console.ConsoleView._messageSortingTimeSymbol];}}
_onConsoleMessageUpdated(event){var message=(event.data);var viewMessage=message[this._viewMessageSymbol];if(viewMessage){viewMessage.updateMessageElement();this._updateMessageList();}}
_consoleMessageAddedForTest(viewMessage){}
_shouldMessageBeVisible(viewMessage){return this._filter.shouldBeVisible(viewMessage)&&(!this._isSidebarOpen||this._sidebar.shouldBeVisible(viewMessage));}
_appendMessageToEnd(viewMessage,preventCollapse){if(!this._shouldMessageBeVisible(viewMessage)){this._hiddenByFilterCount++;return;}
if(!preventCollapse&&this._tryToCollapseMessages(viewMessage,this._visibleViewMessages.peekLast()))
return;var lastMessage=this._visibleViewMessages.peekLast();if(viewMessage.consoleMessage().type===ConsoleModel.ConsoleMessage.MessageType.EndGroup){if(lastMessage&&!this._currentGroup.messagesHidden())
lastMessage.incrementCloseGroupDecorationCount();this._currentGroup=this._currentGroup.parentGroup()||this._currentGroup;return;}
if(!this._currentGroup.messagesHidden()){var originatingMessage=viewMessage.consoleMessage().originatingMessage();if(lastMessage&&originatingMessage&&lastMessage.consoleMessage()===originatingMessage)
lastMessage.toMessageElement().classList.add('console-adjacent-user-command-result');this._visibleViewMessages.push(viewMessage);this._searchMessage(this._visibleViewMessages.length-1);}
if(viewMessage.consoleMessage().isGroupStartMessage())
this._currentGroup=new Console.ConsoleGroup(this._currentGroup,viewMessage);this._messageAppendedForTests();}
_messageAppendedForTests(){}
_createViewMessage(message){var nestingLevel=this._currentGroup.nestingLevel();switch(message.type){case ConsoleModel.ConsoleMessage.MessageType.Command:return new Console.ConsoleCommand(message,this._linkifier,this._badgePool,nestingLevel);case ConsoleModel.ConsoleMessage.MessageType.Result:return new Console.ConsoleCommandResult(message,this._linkifier,this._badgePool,nestingLevel);case ConsoleModel.ConsoleMessage.MessageType.StartGroupCollapsed:case ConsoleModel.ConsoleMessage.MessageType.StartGroup:return new Console.ConsoleGroupViewMessage(message,this._linkifier,this._badgePool,nestingLevel);default:return new Console.ConsoleViewMessage(message,this._linkifier,this._badgePool,nestingLevel);}}
_consoleCleared(){this._currentMatchRangeIndex=-1;this._consoleMessages=[];this._groupableMessages.clear();this._groupableMessageTitle.clear();this._sidebar.clear();this._updateMessageList();this._hidePromptSuggestBox();this._viewport.setStickToBottom(true);this._linkifier.reset();this._badgePool.reset();this._filter.clear();}
_handleContextMenuEvent(event){var contextMenu=new UI.ContextMenu(event);if(event.target.isSelfOrDescendant(this._promptElement)){contextMenu.show();return;}
var sourceElement=event.target.enclosingNodeOrSelfWithClass('console-message-wrapper');var consoleMessage=sourceElement?sourceElement.message.consoleMessage():null;var filterSubMenu=contextMenu.headerSection().appendSubMenuItem(Common.UIString('Filter'));if(consoleMessage&&consoleMessage.url){var menuTitle=Common.UIString('Hide messages from %s',new Common.ParsedURL(consoleMessage.url).displayName);filterSubMenu.headerSection().appendItem(menuTitle,this._filter.addMessageURLFilter.bind(this._filter,consoleMessage.url));}
var unhideAll=filterSubMenu.footerSection().appendItem(Common.UIString('Unhide all'),this._filter.removeMessageURLFilter.bind(this._filter));var hasFilters=false;for(var url in this._filter.messageURLFilters()){filterSubMenu.defaultSection().appendCheckboxItem(String.sprintf('%s (%d)',new Common.ParsedURL(url).displayName,this._urlToMessageCount[url]),this._filter.removeMessageURLFilter.bind(this._filter,url),true);hasFilters=true;}
filterSubMenu.setEnabled(hasFilters||(consoleMessage&&consoleMessage.url));unhideAll.setEnabled(hasFilters);contextMenu.defaultSection().appendAction('console.clear');contextMenu.defaultSection().appendAction('console.clear.history');contextMenu.saveSection().appendItem(Common.UIString('Save as...'),this._saveConsole.bind(this));var request=consoleMessage?consoleMessage.request:null;if(request&&SDK.NetworkManager.canReplayRequest(request)){contextMenu.debugSection().appendItem(Common.UIString('Replay XHR'),SDK.NetworkManager.replayRequest.bind(null,request));}
contextMenu.show();}
async _saveConsole(){var url=SDK.targetManager.mainTarget().inspectedURL();var parsedURL=url.asParsedURL();var filename=String.sprintf('%s-%d.log',parsedURL?parsedURL.host:'console',Date.now());var stream=new Bindings.FileOutputStream();var progressIndicator=new UI.ProgressIndicator();progressIndicator.setTitle(Common.UIString('Writing file…'));progressIndicator.setTotalWork(this.itemCount());var chunkSize=350;if(!await stream.open(filename))
return;this._progressToolbarItem.element.appendChild(progressIndicator.element);var messageIndex=0;while(messageIndex<this.itemCount()&&!progressIndicator.isCanceled()){var messageContents=[];for(var i=0;i<chunkSize&&i+messageIndex<this.itemCount();++i){var message=this.itemElement(messageIndex+i);messageContents.push(message.toExportString());}
messageIndex+=i;await stream.write(messageContents.join('\n')+'\n');progressIndicator.setWorked(messageIndex);}
stream.close();progressIndicator.done();}
_tryToCollapseMessages(viewMessage,lastMessage){var timestampsShown=this._timestampsSetting.get();if(!timestampsShown&&lastMessage&&!viewMessage.consoleMessage().isGroupMessage()&&viewMessage.consoleMessage().isEqual(lastMessage.consoleMessage())){lastMessage.incrementRepeatCount();if(viewMessage.isLastInSimilarGroup())
lastMessage.setInSimilarGroup(true,true);return true;}
return false;}
_updateMessageList(){this._topGroup=Console.ConsoleGroup.createTopGroup();this._currentGroup=this._topGroup;this._regexMatchRanges=[];this._hiddenByFilterCount=0;for(var i=0;i<this._visibleViewMessages.length;++i){this._visibleViewMessages[i].resetCloseGroupDecorationCount();this._visibleViewMessages[i].resetIncrementRepeatCount();}
this._visibleViewMessages=[];if(this._groupSimilarSetting.get()){this._addGroupableMessagesToEnd();}else{for(var i=0;i<this._consoleMessages.length;++i){this._consoleMessages[i].setInSimilarGroup(false);this._appendMessageToEnd(this._consoleMessages[i]);}}
this._updateFilterStatus();this._searchableView.updateSearchMatchesCount(this._regexMatchRanges.length);this._viewport.invalidate();}
_addGroupableMessagesToEnd(){var alreadyAdded=new Set();for(var i=0;i<this._consoleMessages.length;++i){var viewMessage=this._consoleMessages[i];var message=viewMessage.consoleMessage();if(alreadyAdded.has(message))
continue;if(!message.isGroupable()){this._appendMessageToEnd(viewMessage);alreadyAdded.add(message);continue;}
var key=viewMessage.groupKey();var viewMessagesInGroup=this._groupableMessages.get(key);if(!viewMessagesInGroup||viewMessagesInGroup.length<5){viewMessage.setInSimilarGroup(false);this._appendMessageToEnd(viewMessage);alreadyAdded.add(message);continue;}
if(!viewMessagesInGroup.find(x=>this._shouldMessageBeVisible(x))){alreadyAdded.addAll(viewMessagesInGroup);continue;}
var startGroupViewMessage=this._groupableMessageTitle.get(key);if(!startGroupViewMessage){var startGroupMessage=new ConsoleModel.ConsoleMessage(null,message.source,message.level,viewMessage.groupTitle(),ConsoleModel.ConsoleMessage.MessageType.StartGroupCollapsed);startGroupViewMessage=this._createViewMessage(startGroupMessage);this._groupableMessageTitle.set(key,startGroupViewMessage);}
startGroupViewMessage.setRepeatCount(viewMessagesInGroup.length);this._appendMessageToEnd(startGroupViewMessage);for(var viewMessageInGroup of viewMessagesInGroup){viewMessageInGroup.setInSimilarGroup(true,viewMessagesInGroup.peekLast()===viewMessageInGroup);this._appendMessageToEnd(viewMessageInGroup,true);alreadyAdded.add(viewMessageInGroup.consoleMessage());}
var endGroupMessage=new ConsoleModel.ConsoleMessage(null,message.source,message.level,message.messageText,ConsoleModel.ConsoleMessage.MessageType.EndGroup);this._appendMessageToEnd(this._createViewMessage(endGroupMessage));}}
_messagesClicked(event){if(!this._messagesElement.hasSelection()){var clickedOutsideMessageList=event.target===this._messagesElement;if(clickedOutsideMessageList)
this._prompt.moveCaretToEndOfPrompt();var oldScrollTop=this._viewport.element.scrollTop;this.focus();this._viewport.element.scrollTop=oldScrollTop;}
var groupMessage=event.target.enclosingNodeOrSelfWithClass('console-group-title');if(!groupMessage)
return;var consoleGroupViewMessage=groupMessage.message;consoleGroupViewMessage.setCollapsed(!consoleGroupViewMessage.collapsed());this._updateMessageList();}
_registerShortcuts(){this._shortcuts={};var shortcut=UI.KeyboardShortcut;var section=UI.shortcutsScreen.section(Common.UIString('Console'));var shortcutL=shortcut.makeDescriptor('l',UI.KeyboardShortcut.Modifiers.Ctrl);var keys=[shortcutL];if(Host.isMac()){var shortcutK=shortcut.makeDescriptor('k',UI.KeyboardShortcut.Modifiers.Meta);keys.unshift(shortcutK);}
section.addAlternateKeys(keys,Common.UIString('Clear console'));keys=[shortcut.makeDescriptor(shortcut.Keys.Tab),shortcut.makeDescriptor(shortcut.Keys.Right)];section.addRelatedKeys(keys,Common.UIString('Accept suggestion'));var shortcutU=shortcut.makeDescriptor('u',UI.KeyboardShortcut.Modifiers.Ctrl);this._shortcuts[shortcutU.key]=this._clearPromptBackwards.bind(this);section.addAlternateKeys([shortcutU],Common.UIString('Clear console prompt'));keys=[shortcut.makeDescriptor(shortcut.Keys.Down),shortcut.makeDescriptor(shortcut.Keys.Up)];section.addRelatedKeys(keys,Common.UIString('Next/previous line'));if(Host.isMac()){keys=[shortcut.makeDescriptor('N',shortcut.Modifiers.Alt),shortcut.makeDescriptor('P',shortcut.Modifiers.Alt)];section.addRelatedKeys(keys,Common.UIString('Next/previous command'));}
section.addKey(shortcut.makeDescriptor(shortcut.Keys.Enter),Common.UIString('Execute command'));}
_clearPromptBackwards(){this._prompt.setText('');}
_promptKeyDown(event){var keyboardEvent=(event);if(keyboardEvent.key==='PageUp'){this._updateStickToBottomOnWheel();return;}
var shortcut=UI.KeyboardShortcut.makeKeyFromEvent(keyboardEvent);var handler=this._shortcuts[shortcut];if(handler){handler();keyboardEvent.preventDefault();}}
_printResult(result,originatingConsoleMessage,exceptionDetails){if(!result)
return;var level=!!exceptionDetails?ConsoleModel.ConsoleMessage.MessageLevel.Error:ConsoleModel.ConsoleMessage.MessageLevel.Info;var message;if(!exceptionDetails){message=new ConsoleModel.ConsoleMessage(result.runtimeModel(),ConsoleModel.ConsoleMessage.MessageSource.JS,level,'',ConsoleModel.ConsoleMessage.MessageType.Result,undefined,undefined,undefined,undefined,[result]);}else{message=ConsoleModel.ConsoleMessage.fromException(result.runtimeModel(),exceptionDetails,ConsoleModel.ConsoleMessage.MessageType.Result,undefined,undefined);}
message.setOriginatingMessage(originatingConsoleMessage);ConsoleModel.consoleModel.addMessage(message);}
_commandEvaluated(event){var data=(event.data);this._prompt.history().pushHistoryItem(data.commandMessage.messageText);this._consoleHistorySetting.set(this._prompt.history().historyData().slice(-Console.ConsoleView.persistedHistorySize));this._printResult(data.result,data.commandMessage,data.exceptionDetails);}
elementsToRestoreScrollPositionsFor(){return[this._messagesElement];}
searchCanceled(){this._cleanupAfterSearch();for(var i=0;i<this._visibleViewMessages.length;++i){var message=this._visibleViewMessages[i];message.setSearchRegex(null);}
this._currentMatchRangeIndex=-1;this._regexMatchRanges=[];delete this._searchRegex;this._viewport.refresh();}
performSearch(searchConfig,shouldJump,jumpBackwards){this.searchCanceled();this._searchableView.updateSearchMatchesCount(0);this._searchRegex=searchConfig.toSearchRegex(true);this._regexMatchRanges=[];this._currentMatchRangeIndex=-1;if(shouldJump)
this._searchShouldJumpBackwards=!!jumpBackwards;this._searchProgressIndicator=new UI.ProgressIndicator();this._searchProgressIndicator.setTitle(Common.UIString('Searching…'));this._searchProgressIndicator.setTotalWork(this._visibleViewMessages.length);this._progressToolbarItem.element.appendChild(this._searchProgressIndicator.element);this._innerSearch(0);}
_cleanupAfterSearch(){delete this._searchShouldJumpBackwards;if(this._innerSearchTimeoutId){clearTimeout(this._innerSearchTimeoutId);delete this._innerSearchTimeoutId;}
if(this._searchProgressIndicator){this._searchProgressIndicator.done();delete this._searchProgressIndicator;}}
_searchFinishedForTests(){}
_innerSearch(index){delete this._innerSearchTimeoutId;if(this._searchProgressIndicator.isCanceled()){this._cleanupAfterSearch();return;}
var startTime=Date.now();for(;index<this._visibleViewMessages.length&&Date.now()-startTime<100;++index)
this._searchMessage(index);this._searchableView.updateSearchMatchesCount(this._regexMatchRanges.length);if(typeof this._searchShouldJumpBackwards!=='undefined'&&this._regexMatchRanges.length){this._jumpToMatch(this._searchShouldJumpBackwards?-1:0);delete this._searchShouldJumpBackwards;}
if(index===this._visibleViewMessages.length){this._cleanupAfterSearch();setTimeout(this._searchFinishedForTests.bind(this),0);return;}
this._innerSearchTimeoutId=setTimeout(this._innerSearch.bind(this,index),100);this._searchProgressIndicator.setWorked(index);}
_searchMessage(index){var message=this._visibleViewMessages[index];message.setSearchRegex(this._searchRegex);for(var i=0;i<message.searchCount();++i)
this._regexMatchRanges.push({messageIndex:index,matchIndex:i});}
jumpToNextSearchResult(){this._jumpToMatch(this._currentMatchRangeIndex+1);}
jumpToPreviousSearchResult(){this._jumpToMatch(this._currentMatchRangeIndex-1);}
supportsCaseSensitiveSearch(){return true;}
supportsRegexSearch(){return true;}
_jumpToMatch(index){if(!this._regexMatchRanges.length)
return;var matchRange;if(this._currentMatchRangeIndex>=0){matchRange=this._regexMatchRanges[this._currentMatchRangeIndex];var message=this._visibleViewMessages[matchRange.messageIndex];message.searchHighlightNode(matchRange.matchIndex).classList.remove(UI.highlightedCurrentSearchResultClassName);}
index=mod(index,this._regexMatchRanges.length);this._currentMatchRangeIndex=index;this._searchableView.updateCurrentMatchIndex(index);matchRange=this._regexMatchRanges[index];var message=this._visibleViewMessages[matchRange.messageIndex];var highlightNode=message.searchHighlightNode(matchRange.matchIndex);highlightNode.classList.add(UI.highlightedCurrentSearchResultClassName);this._viewport.scrollItemIntoView(matchRange.messageIndex);highlightNode.scrollIntoViewIfNeeded();}
_updateStickToBottomOnMouseDown(){this._muteViewportUpdates=true;this._viewport.setStickToBottom(false);if(this._waitForScrollTimeout){clearTimeout(this._waitForScrollTimeout);delete this._waitForScrollTimeout;}}
_updateStickToBottomOnMouseUp(){if(!this._muteViewportUpdates)
return;this._waitForScrollTimeout=setTimeout(updateViewportState.bind(this),200);function updateViewportState(){this._muteViewportUpdates=false;this._viewport.setStickToBottom(this._messagesElement.isScrolledToBottom());if(this._maybeDirtyWhileMuted){this._scheduleViewportRefresh();delete this._maybeDirtyWhileMuted;}
delete this._waitForScrollTimeout;this._updateViewportStickinessForTest();}}
_updateViewportStickinessForTest(){}
_updateStickToBottomOnWheel(){this._updateStickToBottomOnMouseDown();this._updateStickToBottomOnMouseUp();}
_promptTextChanged(){if(this.itemCount()!==0&&this._viewport.firstVisibleIndex()!==this.itemCount())
this._immediatelyScrollToBottom();this._promptTextChangedForTest();}
_promptTextChangedForTest(){}};Console.ConsoleView.persistedHistorySize=300;Console.ConsoleViewFilter=class{constructor(filterChangedCallback){this._filterChanged=filterChangedCallback;this._messageURLFiltersSetting=Common.settings.createSetting('messageURLFilters',{});this._messageLevelFiltersSetting=Console.ConsoleViewFilter.levelFilterSetting();this._hideNetworkMessagesSetting=Common.moduleSetting('hideNetworkMessages');this._filterByExecutionContextSetting=Common.moduleSetting('selectedContextFilterEnabled');this._filterByConsoleAPISetting=Common.moduleSetting('consoleAPIFilterEnabled');this._messageURLFiltersSetting.addChangeListener(this._onFilterChanged.bind(this));this._messageLevelFiltersSetting.addChangeListener(this._onFilterChanged.bind(this));this._hideNetworkMessagesSetting.addChangeListener(this._onFilterChanged.bind(this));this._filterByExecutionContextSetting.addChangeListener(this._onFilterChanged.bind(this));this._filterByConsoleAPISetting.addChangeListener(this._onFilterChanged.bind(this));UI.context.addFlavorChangeListener(SDK.ExecutionContext,this._onFilterChanged,this);var filterKeys=Object.values(Console.ConsoleFilter.FilterType);this._suggestionBuilder=new UI.FilterSuggestionBuilder(filterKeys);this._textFilterUI=new UI.ToolbarInput(Common.UIString('Filter'),0.2,1,Common.UIString('e.g. /event\\d/ -cdn url:a.com'),this._suggestionBuilder.completions.bind(this._suggestionBuilder));this._textFilterUI.addEventListener(UI.ToolbarInput.Event.TextChanged,this._onFilterChanged,this);this._filterParser=new TextUtils.FilterParser(filterKeys);this._currentFilter=new Console.ConsoleFilter('',[],null,this._messageLevelFiltersSetting.get());this._updateCurrentFilter();this._levelLabels={};this._levelLabels[ConsoleModel.ConsoleMessage.MessageLevel.Verbose]=Common.UIString('Verbose');this._levelLabels[ConsoleModel.ConsoleMessage.MessageLevel.Info]=Common.UIString('Info');this._levelLabels[ConsoleModel.ConsoleMessage.MessageLevel.Warning]=Common.UIString('Warnings');this._levelLabels[ConsoleModel.ConsoleMessage.MessageLevel.Error]=Common.UIString('Errors');this._levelMenuButton=new UI.ToolbarButton('');this._levelMenuButton.turnIntoSelect();this._levelMenuButton.addEventListener(UI.ToolbarButton.Events.Click,this._showLevelContextMenu.bind(this));this._updateLevelMenuButtonText();this._messageLevelFiltersSetting.addChangeListener(this._updateLevelMenuButtonText.bind(this));}
onMessageAdded(message){if(message.type===ConsoleModel.ConsoleMessage.MessageType.Command||message.type===ConsoleModel.ConsoleMessage.MessageType.Result||message.isGroupMessage())
return;if(message.context)
this._suggestionBuilder.addItem(Console.ConsoleFilter.FilterType.Context,message.context);if(message.source)
this._suggestionBuilder.addItem(Console.ConsoleFilter.FilterType.Source,message.source);if(message.url)
this._suggestionBuilder.addItem(Console.ConsoleFilter.FilterType.Url,message.url);}
static levelFilterSetting(){return Common.settings.createSetting('messageLevelFilters',Console.ConsoleFilter.defaultLevelsFilterValue());}
_updateCurrentFilter(){var parsedFilters=this._filterParser.parse(this._textFilterUI.value());if(this._hideNetworkMessagesSetting.get()){parsedFilters.push({key:Console.ConsoleFilter.FilterType.Source,text:ConsoleModel.ConsoleMessage.MessageSource.Network,negative:true});}
if(this._filterByConsoleAPISetting.get()){parsedFilters.push({key:Console.ConsoleFilter.FilterType.Source,text:ConsoleModel.ConsoleMessage.MessageSource.ConsoleAPI,negative:false});}
var blockedURLs=Object.keys(this._messageURLFiltersSetting.get());var urlFilters=blockedURLs.map(url=>({key:Console.ConsoleFilter.FilterType.Url,text:url,negative:true}));parsedFilters=parsedFilters.concat(urlFilters);this._currentFilter.executionContext=this._filterByExecutionContextSetting.get()?UI.context.flavor(SDK.ExecutionContext):null;this._currentFilter.parsedFilters=parsedFilters;this._currentFilter.levelsMask=this._messageLevelFiltersSetting.get();}
_onFilterChanged(){this._updateCurrentFilter();this._filterChanged();}
_updateLevelMenuButtonText(){var isAll=true;var isDefault=true;var allValue=Console.ConsoleFilter.allLevelsFilterValue();var defaultValue=Console.ConsoleFilter.defaultLevelsFilterValue();var text=null;var levels=this._messageLevelFiltersSetting.get();for(var name of Object.values(ConsoleModel.ConsoleMessage.MessageLevel)){isAll=isAll&&levels[name]===allValue[name];isDefault=isDefault&&levels[name]===defaultValue[name];if(levels[name])
text=text?Common.UIString('Custom levels'):Common.UIString('%s only',this._levelLabels[name]);}
if(isAll)
text=Common.UIString('All levels');else if(isDefault)
text=Common.UIString('Default levels');else
text=text||Common.UIString('Hide all');this._levelMenuButton.setText(text);}
_showLevelContextMenu(event){var mouseEvent=(event.data);var setting=this._messageLevelFiltersSetting;var levels=setting.get();var contextMenu=new UI.ContextMenu(mouseEvent,true);contextMenu.headerSection().appendItem(Common.UIString('Default'),()=>setting.set(Console.ConsoleFilter.defaultLevelsFilterValue()));for(var level in this._levelLabels){contextMenu.defaultSection().appendCheckboxItem(this._levelLabels[level],toggleShowLevel.bind(null,level),levels[level]);}
contextMenu.show();function toggleShowLevel(level){levels[level]=!levels[level];setting.set(levels);}}
addMessageURLFilter(url){var value=this._messageURLFiltersSetting.get();value[url]=true;this._messageURLFiltersSetting.set(value);}
removeMessageURLFilter(url){var value;if(url){value=this._messageURLFiltersSetting.get();delete value[url];}else{value={};}
this._messageURLFiltersSetting.set(value);}
messageURLFilters(){return this._messageURLFiltersSetting.get();}
shouldBeVisible(viewMessage){return this._currentFilter.shouldBeVisible(viewMessage);}
clear(){this._suggestionBuilder.clear();}
reset(){this._messageURLFiltersSetting.set({});this._messageLevelFiltersSetting.set(Console.ConsoleFilter.defaultLevelsFilterValue());this._filterByExecutionContextSetting.set(false);this._filterByConsoleAPISetting.set(false);this._hideNetworkMessagesSetting.set(false);this._textFilterUI.setValue('');this._onFilterChanged();}};Console.ConsoleCommand=class extends Console.ConsoleViewMessage{constructor(message,linkifier,badgePool,nestingLevel){super(message,linkifier,badgePool,nestingLevel);}
contentElement(){if(!this._contentElement){this._contentElement=createElementWithClass('div','console-user-command');var icon=UI.Icon.create('smallicon-user-command','command-result-icon');this._contentElement.appendChild(icon);this._contentElement.message=this;this._formattedCommand=createElementWithClass('span','source-code');this._formattedCommand.textContent=this.text.replaceControlCharacters();this._contentElement.appendChild(this._formattedCommand);if(this._formattedCommand.textContent.length<Console.ConsoleCommand.MaxLengthToIgnoreHighlighter){var javascriptSyntaxHighlighter=new UI.SyntaxHighlighter('text/javascript',true);javascriptSyntaxHighlighter.syntaxHighlightNode(this._formattedCommand).then(this._updateSearch.bind(this));}else{this._updateSearch();}
this.updateTimestamp();}
return this._contentElement;}
_updateSearch(){this.setSearchRegex(this.searchRegex());}};Console.ConsoleCommand.MaxLengthToIgnoreHighlighter=10000;Console.ConsoleCommandResult=class extends Console.ConsoleViewMessage{constructor(message,linkifier,badgePool,nestingLevel){super(message,linkifier,badgePool,nestingLevel);}
contentElement(){var element=super.contentElement();if(!element.classList.contains('console-user-command-result')){element.classList.add('console-user-command-result');if(this.consoleMessage().level===ConsoleModel.ConsoleMessage.MessageLevel.Info){var icon=UI.Icon.create('smallicon-command-result','command-result-icon');element.insertBefore(icon,element.firstChild);}}
return element;}};Console.ConsoleGroup=class{constructor(parentGroup,groupMessage){this._parentGroup=parentGroup;this._nestingLevel=parentGroup?parentGroup.nestingLevel()+1:0;this._messagesHidden=groupMessage&&groupMessage.collapsed()||this._parentGroup&&this._parentGroup.messagesHidden();}
static createTopGroup(){return new Console.ConsoleGroup(null,null);}
messagesHidden(){return this._messagesHidden;}
nestingLevel(){return this._nestingLevel;}
parentGroup(){return this._parentGroup;}};Console.ConsoleView.ActionDelegate=class{handleAction(context,actionId){switch(actionId){case'console.show':InspectorFrontendHost.bringToFront();Common.console.show();return true;case'console.clear':Console.ConsoleView.clearConsole();return true;case'console.clear.history':Console.ConsoleView.instance()._clearHistory();return true;}
return false;}};Console.ConsoleView.RegexMatchRange;Console.ConsoleView._messageSortingTimeSymbol=Symbol('messageSortingTime');;Console.ConsolePanel=class extends UI.Panel{constructor(){super('console');this._view=Console.ConsoleView.instance();}
static instance(){return(self.runtime.sharedInstance(Console.ConsolePanel));}
wasShown(){super.wasShown();var wrapper=Console.ConsolePanel.WrapperView._instance;if(wrapper&&wrapper.isShowing())
UI.inspectorView.setDrawerMinimized(true);this._view.show(this.element);}
willHide(){super.willHide();if(Console.ConsolePanel.WrapperView._instance)
Console.ConsolePanel.WrapperView._instance._showViewInWrapper();UI.inspectorView.setDrawerMinimized(false);}
searchableView(){return Console.ConsoleView.instance().searchableView();}};Console.ConsolePanel.WrapperView=class extends UI.VBox{constructor(){super();this.element.classList.add('console-view-wrapper');Console.ConsolePanel.WrapperView._instance=this;this._view=Console.ConsoleView.instance();}
wasShown(){if(!Console.ConsolePanel.instance().isShowing())
this._showViewInWrapper();else
UI.inspectorView.setDrawerMinimized(true);}
willHide(){UI.inspectorView.setDrawerMinimized(false);}
_showViewInWrapper(){this._view.show(this.element);}};Console.ConsolePanel.ConsoleRevealer=class{reveal(object){var consoleView=Console.ConsoleView.instance();if(consoleView.isShowing()){consoleView.focus();return Promise.resolve();}
UI.viewManager.showView('console-view');return Promise.resolve();}};;Runtime.cachedResources["console/consoleContextSelector.css"]="/*\n * Copyright 2017 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n:host {\n    padding: 2px 1px 2px 2px;\n    white-space: nowrap;\n    display: flex;\n    flex-direction: column;\n    height: 36px;\n    justify-content: center;\n}\n\n.title {\n    overflow: hidden;\n    text-overflow: ellipsis;\n    flex-grow: 0;\n}\n\n.badge {\n    pointer-events: none;\n    margin-right: 4px;\n    display: inline-block;\n    height: 15px;\n}\n\n.subtitle {\n    color: #999;\n    margin-right: 3px;\n    overflow: hidden;\n    text-overflow: ellipsis;\n    flex-grow: 0;\n}\n\n:host(.highlighted) .subtitle {\n    color: inherit;\n}\n\n/*# sourceURL=console/consoleContextSelector.css */";Runtime.cachedResources["console/consoleSidebar.css"]="/*\n * Copyright (c) 2017 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n:host {\n    overflow: auto;\n}\n\n.tree-outline-disclosure {\n    max-width: 100%;\n    padding-left: 6px;\n}\n\n.count {\n    flex: none;\n    margin: 0 8px;\n}\n\n[is=ui-icon] {\n    margin: 0 5px;\n}\n\n[is=ui-icon].icon-warning {\n    background: linear-gradient(45deg, hsla(48, 100%, 50%, 1), hsla(48, 70%, 50%, 1));\n}\n\nli {\n    height: 24px;\n}\n\nli .largeicon-navigator-file {\n    background: linear-gradient(45deg, hsl(48, 70%, 50%), hsl(48, 70%, 70%));\n    margin: 0;\n}\n\nli .largeicon-navigator-folder {\n    background: linear-gradient(45deg, hsl(210, 82%, 65%), hsl(210, 82%, 80%));\n    margin: -3px -3px 0 -5px;\n}\n\n.tree-element-title {\n    flex-shrink: 100;\n    flex-grow: 1;\n    overflow: hidden;\n    text-overflow: ellipsis;\n}\n\n/*# sourceURL=console/consoleSidebar.css */";Runtime.cachedResources["console/consoleView.css"]="/*\n * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.\n * Copyright (C) 2009 Anthony Ricaud <rik@webkit.org>\n *\n * Redistribution and use in source and binary forms, with or without\n * modification, are permitted provided that the following conditions\n * are met:\n *\n * 1.  Redistributions of source code must retain the above copyright\n *     notice, this list of conditions and the following disclaimer.\n * 2.  Redistributions in binary form must reproduce the above copyright\n *     notice, this list of conditions and the following disclaimer in the\n *     documentation and/or other materials provided with the distribution.\n * 3.  Neither the name of Apple Computer, Inc. (\"Apple\") nor the names of\n *     its contributors may be used to endorse or promote products derived\n *     from this software without specific prior written permission.\n *\n * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS \"AS IS\" AND ANY\n * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY\n * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND\n * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF\n * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n */\n\n.console-view {\n    background-color: white;\n    overflow: hidden;\n}\n\n.console-view > .toolbar {\n    border-bottom: 1px solid #dadada;\n}\n\n.console-view-wrapper {\n    background-color: #eee;\n}\n\n.console-view-fix-select-all {\n    height: 0;\n    overflow: hidden;\n}\n\n.console-settings-pane {\n    flex: none;\n    border-bottom: 1px solid #dadada;\n}\n\n.console-settings-pane .toolbar {\n    flex: 1 1;\n}\n\n#console-messages {\n    flex: 1 1;\n    padding: 2px 0;\n    overflow-y: auto;\n    word-wrap: break-word;\n    -webkit-user-select: text;\n    transform: translateZ(0);\n}\n\n#console-prompt {\n    clear: right;\n    position: relative;\n    margin: 0 22px 0 20px;\n    min-height: 18px;  /* Sync with ConsoleViewMessage.js */\n}\n\n#console-prompt .CodeMirror {\n    padding: 3px 0 1px 0;\n}\n\n#console-prompt .CodeMirror-line {\n    padding-top: 0;\n}\n\n#console-prompt .CodeMirror-lines {\n    padding-top: 0;\n}\n\n#console-prompt .console-prompt-icon {\n    position: absolute;\n    left: -13px;\n    top: 5px;\n    -webkit-user-select: none;\n}\n\n.console-message,\n.console-user-command {\n    clear: right;\n    position: relative;\n    padding: 3px 22px 1px 0;\n    margin-left: 24px;\n    min-height: 18px;  /* Sync with ConsoleViewMessage.js */\n    flex: auto;\n    display: flex;\n}\n\n.console-message > * {\n    flex: auto;\n}\n\n.console-timestamp {\n    color: gray;\n    -webkit-user-select: none;\n    flex: none;\n    margin-right: 5px;\n}\n\n.message-level-icon, .command-result-icon {\n    position: absolute;\n    left: -17px;\n    top: 4px;\n    -webkit-user-select: none;\n}\n\n.console-message-repeat-count {\n    margin: 2px 0 0 10px;\n    flex: none;\n}\n\n.repeated-message {\n    margin-left: 4px;\n}\n\n.repeated-message .message-level-icon {\n    display: none;\n}\n\n.repeated-message .console-message-stack-trace-toggle,\n.repeated-message > .console-message-text {\n    flex: 1;\n}\n\n.console-error-level .repeated-message,\n.console-warning-level .repeated-message,\n.console-verbose-level .repeated-message,\n.console-info-level .repeated-message {\n    display: flex;\n}\n\n.console-info {\n    color: rgb(128, 128, 128);\n    font-style: italic;\n    padding-bottom: 2px;\n}\n\n.console-group .console-group > .console-group-messages {\n    margin-left: 16px;\n}\n\n.console-group-title.console-from-api {\n    font-weight: bold;\n}\n\n.console-group-title .console-message {\n    margin-left: 12px;\n}\n\n.expand-group-icon {\n    -webkit-user-select: none;\n    flex: none;\n    background-color: rgb(110, 110, 110);\n    position: relative;\n    left: 10px;\n    top: 5px;\n    margin-right: 2px;\n}\n\n.console-group-title .message-level-icon {\n    display: none;\n}\n\n.console-message-repeat-count .expand-group-icon {\n    left: 2px;\n    top: 2px;\n    background-color: #fff;\n    margin-right: 4px;\n}\n\n.console-group {\n    position: relative;\n}\n\n.console-message-wrapper {\n    display: flex;\n    border-bottom: 1px solid rgb(240, 240, 240);\n}\n\n.console-message-wrapper.console-adjacent-user-command-result {\n    border-bottom: none;\n}\n\n.console-message-wrapper.console-error-level {\n    border-top: 1px solid hsl(0, 100%, 92%);\n    border-bottom: 1px solid hsl(0, 100%, 92%);\n    margin-top: -1px;\n}\n\n.console-message-wrapper.console-warning-level {\n    border-top: 1px solid hsl(50, 100%, 88%);\n    border-bottom: 1px solid hsl(50, 100%, 88%);\n    margin-top: -1px;\n}\n\n.console-message-wrapper .nesting-level-marker {\n    width: 14px;\n    flex: 0 0 auto;\n    border-right: 1px solid #a5a5a5;\n    position: relative;\n    margin-bottom: -1px;\n}\n\n.console-message-wrapper:last-child .nesting-level-marker::before,\n.console-message-wrapper .nesting-level-marker.group-closed::before {\n    content: \"\";\n}\n\n.console-message-wrapper .nesting-level-marker::before {\n    border-bottom: 1px solid #a5a5a5;\n    position: absolute;\n    top: 0;\n    left: 0;\n    margin-left: 100%;\n    width: 3px;\n    height: 100%;\n    box-sizing: border-box;\n}\n\n.console-error-level {\n    background-color: hsl(0, 100%, 97%);\n}\n\n.-theme-with-dark-background .console-error-level {\n    background-color: hsl(0, 100%, 8%);\n}\n\n.console-warning-level {\n    background-color: hsl(50, 100%, 95%);\n}\n\n.-theme-with-dark-background .console-warning-level {\n    background-color: hsl(50, 100%, 10%);\n}\n\n.console-warning-level .console-message-text {\n    color: hsl(39, 100%, 18%);\n}\n\n.console-error-level .console-message-text,\n.console-error-level .console-view-object-properties-section {\n    color: red !important;\n}\n\n.-theme-with-dark-background .console-error-level .console-message-text,\n.-theme-with-dark-background .console-error-level .console-view-object-properties-section {\n    color: hsl(0, 100%, 75%) !important;\n}\n\n.-theme-with-dark-background .console-verbose-level .console-message-text {\n    color: hsl(220, 100%, 65%) !important;\n}\n\n.console-message.console-warning-level {\n    background-color: rgb(255, 250, 224);\n}\n\n#console-messages .link {\n    text-decoration: underline;\n}\n\n#console-messages .link,\n#console-messages .devtools-link {\n    color: rgb(33%, 33%, 33%);\n    cursor: pointer;\n    word-break: break-all;\n}\n\n#console-messages .link:hover,\n#console-messages .devtools-link:hover {\n    color: rgb(15%, 15%, 15%);\n}\n\n.console-group-messages .section {\n    margin: 0 0 0 12px !important;\n}\n\n.console-group-messages .section > .header {\n    padding: 0 8px 0 0;\n    background-image: none;\n    border: none;\n    min-height: 0;\n}\n\n.console-group-messages .section > .header::before {\n    margin-left: -12px;\n}\n\n.console-group-messages .section > .header .title {\n    color: #222;\n    font-weight: normal;\n    line-height: 13px;\n}\n\n.console-group-messages .section .properties li .info {\n    padding-top: 0;\n    padding-bottom: 0;\n    color: rgb(60%, 60%, 60%);\n}\n\n.console-object-preview {\n    white-space: normal;\n    word-wrap: break-word;\n    font-style: italic;\n}\n\n.console-object-preview .name {\n    /* Follows .section .properties .name, .event-properties .name */\n    color: rgb(136, 19, 145);\n    flex-shrink: 0;\n}\n\n.console-message-text .object-value-string {\n    white-space: pre-wrap;\n}\n\n.console-message-formatted-table {\n    clear: both;\n}\n\n.console-message-anchor {\n    float: right;\n    text-align: right;\n    max-width: 100%;\n    margin-left: 4px;\n}\n\n.console-message-badge {\n    float: right;\n    margin-left: 4px;\n}\n\n.console-message-nowrap-below,\n.console-message-nowrap-below div,\n.console-message-nowrap-below span {\n    white-space: nowrap !important;\n}\n\n.object-state-note {\n    display: inline-block;\n    width: 11px;\n    height: 11px;\n    color: white;\n    text-align: center;\n    border-radius: 3px;\n    line-height: 13px;\n    margin: 0 6px;\n    font-size: 9px;\n}\n\n.-theme-with-dark-background .object-state-note {\n    background-color: hsl(230, 100%, 80%);\n}\n\n.info-note {\n    background-color: rgb(179, 203, 247);\n}\n\n.info-note::before {\n    content: \"i\";\n}\n\n.console-view-object-properties-section:not(.expanded) .info-note {\n    display: none;\n}\n\n.console-view-object-properties-section {\n    padding: 0px;\n    position: relative;\n    vertical-align: baseline;\n    color: inherit;\n    display: inline-block;\n}\n\n.console-message-stack-trace-toggle {\n    display: flex;\n    flex-direction: row;\n    align-items: flex-start;\n}\n\n.console-message-stack-trace-wrapper {\n    flex: 1 1 auto;\n    display: flex;\n    flex-direction: column;\n    align-items: stretch;\n}\n\n.console-message-stack-trace-wrapper > * {\n    flex: none;\n}\n\n.console-message-expand-icon {\n    margin-bottom: -2px;\n}\n/*# sourceURL=console/consoleView.css */";