SourceFrame.SourcesTextEditor=class extends TextEditor.CodeMirrorTextEditor{constructor(delegate){super({lineNumbers:true,lineWrapping:false,bracketMatchingSetting:Common.moduleSetting('textEditorBracketMatching'),padBottom:true});this.codeMirror().addKeyMap({'Enter':'smartNewlineAndIndent','Esc':'sourcesDismiss'});this._delegate=delegate;this.codeMirror().on('cursorActivity',this._cursorActivity.bind(this));this.codeMirror().on('gutterClick',this._gutterClick.bind(this));this.codeMirror().on('scroll',this._scroll.bind(this));this.codeMirror().on('focus',this._focus.bind(this));this.codeMirror().on('blur',this._blur.bind(this));this.codeMirror().on('beforeSelectionChange',this._fireBeforeSelectionChanged.bind(this));this.element.addEventListener('contextmenu',this._contextMenu.bind(this),false);this.codeMirror().addKeyMap(SourceFrame.SourcesTextEditor._BlockIndentController);this._tokenHighlighter=new SourceFrame.SourcesTextEditor.TokenHighlighter(this,this.codeMirror());this._gutters=['CodeMirror-linenumbers'];this.codeMirror().setOption('gutters',this._gutters.slice());this.codeMirror().setOption('electricChars',false);this.codeMirror().setOption('smartIndent',false);function updateAnticipateJumpFlag(value){this._isHandlingMouseDownEvent=value;}
this.element.addEventListener('mousedown',updateAnticipateJumpFlag.bind(this,true),true);this.element.addEventListener('mousedown',updateAnticipateJumpFlag.bind(this,false),false);Common.moduleSetting('textEditorIndent').addChangeListener(this._onUpdateEditorIndentation,this);Common.moduleSetting('textEditorAutoDetectIndent').addChangeListener(this._onUpdateEditorIndentation,this);Common.moduleSetting('showWhitespacesInEditor').addChangeListener(this._updateWhitespace,this);this._onUpdateEditorIndentation();this._setupWhitespaceHighlight();}
static _guessIndentationLevel(lines){var tabRegex=/^\t+/;var tabLines=0;var indents={};for(var lineNumber=0;lineNumber<lines.length;++lineNumber){var text=lines[lineNumber];if(text.length===0||!TextUtils.TextUtils.isSpaceChar(text[0]))
continue;if(tabRegex.test(text)){++tabLines;continue;}
var i=0;while(i<text.length&&TextUtils.TextUtils.isSpaceChar(text[i]))
++i;if(i%2!==0)
continue;indents[i]=1+(indents[i]||0);}
var linesCountPerIndentThreshold=3*lines.length/100;if(tabLines&&tabLines>linesCountPerIndentThreshold)
return'\t';var minimumIndent=Infinity;for(var i in indents){if(indents[i]<linesCountPerIndentThreshold)
continue;var indent=parseInt(i,10);if(minimumIndent>indent)
minimumIndent=indent;}
if(minimumIndent===Infinity)
return Common.moduleSetting('textEditorIndent').get();return' '.repeat(minimumIndent);}
_isSearchActive(){return!!this._tokenHighlighter.highlightedRegex();}
scrollToLine(lineNumber){super.scrollToLine(lineNumber);this._scroll();}
highlightSearchResults(regex,range){function innerHighlightRegex(){if(range){this.scrollLineIntoView(range.startLine);if(range.endColumn>TextEditor.CodeMirrorTextEditor.maxHighlightLength)
this.setSelection(range);else
this.setSelection(TextUtils.TextRange.createFromLocation(range.startLine,range.startColumn));}
this._tokenHighlighter.highlightSearchResults(regex,range);}
if(!this._selectionBeforeSearch)
this._selectionBeforeSearch=this.selection();this.codeMirror().operation(innerHighlightRegex.bind(this));}
cancelSearchResultsHighlight(){this.codeMirror().operation(this._tokenHighlighter.highlightSelectedTokens.bind(this._tokenHighlighter));if(this._selectionBeforeSearch){this._reportJump(this._selectionBeforeSearch,this.selection());delete this._selectionBeforeSearch;}}
removeHighlight(highlightDescriptor){highlightDescriptor.clear();}
highlightRange(range,cssClass){cssClass='CodeMirror-persist-highlight '+cssClass;var pos=TextEditor.CodeMirrorUtils.toPos(range);++pos.end.ch;return this.codeMirror().markText(pos.start,pos.end,{className:cssClass,startStyle:cssClass+'-start',endStyle:cssClass+'-end'});}
installGutter(type,leftToNumbers){if(this._gutters.indexOf(type)!==-1)
return;if(leftToNumbers)
this._gutters.unshift(type);else
this._gutters.push(type);this.codeMirror().setOption('gutters',this._gutters.slice());this.refresh();}
uninstallGutter(type){var index=this._gutters.indexOf(type);if(index===-1)
return;this.codeMirror().clearGutter(type);this._gutters.splice(index,1);this.codeMirror().setOption('gutters',this._gutters.slice());this.refresh();}
setGutterDecoration(lineNumber,type,element){console.assert(this._gutters.indexOf(type)!==-1,'Cannot decorate unexisting gutter.');this.codeMirror().setGutterMarker(lineNumber,type,element);}
setExecutionLocation(lineNumber,columnNumber){this.clearPositionHighlight();this._executionLine=this.codeMirror().getLineHandle(lineNumber);if(!this._executionLine)
return;this.showExecutionLineBackground();this.codeMirror().addLineClass(this._executionLine,'wrap','cm-execution-line-outline');var token=this.tokenAtTextPosition(lineNumber,columnNumber);if(token&&!token.type&&token.startColumn+1===token.endColumn){var tokenContent=this.codeMirror().getLine(lineNumber)[token.startColumn];if(tokenContent==='.'||tokenContent==='(')
token=this.tokenAtTextPosition(lineNumber,token.endColumn+1);}
var endColumn;if(token&&token.type)
endColumn=token.endColumn;else
endColumn=this.codeMirror().getLine(lineNumber).length;this._executionLineTailMarker=this.codeMirror().markText({line:lineNumber,ch:columnNumber},{line:lineNumber,ch:endColumn},{className:'cm-execution-line-tail'});}
showExecutionLineBackground(){if(this._executionLine)
this.codeMirror().addLineClass(this._executionLine,'wrap','cm-execution-line');}
hideExecutionLineBackground(){if(this._executionLine)
this.codeMirror().removeLineClass(this._executionLine,'wrap','cm-execution-line');}
clearExecutionLine(){this.clearPositionHighlight();if(this._executionLine){this.hideExecutionLineBackground();this.codeMirror().removeLineClass(this._executionLine,'wrap','cm-execution-line-outline');}
delete this._executionLine;if(this._executionLineTailMarker)
this._executionLineTailMarker.clear();delete this._executionLineTailMarker;}
toggleLineClass(lineNumber,className,toggled){if(this.hasLineClass(lineNumber,className)===toggled)
return;var lineHandle=this.codeMirror().getLineHandle(lineNumber);if(!lineHandle)
return;if(toggled){this.codeMirror().addLineClass(lineHandle,'gutter',className);this.codeMirror().addLineClass(lineHandle,'wrap',className);}else{this.codeMirror().removeLineClass(lineHandle,'gutter',className);this.codeMirror().removeLineClass(lineHandle,'wrap',className);}}
hasLineClass(lineNumber,className){var lineInfo=this.codeMirror().lineInfo(lineNumber);var wrapClass=lineInfo.wrapClass||'';var classNames=wrapClass.split(' ');return classNames.indexOf(className)!==-1;}
_gutterClick(instance,lineNumber,gutter,event){this.dispatchEventToListeners(SourceFrame.SourcesTextEditor.Events.GutterClick,{lineNumber:lineNumber,event:event});}
_contextMenu(event){var contextMenu=new UI.ContextMenu(event);event.consume(true);var wrapper=event.target.enclosingNodeOrSelfWithClass('CodeMirror-gutter-wrapper');var target=wrapper?wrapper.querySelector('.CodeMirror-linenumber'):null;var promise;if(target){promise=this._delegate.populateLineGutterContextMenu(contextMenu,parseInt(target.textContent,10)-1);}else{var textSelection=this.selection();promise=this._delegate.populateTextAreaContextMenu(contextMenu,textSelection.startLine,textSelection.startColumn);}
promise.then(showAsync.bind(this));function showAsync(){contextMenu.appendApplicableItems(this);contextMenu.show();}}
editRange(range,text,origin){var newRange=super.editRange(range,text,origin);if(Common.moduleSetting('textEditorAutoDetectIndent').get())
this._onUpdateEditorIndentation();return newRange;}
_onUpdateEditorIndentation(){this._setEditorIndentation(TextEditor.CodeMirrorUtils.pullLines(this.codeMirror(),SourceFrame.SourcesTextEditor.LinesToScanForIndentationGuessing));}
_setEditorIndentation(lines){var extraKeys={};var indent=Common.moduleSetting('textEditorIndent').get();if(Common.moduleSetting('textEditorAutoDetectIndent').get())
indent=SourceFrame.SourcesTextEditor._guessIndentationLevel(lines);if(indent===TextUtils.TextUtils.Indent.TabCharacter){this.codeMirror().setOption('indentWithTabs',true);this.codeMirror().setOption('indentUnit',4);}else{this.codeMirror().setOption('indentWithTabs',false);this.codeMirror().setOption('indentUnit',indent.length);extraKeys.Tab=function(codeMirror){if(codeMirror.somethingSelected())
return CodeMirror.Pass;var pos=codeMirror.getCursor('head');codeMirror.replaceRange(indent.substring(pos.ch%indent.length),codeMirror.getCursor());};}
this.codeMirror().setOption('extraKeys',extraKeys);this._indentationLevel=indent;}
indent(){return this._indentationLevel;}
_onAutoAppendedSpaces(){this._autoAppendedSpaces=this._autoAppendedSpaces||[];for(var i=0;i<this._autoAppendedSpaces.length;++i){var position=this._autoAppendedSpaces[i].resolve();if(!position)
continue;var line=this.line(position.lineNumber);if(line.length===position.columnNumber&&TextUtils.TextUtils.lineIndent(line).length===line.length){this.codeMirror().replaceRange('',new CodeMirror.Pos(position.lineNumber,0),new CodeMirror.Pos(position.lineNumber,position.columnNumber));}}
this._autoAppendedSpaces=[];var selections=this.selections();for(var i=0;i<selections.length;++i){var selection=selections[i];this._autoAppendedSpaces.push(this.textEditorPositionHandle(selection.startLine,selection.startColumn));}}
_cursorActivity(){if(!this._isSearchActive())
this.codeMirror().operation(this._tokenHighlighter.highlightSelectedTokens.bind(this._tokenHighlighter));var start=this.codeMirror().getCursor('anchor');var end=this.codeMirror().getCursor('head');this.dispatchEventToListeners(SourceFrame.SourcesTextEditor.Events.SelectionChanged,TextEditor.CodeMirrorUtils.toRange(start,end));}
_reportJump(from,to){if(from&&to&&from.equal(to))
return;this.dispatchEventToListeners(SourceFrame.SourcesTextEditor.Events.JumpHappened,{from:from,to:to});}
_scroll(){var topmostLineNumber=this.codeMirror().lineAtHeight(this.codeMirror().getScrollInfo().top,'local');this.dispatchEventToListeners(SourceFrame.SourcesTextEditor.Events.ScrollChanged,topmostLineNumber);}
_focus(){this.dispatchEventToListeners(SourceFrame.SourcesTextEditor.Events.EditorFocused);}
_blur(){this.dispatchEventToListeners(SourceFrame.SourcesTextEditor.Events.EditorBlurred);}
_fireBeforeSelectionChanged(codeMirror,selection){if(!this._isHandlingMouseDownEvent)
return;if(!selection.ranges.length)
return;var primarySelection=selection.ranges[0];this._reportJump(this.selection(),TextEditor.CodeMirrorUtils.toRange(primarySelection.anchor,primarySelection.head));}
dispose(){super.dispose();Common.moduleSetting('textEditorIndent').removeChangeListener(this._onUpdateEditorIndentation,this);Common.moduleSetting('textEditorAutoDetectIndent').removeChangeListener(this._onUpdateEditorIndentation,this);Common.moduleSetting('showWhitespacesInEditor').removeChangeListener(this._updateWhitespace,this);}
setText(text){this._setEditorIndentation(text.split('\n').slice(0,SourceFrame.SourcesTextEditor.LinesToScanForIndentationGuessing));super.setText(text);}
_updateWhitespace(){this.setMimeType(this.mimeType());}
rewriteMimeType(mimeType){this._setupWhitespaceHighlight();var whitespaceMode=Common.moduleSetting('showWhitespacesInEditor').get();this.element.classList.toggle('show-whitespaces',whitespaceMode==='all');if(whitespaceMode==='all')
return this._allWhitespaceOverlayMode(mimeType);else if(whitespaceMode==='trailing')
return this._trailingWhitespaceOverlayMode(mimeType);return mimeType;}
_allWhitespaceOverlayMode(mimeType){var modeName=CodeMirror.mimeModes[mimeType]?(CodeMirror.mimeModes[mimeType].name||CodeMirror.mimeModes[mimeType]):CodeMirror.mimeModes['text/plain'];modeName+='+all-whitespaces';if(CodeMirror.modes[modeName])
return modeName;function modeConstructor(config,parserConfig){function nextToken(stream){if(stream.peek()===' '){var spaces=0;while(spaces<SourceFrame.SourcesTextEditor.MaximumNumberOfWhitespacesPerSingleSpan&&stream.peek()===' '){++spaces;stream.next();}
return'whitespace whitespace-'+spaces;}
while(!stream.eol()&&stream.peek()!==' ')
stream.next();return null;}
var whitespaceMode={token:nextToken};return CodeMirror.overlayMode(CodeMirror.getMode(config,mimeType),whitespaceMode,false);}
CodeMirror.defineMode(modeName,modeConstructor);return modeName;}
_trailingWhitespaceOverlayMode(mimeType){var modeName=CodeMirror.mimeModes[mimeType]?(CodeMirror.mimeModes[mimeType].name||CodeMirror.mimeModes[mimeType]):CodeMirror.mimeModes['text/plain'];modeName+='+trailing-whitespaces';if(CodeMirror.modes[modeName])
return modeName;function modeConstructor(config,parserConfig){function nextToken(stream){if(stream.match(/^\s+$/,true))
return true?'trailing-whitespace':null;do
stream.next();while(!stream.eol()&&stream.peek()!==' ');return null;}
var whitespaceMode={token:nextToken};return CodeMirror.overlayMode(CodeMirror.getMode(config,mimeType),whitespaceMode,false);}
CodeMirror.defineMode(modeName,modeConstructor);return modeName;}
_setupWhitespaceHighlight(){var doc=this.element.ownerDocument;if(doc._codeMirrorWhitespaceStyleInjected||!Common.moduleSetting('showWhitespacesInEditor').get())
return;doc._codeMirrorWhitespaceStyleInjected=true;const classBase='.show-whitespaces .CodeMirror .cm-whitespace-';const spaceChar='·';var spaceChars='';var rules='';for(var i=1;i<=SourceFrame.SourcesTextEditor.MaximumNumberOfWhitespacesPerSingleSpan;++i){spaceChars+=spaceChar;var rule=classBase+i+'::before { content: \''+spaceChars+'\';}\n';rules+=rule;}
var style=doc.createElement('style');style.textContent=rules;doc.head.appendChild(style);}};SourceFrame.SourcesTextEditor.GutterClickEventData;SourceFrame.SourcesTextEditor.Events={GutterClick:Symbol('GutterClick'),SelectionChanged:Symbol('SelectionChanged'),ScrollChanged:Symbol('ScrollChanged'),EditorFocused:Symbol('EditorFocused'),EditorBlurred:Symbol('EditorBlurred'),JumpHappened:Symbol('JumpHappened')};SourceFrame.SourcesTextEditorDelegate=function(){};SourceFrame.SourcesTextEditorDelegate.prototype={populateLineGutterContextMenu(contextMenu,lineNumber){},populateTextAreaContextMenu(contextMenu,lineNumber,columnNumber){},};CodeMirror.commands.smartNewlineAndIndent=function(codeMirror){codeMirror.operation(innerSmartNewlineAndIndent.bind(null,codeMirror));function innerSmartNewlineAndIndent(codeMirror){var selections=codeMirror.listSelections();var replacements=[];for(var i=0;i<selections.length;++i){var selection=selections[i];var cur=CodeMirror.cmpPos(selection.head,selection.anchor)<0?selection.head:selection.anchor;var line=codeMirror.getLine(cur.line);var indent=TextUtils.TextUtils.lineIndent(line);replacements.push('\n'+indent.substring(0,Math.min(cur.ch,indent.length)));}
codeMirror.replaceSelections(replacements);codeMirror._codeMirrorTextEditor._onAutoAppendedSpaces();}};CodeMirror.commands.sourcesDismiss=function(codemirror){if(codemirror.listSelections().length===1&&codemirror._codeMirrorTextEditor._isSearchActive())
return CodeMirror.Pass;return CodeMirror.commands.dismiss(codemirror);};SourceFrame.SourcesTextEditor._BlockIndentController={name:'blockIndentKeymap',Enter:function(codeMirror){var selections=codeMirror.listSelections();var replacements=[];var allSelectionsAreCollapsedBlocks=false;for(var i=0;i<selections.length;++i){var selection=selections[i];var start=CodeMirror.cmpPos(selection.head,selection.anchor)<0?selection.head:selection.anchor;var line=codeMirror.getLine(start.line);var indent=TextUtils.TextUtils.lineIndent(line);var indentToInsert='\n'+indent+codeMirror._codeMirrorTextEditor.indent();var isCollapsedBlock=false;if(selection.head.ch===0)
return CodeMirror.Pass;if(line.substr(selection.head.ch-1,2)==='{}'){indentToInsert+='\n'+indent;isCollapsedBlock=true;}else if(line.substr(selection.head.ch-1,1)!=='{'){return CodeMirror.Pass;}
if(i>0&&allSelectionsAreCollapsedBlocks!==isCollapsedBlock)
return CodeMirror.Pass;replacements.push(indentToInsert);allSelectionsAreCollapsedBlocks=isCollapsedBlock;}
codeMirror.replaceSelections(replacements);if(!allSelectionsAreCollapsedBlocks){codeMirror._codeMirrorTextEditor._onAutoAppendedSpaces();return;}
selections=codeMirror.listSelections();var updatedSelections=[];for(var i=0;i<selections.length;++i){var selection=selections[i];var line=codeMirror.getLine(selection.head.line-1);var position=new CodeMirror.Pos(selection.head.line-1,line.length);updatedSelections.push({head:position,anchor:position});}
codeMirror.setSelections(updatedSelections);codeMirror._codeMirrorTextEditor._onAutoAppendedSpaces();},'\'}\'':function(codeMirror){if(codeMirror.somethingSelected())
return CodeMirror.Pass;var selections=codeMirror.listSelections();var replacements=[];for(var i=0;i<selections.length;++i){var selection=selections[i];var line=codeMirror.getLine(selection.head.line);if(line!==TextUtils.TextUtils.lineIndent(line))
return CodeMirror.Pass;replacements.push('}');}
codeMirror.replaceSelections(replacements);selections=codeMirror.listSelections();replacements=[];var updatedSelections=[];for(var i=0;i<selections.length;++i){var selection=selections[i];var matchingBracket=codeMirror.findMatchingBracket(selection.head);if(!matchingBracket||!matchingBracket.match)
return;updatedSelections.push({head:selection.head,anchor:new CodeMirror.Pos(selection.head.line,0)});var line=codeMirror.getLine(matchingBracket.to.line);var indent=TextUtils.TextUtils.lineIndent(line);replacements.push(indent+'}');}
codeMirror.setSelections(updatedSelections);codeMirror.replaceSelections(replacements);}};SourceFrame.SourcesTextEditor.TokenHighlighter=class{constructor(textEditor,codeMirror){this._textEditor=textEditor;this._codeMirror=codeMirror;}
highlightSearchResults(regex,range){var oldRegex=this._highlightRegex;this._highlightRegex=regex;this._highlightRange=range;if(this._searchResultMarker){this._searchResultMarker.clear();delete this._searchResultMarker;}
if(this._highlightDescriptor&&this._highlightDescriptor.selectionStart)
this._codeMirror.removeLineClass(this._highlightDescriptor.selectionStart.line,'wrap','cm-line-with-selection');var selectionStart=this._highlightRange?new CodeMirror.Pos(this._highlightRange.startLine,this._highlightRange.startColumn):null;if(selectionStart)
this._codeMirror.addLineClass(selectionStart.line,'wrap','cm-line-with-selection');if(oldRegex&&this._highlightRegex.toString()===oldRegex.toString()){if(this._highlightDescriptor)
this._highlightDescriptor.selectionStart=selectionStart;}else{this._removeHighlight();this._setHighlighter(this._searchHighlighter.bind(this,this._highlightRegex),selectionStart);}
if(this._highlightRange){var pos=TextEditor.CodeMirrorUtils.toPos(this._highlightRange);this._searchResultMarker=this._codeMirror.markText(pos.start,pos.end,{className:'cm-column-with-selection'});}}
highlightedRegex(){return this._highlightRegex;}
highlightSelectedTokens(){delete this._highlightRegex;delete this._highlightRange;if(this._highlightDescriptor&&this._highlightDescriptor.selectionStart)
this._codeMirror.removeLineClass(this._highlightDescriptor.selectionStart.line,'wrap','cm-line-with-selection');this._removeHighlight();var selectionStart=this._codeMirror.getCursor('start');var selectionEnd=this._codeMirror.getCursor('end');if(selectionStart.line!==selectionEnd.line)
return;if(selectionStart.ch===selectionEnd.ch)
return;var selections=this._codeMirror.getSelections();if(selections.length>1)
return;var selectedText=selections[0];if(this._isWord(selectedText,selectionStart.line,selectionStart.ch,selectionEnd.ch)){if(selectionStart)
this._codeMirror.addLineClass(selectionStart.line,'wrap','cm-line-with-selection');this._setHighlighter(this._tokenHighlighter.bind(this,selectedText,selectionStart),selectionStart);}}
_isWord(selectedText,lineNumber,startColumn,endColumn){var line=this._codeMirror.getLine(lineNumber);var leftBound=startColumn===0||!TextUtils.TextUtils.isWordChar(line.charAt(startColumn-1));var rightBound=endColumn===line.length||!TextUtils.TextUtils.isWordChar(line.charAt(endColumn));return leftBound&&rightBound&&TextUtils.TextUtils.isWord(selectedText);}
_removeHighlight(){if(this._highlightDescriptor){this._codeMirror.removeOverlay(this._highlightDescriptor.overlay);delete this._highlightDescriptor;}}
_searchHighlighter(regex,stream){if(stream.column()===0)
delete this._searchMatchLength;if(this._searchMatchLength){if(this._searchMatchLength>2){for(var i=0;i<this._searchMatchLength-2;++i)
stream.next();this._searchMatchLength=1;return'search-highlight';}else{stream.next();delete this._searchMatchLength;return'search-highlight search-highlight-end';}}
var match=stream.match(regex,false);if(match){stream.next();var matchLength=match[0].length;if(matchLength===1)
return'search-highlight search-highlight-full';this._searchMatchLength=matchLength;return'search-highlight search-highlight-start';}
while(!stream.match(regex,false)&&stream.next()){}}
_tokenHighlighter(token,selectionStart,stream){var tokenFirstChar=token.charAt(0);if(stream.match(token)&&(stream.eol()||!TextUtils.TextUtils.isWordChar(stream.peek())))
return stream.column()===selectionStart.ch?'token-highlight column-with-selection':'token-highlight';var eatenChar;do
eatenChar=stream.next();while(eatenChar&&(TextUtils.TextUtils.isWordChar(eatenChar)||stream.peek()!==tokenFirstChar));}
_setHighlighter(highlighter,selectionStart){var overlayMode={token:highlighter};this._codeMirror.addOverlay(overlayMode);this._highlightDescriptor={overlay:overlayMode,selectionStart:selectionStart};}};SourceFrame.SourcesTextEditor.LinesToScanForIndentationGuessing=1000;SourceFrame.SourcesTextEditor.MaximumNumberOfWhitespacesPerSingleSpan=16;;SourceFrame.FontView=class extends UI.SimpleView{constructor(mimeType,contentProvider){super(Common.UIString('Font'));this.registerRequiredCSS('source_frame/fontView.css');this.element.classList.add('font-view');this._url=contentProvider.contentURL();this._mimeType=mimeType;this._contentProvider=contentProvider;this._mimeTypeLabel=new UI.ToolbarText(mimeType);}
syncToolbarItems(){return[this._mimeTypeLabel];}
_onFontContentLoaded(uniqueFontName,content){var url=content?Common.ContentProvider.contentAsDataURL(content,this._mimeType,true):this._url;this.fontStyleElement.textContent=String.sprintf('@font-face { font-family: "%s"; src: url(%s); }',uniqueFontName,url);}
_createContentIfNeeded(){if(this.fontPreviewElement)
return;var uniqueFontName='WebInspectorFontPreview'+(++SourceFrame.FontView._fontId);this.fontStyleElement=createElement('style');this._contentProvider.requestContent().then(this._onFontContentLoaded.bind(this,uniqueFontName));this.element.appendChild(this.fontStyleElement);var fontPreview=createElement('div');for(var i=0;i<SourceFrame.FontView._fontPreviewLines.length;++i){if(i>0)
fontPreview.createChild('br');fontPreview.createTextChild(SourceFrame.FontView._fontPreviewLines[i]);}
this.fontPreviewElement=fontPreview.cloneNode(true);this.fontPreviewElement.style.overflow='hidden';this.fontPreviewElement.style.setProperty('font-family',uniqueFontName);this.fontPreviewElement.style.setProperty('visibility','hidden');this._dummyElement=fontPreview;this._dummyElement.style.visibility='hidden';this._dummyElement.style.zIndex='-1';this._dummyElement.style.display='inline';this._dummyElement.style.position='absolute';this._dummyElement.style.setProperty('font-family',uniqueFontName);this._dummyElement.style.setProperty('font-size',SourceFrame.FontView._measureFontSize+'px');this.element.appendChild(this.fontPreviewElement);}
wasShown(){this._createContentIfNeeded();this.updateFontPreviewSize();}
onResize(){if(this._inResize)
return;this._inResize=true;try{this.updateFontPreviewSize();}finally{delete this._inResize;}}
_measureElement(){this.element.appendChild(this._dummyElement);var result={width:this._dummyElement.offsetWidth,height:this._dummyElement.offsetHeight};this.element.removeChild(this._dummyElement);return result;}
updateFontPreviewSize(){if(!this.fontPreviewElement||!this.isShowing())
return;this.fontPreviewElement.style.removeProperty('visibility');var dimension=this._measureElement();const height=dimension.height;const width=dimension.width;const containerWidth=this.element.offsetWidth-50;const containerHeight=this.element.offsetHeight-30;if(!height||!width||!containerWidth||!containerHeight){this.fontPreviewElement.style.removeProperty('font-size');return;}
var widthRatio=containerWidth/width;var heightRatio=containerHeight/height;var finalFontSize=Math.floor(SourceFrame.FontView._measureFontSize*Math.min(widthRatio,heightRatio))-2;this.fontPreviewElement.style.setProperty('font-size',finalFontSize+'px',null);}};SourceFrame.FontView._fontPreviewLines=['ABCDEFGHIJKLM','NOPQRSTUVWXYZ','abcdefghijklm','nopqrstuvwxyz','1234567890'];SourceFrame.FontView._fontId=0;SourceFrame.FontView._measureFontSize=50;;SourceFrame.ImageView=class extends UI.SimpleView{constructor(mimeType,contentProvider){super(Common.UIString('Image'));this.registerRequiredCSS('source_frame/imageView.css');this.element.classList.add('image-view');this._url=contentProvider.contentURL();this._parsedURL=new Common.ParsedURL(this._url);this._mimeType=mimeType;this._contentProvider=contentProvider;this._uiSourceCode=contentProvider instanceof Workspace.UISourceCode?(contentProvider):null;if(this._uiSourceCode){this._uiSourceCode.addEventListener(Workspace.UISourceCode.Events.WorkingCopyCommitted,this._workingCopyCommitted,this);new UI.DropTarget(this.element,[UI.DropTarget.Type.ImageFile,UI.DropTarget.Type.URI],Common.UIString('Drop image file here'),this._handleDrop.bind(this));}
this._sizeLabel=new UI.ToolbarText();this._dimensionsLabel=new UI.ToolbarText();this._mimeTypeLabel=new UI.ToolbarText(mimeType);this._container=this.element.createChild('div','image');this._imagePreviewElement=this._container.createChild('img','resource-image-view');this._imagePreviewElement.addEventListener('contextmenu',this._contextMenu.bind(this),true);}
syncToolbarItems(){return[this._sizeLabel,new UI.ToolbarSeparator(),this._dimensionsLabel,new UI.ToolbarSeparator(),this._mimeTypeLabel];}
wasShown(){this._updateContentIfNeeded();}
disposeView(){if(this._uiSourceCode){this._uiSourceCode.removeEventListener(Workspace.UISourceCode.Events.WorkingCopyCommitted,this._workingCopyCommitted,this);}}
_workingCopyCommitted(){this._updateContentIfNeeded();}
async _updateContentIfNeeded(){var content=await this._contentProvider.requestContent();if(this._cachedContent===content)
return;var contentEncoded=await this._contentProvider.contentEncoded();this._cachedContent=content;var imageSrc='data:'+this._mimeType+(contentEncoded?';base64,':',')+content;if(imageSrc===null)
imageSrc=this._url;this._imagePreviewElement.src=imageSrc;this._sizeLabel.setText(Number.bytesToString(this._base64ToSize(content)));this._dimensionsLabel.setText(Common.UIString('%d × %d',this._imagePreviewElement.naturalWidth,this._imagePreviewElement.naturalHeight));}
_base64ToSize(content){if(!content||!content.length)
return 0;var size=(content.length||0)*3/4;if(content.length>0&&content[content.length-1]==='=')
size--;if(content.length>1&&content[content.length-2]==='=')
size--;return size;}
_contextMenu(event){var contextMenu=new UI.ContextMenu(event);if(!this._parsedURL.isDataURL())
contextMenu.clipboardSection().appendItem(Common.UIString('Copy image URL'),this._copyImageURL.bind(this));if(this._imagePreviewElement.src){contextMenu.clipboardSection().appendItem(Common.UIString('Copy image as data URI'),this._copyImageAsDataURL.bind(this));}
contextMenu.clipboardSection().appendItem(Common.UIString('Open image in new tab'),this._openInNewTab.bind(this));contextMenu.clipboardSection().appendItem(Common.UIString('Save\u2026'),this._saveImage.bind(this));contextMenu.show();}
_copyImageAsDataURL(){InspectorFrontendHost.copyText(this._imagePreviewElement.src);}
_copyImageURL(){InspectorFrontendHost.copyText(this._url);}
_saveImage(){var link=createElement('a');link.download=this._parsedURL.displayName;link.href=this._url;link.click();}
_openInNewTab(){InspectorFrontendHost.openInNewTab(this._url);}
async _handleDrop(dataTransfer){var items=dataTransfer.items;if(!items.length||items[0].kind!=='file')
return;var entry=items[0].webkitGetAsEntry();var encoded=!entry.name.endsWith('.svg');entry.file(file=>{var reader=new FileReader();reader.onloadend=()=>{var result;try{result=(reader.result);}catch(e){result=null;console.error('Can\'t read file: '+e);}
if(typeof result!=='string')
return;this._uiSourceCode.setContent(encoded?btoa(result):result,encoded);};if(encoded)
reader.readAsBinaryString(file);else
reader.readAsText(file);});}};;SourceFrame.SourceFrame=class extends UI.SimpleView{constructor(lazyContent){super(Common.UIString('Source'));this._lazyContent=lazyContent;this._textEditor=new SourceFrame.SourcesTextEditor(this);this._textEditor.show(this.element);this._searchConfig=null;this._delayedFindSearchMatches=null;this._currentSearchResultIndex=-1;this._searchResults=[];this._searchRegex=null;this._textEditor.addEventListener(SourceFrame.SourcesTextEditor.Events.EditorFocused,this._resetCurrentSearchResultIndex,this);this._textEditor.addEventListener(SourceFrame.SourcesTextEditor.Events.SelectionChanged,this._updateSourcePosition,this);this._textEditor.addEventListener(UI.TextEditor.Events.TextChanged,event=>{if(!this._muteChangeEventsForSetContent)
this.onTextChanged(event.data.oldRange,event.data.newRange);});this._muteChangeEventsForSetContent=false;this._shortcuts={};this.element.addEventListener('keydown',this._handleKeyDown.bind(this),false);this._sourcePosition=new UI.ToolbarText();this._searchableView=null;this._editable=false;this._textEditor.setReadOnly(true);this._positionToReveal=null;this._lineToScrollTo=null;this._selectionToSet=null;this._loaded=false;this._contentRequested=false;this._highlighterType='';}
setEditable(editable){this._editable=editable;if(this._loaded)
this._textEditor.setReadOnly(!editable);}
addShortcut(key,handler){this._shortcuts[key]=handler;}
wasShown(){this._ensureContentLoaded();this._wasShownOrLoaded();}
willHide(){super.willHide();this._clearPositionToReveal();}
syncToolbarItems(){return[this._sourcePosition];}
get loaded(){return this._loaded;}
get textEditor(){return this._textEditor;}
_ensureContentLoaded(){if(!this._contentRequested){this._contentRequested=true;this._lazyContent().then(this.setContent.bind(this));}}
revealPosition(line,column,shouldHighlight){this._lineToScrollTo=null;this._selectionToSet=null;this._positionToReveal={line:line,column:column,shouldHighlight:shouldHighlight};this._innerRevealPositionIfNeeded();}
_innerRevealPositionIfNeeded(){if(!this._positionToReveal)
return;if(!this.loaded||!this.isShowing())
return;this._textEditor.revealPosition(this._positionToReveal.line,this._positionToReveal.column,this._positionToReveal.shouldHighlight);this._positionToReveal=null;}
_clearPositionToReveal(){this._textEditor.clearPositionHighlight();this._positionToReveal=null;}
scrollToLine(line){this._clearPositionToReveal();this._lineToScrollTo=line;this._innerScrollToLineIfNeeded();}
_innerScrollToLineIfNeeded(){if(this._lineToScrollTo!==null){if(this.loaded&&this.isShowing()){this._textEditor.scrollToLine(this._lineToScrollTo);this._lineToScrollTo=null;}}}
selection(){return this.textEditor.selection();}
setSelection(textRange){this._selectionToSet=textRange;this._innerSetSelectionIfNeeded();}
_innerSetSelectionIfNeeded(){if(this._selectionToSet&&this.loaded&&this.isShowing()){this._textEditor.setSelection(this._selectionToSet);this._selectionToSet=null;}}
_wasShownOrLoaded(){this._innerRevealPositionIfNeeded();this._innerSetSelectionIfNeeded();this._innerScrollToLineIfNeeded();}
onTextChanged(oldRange,newRange){if(this._searchConfig&&this._searchableView)
this.performSearch(this._searchConfig,false,false);}
_simplifyMimeType(content,mimeType){if(!mimeType)
return'';if(mimeType.indexOf('javascript')>=0||mimeType.indexOf('jscript')>=0||mimeType.indexOf('ecmascript')>=0)
return'text/javascript';if(mimeType==='text/x-php'&&content.match(/\<\?.*\?\>/g))
return'application/x-httpd-php';return mimeType;}
setHighlighterType(highlighterType){this._highlighterType=highlighterType;this._updateHighlighterType('');}
_updateHighlighterType(content){this._textEditor.setMimeType(this._simplifyMimeType(content,this._highlighterType));}
setContent(content){this._muteChangeEventsForSetContent=true;if(!this._loaded){this._loaded=true;this._textEditor.setText(content||'');this._textEditor.markClean();this._textEditor.setReadOnly(!this._editable);}else{var scrollTop=this._textEditor.scrollTop();var selection=this._textEditor.selection();this._textEditor.setText(content||'');this._textEditor.setScrollTop(scrollTop);this._textEditor.setSelection(selection);}
this._updateHighlighterType(content||'');this._wasShownOrLoaded();if(this._delayedFindSearchMatches){this._delayedFindSearchMatches();this._delayedFindSearchMatches=null;}
this._muteChangeEventsForSetContent=false;this.onTextEditorContentSet();}
onTextEditorContentSet(){}
setSearchableView(view){this._searchableView=view;}
_doFindSearchMatches(searchConfig,shouldJump,jumpBackwards){this._currentSearchResultIndex=-1;this._searchResults=[];var regex=searchConfig.toSearchRegex();this._searchRegex=regex;this._searchResults=this._collectRegexMatches(regex);if(this._searchableView)
this._searchableView.updateSearchMatchesCount(this._searchResults.length);if(!this._searchResults.length)
this._textEditor.cancelSearchResultsHighlight();else if(shouldJump&&jumpBackwards)
this.jumpToPreviousSearchResult();else if(shouldJump)
this.jumpToNextSearchResult();else
this._textEditor.highlightSearchResults(regex,null);}
performSearch(searchConfig,shouldJump,jumpBackwards){if(this._searchableView)
this._searchableView.updateSearchMatchesCount(0);this._resetSearch();this._searchConfig=searchConfig;if(this.loaded)
this._doFindSearchMatches(searchConfig,shouldJump,!!jumpBackwards);else
this._delayedFindSearchMatches=this._doFindSearchMatches.bind(this,searchConfig,shouldJump,!!jumpBackwards);this._ensureContentLoaded();}
_resetCurrentSearchResultIndex(){if(!this._searchResults.length)
return;this._currentSearchResultIndex=-1;if(this._searchableView)
this._searchableView.updateCurrentMatchIndex(this._currentSearchResultIndex);this._textEditor.highlightSearchResults((this._searchRegex),null);}
_resetSearch(){this._searchConfig=null;this._delayedFindSearchMatches=null;this._currentSearchResultIndex=-1;this._searchResults=[];this._searchRegex=null;}
searchCanceled(){var range=this._currentSearchResultIndex!==-1?this._searchResults[this._currentSearchResultIndex]:null;this._resetSearch();if(!this.loaded)
return;this._textEditor.cancelSearchResultsHighlight();if(range)
this.setSelection(range);}
jumpToLastSearchResult(){this.jumpToSearchResult(this._searchResults.length-1);}
_searchResultIndexForCurrentSelection(){return this._searchResults.lowerBound(this._textEditor.selection().collapseToEnd(),TextUtils.TextRange.comparator);}
jumpToNextSearchResult(){var currentIndex=this._searchResultIndexForCurrentSelection();var nextIndex=this._currentSearchResultIndex===-1?currentIndex:currentIndex+1;this.jumpToSearchResult(nextIndex);}
jumpToPreviousSearchResult(){var currentIndex=this._searchResultIndexForCurrentSelection();this.jumpToSearchResult(currentIndex-1);}
supportsCaseSensitiveSearch(){return true;}
supportsRegexSearch(){return true;}
get currentSearchResultIndex(){return this._currentSearchResultIndex;}
jumpToSearchResult(index){if(!this.loaded||!this._searchResults.length)
return;this._currentSearchResultIndex=(index+this._searchResults.length)%this._searchResults.length;if(this._searchableView)
this._searchableView.updateCurrentMatchIndex(this._currentSearchResultIndex);this._textEditor.highlightSearchResults((this._searchRegex),this._searchResults[this._currentSearchResultIndex]);}
replaceSelectionWith(searchConfig,replacement){var range=this._searchResults[this._currentSearchResultIndex];if(!range)
return;this._textEditor.highlightSearchResults((this._searchRegex),null);var oldText=this._textEditor.text(range);var regex=searchConfig.toSearchRegex();var text;if(regex.__fromRegExpQuery){text=oldText.replace(regex,replacement);}else{text=oldText.replace(regex,function(){return replacement;});}
var newRange=this._textEditor.editRange(range,text);this._textEditor.setSelection(newRange.collapseToEnd());}
replaceAllWith(searchConfig,replacement){this._resetCurrentSearchResultIndex();var text=this._textEditor.text();var range=this._textEditor.fullRange();var regex=searchConfig.toSearchRegex(true);if(regex.__fromRegExpQuery){text=text.replace(regex,replacement);}else{text=text.replace(regex,function(){return replacement;});}
var ranges=this._collectRegexMatches(regex);if(!ranges.length)
return;var currentRangeIndex=ranges.lowerBound(this._textEditor.selection(),TextUtils.TextRange.comparator);var lastRangeIndex=mod(currentRangeIndex-1,ranges.length);var lastRange=ranges[lastRangeIndex];var replacementLineEndings=replacement.computeLineEndings();var replacementLineCount=replacementLineEndings.length;var lastLineNumber=lastRange.startLine+replacementLineEndings.length-1;var lastColumnNumber=lastRange.startColumn;if(replacementLineEndings.length>1){lastColumnNumber=replacementLineEndings[replacementLineCount-1]-replacementLineEndings[replacementLineCount-2]-1;}
this._textEditor.editRange(range,text);this._textEditor.revealPosition(lastLineNumber,lastColumnNumber);this._textEditor.setSelection(TextUtils.TextRange.createFromLocation(lastLineNumber,lastColumnNumber));}
_collectRegexMatches(regexObject){var ranges=[];for(var i=0;i<this._textEditor.linesCount;++i){var line=this._textEditor.line(i);var offset=0;do{var match=regexObject.exec(line);if(match){var matchEndIndex=match.index+Math.max(match[0].length,1);if(match[0].length)
ranges.push(new TextUtils.TextRange(i,offset+match.index,i,offset+matchEndIndex));offset+=matchEndIndex;line=line.substring(matchEndIndex);}}while(match&&line);}
return ranges;}
populateLineGutterContextMenu(contextMenu,lineNumber){return Promise.resolve();}
populateTextAreaContextMenu(contextMenu,lineNumber,columnNumber){return Promise.resolve();}
canEditSource(){return this._editable;}
_updateSourcePosition(){var selections=this._textEditor.selections();if(!selections.length)
return;if(selections.length>1){this._sourcePosition.setText(Common.UIString('%d selection regions',selections.length));return;}
var textRange=selections[0];if(textRange.isEmpty()){this._sourcePosition.setText(Common.UIString('Line %d, Column %d',textRange.endLine+1,textRange.endColumn+1));return;}
textRange=textRange.normalize();var selectedText=this._textEditor.text(textRange);if(textRange.startLine===textRange.endLine){this._sourcePosition.setText(Common.UIString('%d characters selected',selectedText.length));}else{this._sourcePosition.setText(Common.UIString('%d lines, %d characters selected',textRange.endLine-textRange.startLine+1,selectedText.length));}}
_handleKeyDown(e){var shortcutKey=UI.KeyboardShortcut.makeKeyFromEvent(e);var handler=this._shortcuts[shortcutKey];if(handler&&handler())
e.consume(true);}};;SourceFrame.ResourceSourceFrame=class extends SourceFrame.SourceFrame{constructor(resource){super(resource.requestContent.bind(resource));this._resource=resource;}
static createSearchableView(resource,highlighterType){var sourceFrame=new SourceFrame.ResourceSourceFrame(resource);sourceFrame.setHighlighterType(highlighterType);var searchableView=new UI.SearchableView(sourceFrame);searchableView.setPlaceholder(Common.UIString('Find'));sourceFrame.show(searchableView.element);sourceFrame.setSearchableView(searchableView);return searchableView;}
get resource(){return this._resource;}
populateTextAreaContextMenu(contextMenu,lineNumber,columnNumber){contextMenu.appendApplicableItems(this._resource);return Promise.resolve();}};;SourceFrame.UISourceCodeFrame=class extends SourceFrame.SourceFrame{constructor(uiSourceCode){super(workingCopy);this._uiSourceCode=uiSourceCode;this.setEditable(this._canEditSource());if(Runtime.experiments.isEnabled('sourceDiff'))
this._diff=new SourceFrame.SourceCodeDiff(WorkspaceDiff.workspaceDiff(),this.textEditor);this._muteSourceCodeEvents=false;this._isSettingContent=false;this._autocompleteConfig={isWordChar:TextUtils.TextUtils.isWordChar};Common.moduleSetting('textEditorAutocompletion').addChangeListener(this._updateAutocomplete,this);this._updateAutocomplete();this._persistenceBinding=Persistence.persistence.binding(uiSourceCode);this._rowMessageBuckets=new Map();this._typeDecorationsPending=new Set();this._uiSourceCode.addEventListener(Workspace.UISourceCode.Events.WorkingCopyChanged,this._onWorkingCopyChanged,this);this._uiSourceCode.addEventListener(Workspace.UISourceCode.Events.WorkingCopyCommitted,this._onWorkingCopyCommitted,this);this._messageAndDecorationListeners=[];this._installMessageAndDecorationListeners();Persistence.persistence.subscribeForBindingEvent(this._uiSourceCode,this._onBindingChanged.bind(this));this.textEditor.addEventListener(SourceFrame.SourcesTextEditor.Events.EditorBlurred,()=>UI.context.setFlavor(SourceFrame.UISourceCodeFrame,null));this.textEditor.addEventListener(SourceFrame.SourcesTextEditor.Events.EditorFocused,()=>UI.context.setFlavor(SourceFrame.UISourceCodeFrame,this));Common.settings.moduleSetting('persistenceNetworkOverridesEnabled').addChangeListener(this._onNetworkPersistenceChanged,this);this._updateStyle();this._updateDiffUISourceCode();this._errorPopoverHelper=new UI.PopoverHelper(this.element,this._getErrorPopoverContent.bind(this));this._errorPopoverHelper.setHasPadding(true);this._errorPopoverHelper.setTimeout(100,100);function workingCopy(){if(uiSourceCode.isDirty())
return(Promise.resolve(uiSourceCode.workingCopy()));return uiSourceCode.requestContent();}}
_installMessageAndDecorationListeners(){if(this._persistenceBinding){var networkSourceCode=this._persistenceBinding.network;var fileSystemSourceCode=this._persistenceBinding.fileSystem;this._messageAndDecorationListeners=[networkSourceCode.addEventListener(Workspace.UISourceCode.Events.MessageAdded,this._onMessageAdded,this),networkSourceCode.addEventListener(Workspace.UISourceCode.Events.MessageRemoved,this._onMessageRemoved,this),networkSourceCode.addEventListener(Workspace.UISourceCode.Events.LineDecorationAdded,this._onLineDecorationAdded,this),networkSourceCode.addEventListener(Workspace.UISourceCode.Events.LineDecorationRemoved,this._onLineDecorationRemoved,this),fileSystemSourceCode.addEventListener(Workspace.UISourceCode.Events.MessageAdded,this._onMessageAdded,this),fileSystemSourceCode.addEventListener(Workspace.UISourceCode.Events.MessageRemoved,this._onMessageRemoved,this),];}else{this._messageAndDecorationListeners=[this._uiSourceCode.addEventListener(Workspace.UISourceCode.Events.MessageAdded,this._onMessageAdded,this),this._uiSourceCode.addEventListener(Workspace.UISourceCode.Events.MessageRemoved,this._onMessageRemoved,this),this._uiSourceCode.addEventListener(Workspace.UISourceCode.Events.LineDecorationAdded,this._onLineDecorationAdded,this),this._uiSourceCode.addEventListener(Workspace.UISourceCode.Events.LineDecorationRemoved,this._onLineDecorationRemoved,this)];}}
uiSourceCode(){return this._uiSourceCode;}
wasShown(){super.wasShown();setImmediate(this._updateBucketDecorations.bind(this));this.setEditable(this._canEditSource());}
willHide(){super.willHide();UI.context.setFlavor(SourceFrame.UISourceCodeFrame,null);this._uiSourceCode.removeWorkingCopyGetter();}
_canEditSource(){if(Persistence.persistence.binding(this._uiSourceCode))
return true;if(this._uiSourceCode.project().canSetFileContent())
return true;if(this._uiSourceCode.project().isServiceProject())
return false;var networkPersistenceProject=Persistence.networkPersistenceManager.activeProject();if(this._uiSourceCode.project().type()===Workspace.projectTypes.Network&&networkPersistenceProject){var projectDomain=Persistence.networkPersistenceManager.domainForProject(networkPersistenceProject);var urlDomainPath=this._uiSourceCode.url().replace(/^https?:\/\//,'');if(projectDomain&&urlDomainPath.startsWith(projectDomain+'/'))
return true;}
return this._uiSourceCode.contentType()!==Common.resourceTypes.Document;}
_onNetworkPersistenceChanged(){this.setEditable(this._canEditSource());}
commitEditing(){if(!this._uiSourceCode.isDirty())
return;this._muteSourceCodeEvents=true;this._uiSourceCode.commitWorkingCopy();this._muteSourceCodeEvents=false;}
onTextEditorContentSet(){super.onTextEditorContentSet();for(var message of this._allMessages())
this._addMessageToSource(message);this._decorateAllTypes();}
_allMessages(){if(this._persistenceBinding){var combinedSet=this._persistenceBinding.network.messages();combinedSet.addAll(this._persistenceBinding.fileSystem.messages());return combinedSet;}
return this._uiSourceCode.messages();}
onTextChanged(oldRange,newRange){super.onTextChanged(oldRange,newRange);this._errorPopoverHelper.hidePopover();if(this._isSettingContent)
return;this._muteSourceCodeEvents=true;if(this.textEditor.isClean())
this._uiSourceCode.resetWorkingCopy();else
this._uiSourceCode.setWorkingCopyGetter(this.textEditor.text.bind(this.textEditor));this._muteSourceCodeEvents=false;}
_onWorkingCopyChanged(event){if(this._muteSourceCodeEvents)
return;this._innerSetContent(this._uiSourceCode.workingCopy());this.onUISourceCodeContentChanged();}
_onWorkingCopyCommitted(event){if(!this._muteSourceCodeEvents){this._innerSetContent(this._uiSourceCode.workingCopy());this.onUISourceCodeContentChanged();}
this.textEditor.markClean();this._updateStyle();}
_onBindingChanged(){var binding=Persistence.persistence.binding(this._uiSourceCode);if(binding===this._persistenceBinding)
return;for(var message of this._allMessages())
this._removeMessageFromSource(message);Common.EventTarget.removeEventListeners(this._messageAndDecorationListeners);this._persistenceBinding=binding;for(var message of this._allMessages())
this._addMessageToSource(message);this._installMessageAndDecorationListeners();this._updateStyle();this._decorateAllTypes();this._updateDiffUISourceCode();this.onBindingChanged();}
onBindingChanged(){}
_updateDiffUISourceCode(){if(!this._diff)
return;if(this._persistenceBinding)
this._diff.setUISourceCode(this._persistenceBinding.network);else if(this._uiSourceCode.project().type()===Workspace.projectTypes.Network)
this._diff.setUISourceCode(this._uiSourceCode);else
this._diff.setUISourceCode(null);}
_updateStyle(){this.setEditable(this._canEditSource());}
onUISourceCodeContentChanged(){}
_updateAutocomplete(){this.textEditor.configureAutocomplete(Common.moduleSetting('textEditorAutocompletion').get()?this._autocompleteConfig:null);}
configureAutocomplete(config){this._autocompleteConfig=config;this._updateAutocomplete();}
_innerSetContent(content){this._isSettingContent=true;if(this._diff){var oldContent=this.textEditor.text();this.setContent(content);this._diff.highlightModifiedLines(oldContent,content);}else{this.setContent(content);}
this._isSettingContent=false;}
populateTextAreaContextMenu(contextMenu,lineNumber,columnNumber){function appendItems(){contextMenu.appendApplicableItems(this._uiSourceCode);contextMenu.appendApplicableItems(new Workspace.UILocation(this._uiSourceCode,lineNumber,columnNumber));contextMenu.appendApplicableItems(this);}
return super.populateTextAreaContextMenu(contextMenu,lineNumber,columnNumber).then(appendItems.bind(this));}
attachInfobars(infobars){for(var i=infobars.length-1;i>=0;--i){var infobar=infobars[i];if(!infobar)
continue;this.element.insertBefore(infobar.element,this.element.children[0]);infobar.setParentView(this);}
this.doResize();}
dispose(){if(this._diff)
this._diff.dispose();this.textEditor.dispose();Common.moduleSetting('textEditorAutocompletion').removeChangeListener(this._updateAutocomplete,this);this.detach();Common.settings.moduleSetting('persistenceNetworkOverridesEnabled').removeChangeListener(this._onNetworkPersistenceChanged,this);}
_onMessageAdded(event){var message=(event.data);this._addMessageToSource(message);}
_addMessageToSource(message){if(!this.loaded)
return;var lineNumber=message.lineNumber();if(lineNumber>=this.textEditor.linesCount)
lineNumber=this.textEditor.linesCount-1;if(lineNumber<0)
lineNumber=0;var messageBucket=this._rowMessageBuckets.get(lineNumber);if(!messageBucket){messageBucket=new SourceFrame.UISourceCodeFrame.RowMessageBucket(this,this.textEditor,lineNumber);this._rowMessageBuckets.set(lineNumber,messageBucket);}
messageBucket.addMessage(message);}
_onMessageRemoved(event){var message=(event.data);this._removeMessageFromSource(message);}
_removeMessageFromSource(message){if(!this.loaded)
return;var lineNumber=message.lineNumber();if(lineNumber>=this.textEditor.linesCount)
lineNumber=this.textEditor.linesCount-1;if(lineNumber<0)
lineNumber=0;var messageBucket=this._rowMessageBuckets.get(lineNumber);if(!messageBucket)
return;messageBucket.removeMessage(message);if(!messageBucket.uniqueMessagesCount()){messageBucket.detachFromEditor();this._rowMessageBuckets.delete(lineNumber);}}
_getErrorPopoverContent(event){var element=event.target.enclosingNodeOrSelfWithClass('text-editor-line-decoration-icon')||event.target.enclosingNodeOrSelfWithClass('text-editor-line-decoration-wave');if(!element)
return null;var anchor=element.enclosingNodeOrSelfWithClass('text-editor-line-decoration-icon')?element.boxInWindow():new AnchorBox(event.clientX,event.clientY,1,1);return{box:anchor,show:popover=>{var messageBucket=element.enclosingNodeOrSelfWithClass('text-editor-line-decoration')._messageBucket;var messagesOutline=messageBucket.messagesDescription();popover.contentElement.appendChild(messagesOutline);return Promise.resolve(true);}};}
_updateBucketDecorations(){for(var bucket of this._rowMessageBuckets.values())
bucket._updateDecoration();}
_onLineDecorationAdded(event){var marker=(event.data);this._decorateTypeThrottled(marker.type());}
_onLineDecorationRemoved(event){var marker=(event.data);this._decorateTypeThrottled(marker.type());}
_decorateTypeThrottled(type){if(this._typeDecorationsPending.has(type))
return;this._typeDecorationsPending.add(type);self.runtime.extensions(SourceFrame.UISourceCodeFrame.LineDecorator).find(extension=>extension.descriptor()['decoratorType']===type).instance().then(decorator=>{this._typeDecorationsPending.delete(type);this.textEditor.codeMirror().operation(()=>{decorator.decorate(this._persistenceBinding?this._persistenceBinding.network:this.uiSourceCode(),this.textEditor);});});}
_decorateAllTypes(){for(var extension of self.runtime.extensions(SourceFrame.UISourceCodeFrame.LineDecorator)){var type=extension.descriptor()['decoratorType'];if(this._uiSourceCode.decorationsForType(type))
this._decorateTypeThrottled(type);}}};SourceFrame.UISourceCodeFrame._iconClassPerLevel={};SourceFrame.UISourceCodeFrame._iconClassPerLevel[Workspace.UISourceCode.Message.Level.Error]='smallicon-error';SourceFrame.UISourceCodeFrame._iconClassPerLevel[Workspace.UISourceCode.Message.Level.Warning]='smallicon-warning';SourceFrame.UISourceCodeFrame._bubbleTypePerLevel={};SourceFrame.UISourceCodeFrame._bubbleTypePerLevel[Workspace.UISourceCode.Message.Level.Error]='error';SourceFrame.UISourceCodeFrame._bubbleTypePerLevel[Workspace.UISourceCode.Message.Level.Warning]='warning';SourceFrame.UISourceCodeFrame._lineClassPerLevel={};SourceFrame.UISourceCodeFrame._lineClassPerLevel[Workspace.UISourceCode.Message.Level.Error]='text-editor-line-with-error';SourceFrame.UISourceCodeFrame._lineClassPerLevel[Workspace.UISourceCode.Message.Level.Warning]='text-editor-line-with-warning';SourceFrame.UISourceCodeFrame.LineDecorator=function(){};SourceFrame.UISourceCodeFrame.LineDecorator.prototype={decorate(uiSourceCode,textEditor){}};SourceFrame.UISourceCodeFrame.RowMessage=class{constructor(message){this._message=message;this._repeatCount=1;this.element=createElementWithClass('div','text-editor-row-message');this._icon=this.element.createChild('label','','dt-icon-label');this._icon.type=SourceFrame.UISourceCodeFrame._iconClassPerLevel[message.level()];this._repeatCountElement=this.element.createChild('label','text-editor-row-message-repeat-count hidden','dt-small-bubble');this._repeatCountElement.type=SourceFrame.UISourceCodeFrame._bubbleTypePerLevel[message.level()];var linesContainer=this.element.createChild('div');var lines=this._message.text().split('\n');for(var i=0;i<lines.length;++i){var messageLine=linesContainer.createChild('div');messageLine.textContent=lines[i];}}
message(){return this._message;}
repeatCount(){return this._repeatCount;}
setRepeatCount(repeatCount){if(this._repeatCount===repeatCount)
return;this._repeatCount=repeatCount;this._updateMessageRepeatCount();}
_updateMessageRepeatCount(){this._repeatCountElement.textContent=this._repeatCount;var showRepeatCount=this._repeatCount>1;this._repeatCountElement.classList.toggle('hidden',!showRepeatCount);this._icon.classList.toggle('hidden',showRepeatCount);}};SourceFrame.UISourceCodeFrame.RowMessageBucket=class{constructor(sourceFrame,textEditor,lineNumber){this._sourceFrame=sourceFrame;this.textEditor=textEditor;this._lineHandle=textEditor.textEditorPositionHandle(lineNumber,0);this._decoration=createElementWithClass('div','text-editor-line-decoration');this._decoration._messageBucket=this;this._wave=this._decoration.createChild('div','text-editor-line-decoration-wave');this._icon=this._wave.createChild('label','text-editor-line-decoration-icon','dt-icon-label');this._decorationStartColumn=null;this._messagesDescriptionElement=createElementWithClass('div','text-editor-messages-description-container');this._messages=[];this._level=null;}
_updateWavePosition(lineNumber,columnNumber){lineNumber=Math.min(lineNumber,this.textEditor.linesCount-1);var lineText=this.textEditor.line(lineNumber);columnNumber=Math.min(columnNumber,lineText.length);var lineIndent=TextUtils.TextUtils.lineIndent(lineText).length;var startColumn=Math.max(columnNumber-1,lineIndent);if(this._decorationStartColumn===startColumn)
return;if(this._decorationStartColumn!==null)
this.textEditor.removeDecoration(this._decoration,lineNumber);this.textEditor.addDecoration(this._decoration,lineNumber,startColumn);this._decorationStartColumn=startColumn;}
messagesDescription(){this._messagesDescriptionElement.removeChildren();UI.appendStyle(this._messagesDescriptionElement,'source_frame/messagesPopover.css');for(var i=0;i<this._messages.length;++i)
this._messagesDescriptionElement.appendChild(this._messages[i].element);return this._messagesDescriptionElement;}
detachFromEditor(){var position=this._lineHandle.resolve();if(!position)
return;var lineNumber=position.lineNumber;if(this._level)
this.textEditor.toggleLineClass(lineNumber,SourceFrame.UISourceCodeFrame._lineClassPerLevel[this._level],false);if(this._decorationStartColumn!==null){this.textEditor.removeDecoration(this._decoration,lineNumber);this._decorationStartColumn=null;}}
uniqueMessagesCount(){return this._messages.length;}
addMessage(message){for(var i=0;i<this._messages.length;++i){var rowMessage=this._messages[i];if(rowMessage.message().isEqual(message)){rowMessage.setRepeatCount(rowMessage.repeatCount()+1);return;}}
var rowMessage=new SourceFrame.UISourceCodeFrame.RowMessage(message);this._messages.push(rowMessage);this._updateDecoration();}
removeMessage(message){for(var i=0;i<this._messages.length;++i){var rowMessage=this._messages[i];if(!rowMessage.message().isEqual(message))
continue;rowMessage.setRepeatCount(rowMessage.repeatCount()-1);if(!rowMessage.repeatCount())
this._messages.splice(i,1);this._updateDecoration();return;}}
_updateDecoration(){if(!this._sourceFrame.isShowing())
return;if(!this._messages.length)
return;var position=this._lineHandle.resolve();if(!position)
return;var lineNumber=position.lineNumber;var columnNumber=Number.MAX_VALUE;var maxMessage=null;for(var i=0;i<this._messages.length;++i){var message=this._messages[i].message();columnNumber=Math.min(columnNumber,message.columnNumber());if(!maxMessage||Workspace.UISourceCode.Message.messageLevelComparator(maxMessage,message)<0)
maxMessage=message;}
this._updateWavePosition(lineNumber,columnNumber);if(this._level===maxMessage.level())
return;if(this._level){this.textEditor.toggleLineClass(lineNumber,SourceFrame.UISourceCodeFrame._lineClassPerLevel[this._level],false);this._icon.type='';}
this._level=maxMessage.level();if(!this._level)
return;this.textEditor.toggleLineClass(lineNumber,SourceFrame.UISourceCodeFrame._lineClassPerLevel[this._level],true);this._icon.type=SourceFrame.UISourceCodeFrame._iconClassPerLevel[this._level];}};Workspace.UISourceCode.Message._messageLevelPriority={'Warning':3,'Error':4};Workspace.UISourceCode.Message.messageLevelComparator=function(a,b){return Workspace.UISourceCode.Message._messageLevelPriority[a.level()]-
Workspace.UISourceCode.Message._messageLevelPriority[b.level()];};;SourceFrame.JSONView=class extends UI.VBox{constructor(parsedJSON){super();this.registerRequiredCSS('source_frame/jsonView.css');this._parsedJSON=parsedJSON;this.element.classList.add('json-view');this._searchableView;this._treeOutline;this._currentSearchFocusIndex=0;this._currentSearchTreeElements=[];this._searchRegex=null;}
static createSearchableView(parsedJSON){var jsonView=new SourceFrame.JSONView(parsedJSON);var searchableView=new UI.SearchableView(jsonView);searchableView.setPlaceholder(Common.UIString('Find'));jsonView._searchableView=searchableView;jsonView.show(searchableView.element);jsonView.element.setAttribute('tabIndex',0);return searchableView;}
static parseJSON(text){var returnObj=null;if(text)
returnObj=SourceFrame.JSONView._extractJSON((text));if(!returnObj)
return Promise.resolve((null));return Formatter.formatterWorkerPool().parseJSONRelaxed(returnObj.data).then(handleReturnedJSON);function handleReturnedJSON(data){if(!data)
return null;returnObj.data=data;return returnObj;}}
static _extractJSON(text){if(text.startsWith('<'))
return null;var inner=SourceFrame.JSONView._findBrackets(text,'{','}');var inner2=SourceFrame.JSONView._findBrackets(text,'[',']');inner=inner2.length>inner.length?inner2:inner;if(inner.length===-1||text.length-inner.length>80)
return null;var prefix=text.substring(0,inner.start);var suffix=text.substring(inner.end+1);text=text.substring(inner.start,inner.end+1);if(suffix.trim().length&&!(suffix.trim().startsWith(')')&&prefix.trim().endsWith('(')))
return null;return new SourceFrame.ParsedJSON(text,prefix,suffix);}
static _findBrackets(text,open,close){var start=text.indexOf(open);var end=text.lastIndexOf(close);var length=end-start-1;if(start===-1||end===-1||end<start)
length=-1;return{start:start,end:end,length:length};}
wasShown(){this._initialize();}
_initialize(){if(this._initialized)
return;this._initialized=true;var obj=SDK.RemoteObject.fromLocalObject(this._parsedJSON.data);var title=this._parsedJSON.prefix+obj.description+this._parsedJSON.suffix;this._treeOutline=new ObjectUI.ObjectPropertiesSection(obj,title);this._treeOutline.setEditable(false);this._treeOutline.expand();this.element.appendChild(this._treeOutline.element);}
_jumpToMatch(index){if(!this._searchRegex)
return;var previousFocusElement=this._currentSearchTreeElements[this._currentSearchFocusIndex];if(previousFocusElement)
previousFocusElement.setSearchRegex(this._searchRegex);var newFocusElement=this._currentSearchTreeElements[index];if(newFocusElement){this._updateSearchIndex(index);newFocusElement.setSearchRegex(this._searchRegex,UI.highlightedCurrentSearchResultClassName);newFocusElement.reveal();}else{this._updateSearchIndex(0);}}
_updateSearchCount(count){if(!this._searchableView)
return;this._searchableView.updateSearchMatchesCount(count);}
_updateSearchIndex(index){this._currentSearchFocusIndex=index;if(!this._searchableView)
return;this._searchableView.updateCurrentMatchIndex(index);}
searchCanceled(){this._searchRegex=null;this._currentSearchTreeElements=[];for(var element=this._treeOutline.rootElement();element;element=element.traverseNextTreeElement(false)){if(!(element instanceof ObjectUI.ObjectPropertyTreeElement))
continue;element.revertHighlightChanges();}
this._updateSearchCount(0);this._updateSearchIndex(0);}
performSearch(searchConfig,shouldJump,jumpBackwards){var newIndex=this._currentSearchFocusIndex;var previousSearchFocusElement=this._currentSearchTreeElements[newIndex];this.searchCanceled();this._searchRegex=searchConfig.toSearchRegex(true);for(var element=this._treeOutline.rootElement();element;element=element.traverseNextTreeElement(false)){if(!(element instanceof ObjectUI.ObjectPropertyTreeElement))
continue;var hasMatch=element.setSearchRegex(this._searchRegex);if(hasMatch)
this._currentSearchTreeElements.push(element);if(previousSearchFocusElement===element){var currentIndex=this._currentSearchTreeElements.length-1;if(hasMatch||jumpBackwards)
newIndex=currentIndex;else
newIndex=currentIndex+1;}}
this._updateSearchCount(this._currentSearchTreeElements.length);if(!this._currentSearchTreeElements.length){this._updateSearchIndex(0);return;}
newIndex=mod(newIndex,this._currentSearchTreeElements.length);this._jumpToMatch(newIndex);}
jumpToNextSearchResult(){if(!this._currentSearchTreeElements.length)
return;var newIndex=mod(this._currentSearchFocusIndex+1,this._currentSearchTreeElements.length);this._jumpToMatch(newIndex);}
jumpToPreviousSearchResult(){if(!this._currentSearchTreeElements.length)
return;var newIndex=mod(this._currentSearchFocusIndex-1,this._currentSearchTreeElements.length);this._jumpToMatch(newIndex);}
supportsCaseSensitiveSearch(){return true;}
supportsRegexSearch(){return true;}};SourceFrame.ParsedJSON=class{constructor(data,prefix,suffix){this.data=data;this.prefix=prefix;this.suffix=suffix;}};;SourceFrame.XMLView=class extends UI.Widget{constructor(parsedXML){super(true);this.registerRequiredCSS('source_frame/xmlView.css');this.contentElement.classList.add('shadow-xml-view','source-code');this._treeOutline=new UI.TreeOutlineInShadow();this._treeOutline.registerRequiredCSS('source_frame/xmlTree.css');this.contentElement.appendChild(this._treeOutline.element);this._searchableView;this._currentSearchFocusIndex=0;this._currentSearchTreeElements=[];this._searchConfig;SourceFrame.XMLView.Node.populate(this._treeOutline,parsedXML,this);}
static createSearchableView(parsedXML){var xmlView=new SourceFrame.XMLView(parsedXML);var searchableView=new UI.SearchableView(xmlView);searchableView.setPlaceholder(Common.UIString('Find'));xmlView._searchableView=searchableView;xmlView.show(searchableView.element);xmlView.contentElement.setAttribute('tabIndex',0);return searchableView;}
static parseXML(text,mimeType){var parsedXML;try{parsedXML=(new DOMParser()).parseFromString(text,mimeType);}catch(e){return null;}
if(parsedXML.body)
return null;return parsedXML;}
_jumpToMatch(index,shouldJump){if(!this._searchConfig)
return;var regex=this._searchConfig.toSearchRegex(true);var previousFocusElement=this._currentSearchTreeElements[this._currentSearchFocusIndex];if(previousFocusElement)
previousFocusElement.setSearchRegex(regex);var newFocusElement=this._currentSearchTreeElements[index];if(newFocusElement){this._updateSearchIndex(index);if(shouldJump)
newFocusElement.reveal(true);newFocusElement.setSearchRegex(regex,UI.highlightedCurrentSearchResultClassName);}else{this._updateSearchIndex(0);}}
_updateSearchCount(count){if(!this._searchableView)
return;this._searchableView.updateSearchMatchesCount(count);}
_updateSearchIndex(index){this._currentSearchFocusIndex=index;if(!this._searchableView)
return;this._searchableView.updateCurrentMatchIndex(index);}
_innerPerformSearch(shouldJump,jumpBackwards){if(!this._searchConfig)
return;var newIndex=this._currentSearchFocusIndex;var previousSearchFocusElement=this._currentSearchTreeElements[newIndex];this._innerSearchCanceled();this._currentSearchTreeElements=[];var regex=this._searchConfig.toSearchRegex(true);for(var element=this._treeOutline.rootElement();element;element=element.traverseNextTreeElement(false)){if(!(element instanceof SourceFrame.XMLView.Node))
continue;var hasMatch=element.setSearchRegex(regex);if(hasMatch)
this._currentSearchTreeElements.push(element);if(previousSearchFocusElement===element){var currentIndex=this._currentSearchTreeElements.length-1;if(hasMatch||jumpBackwards)
newIndex=currentIndex;else
newIndex=currentIndex+1;}}
this._updateSearchCount(this._currentSearchTreeElements.length);if(!this._currentSearchTreeElements.length){this._updateSearchIndex(0);return;}
newIndex=mod(newIndex,this._currentSearchTreeElements.length);this._jumpToMatch(newIndex,shouldJump);}
_innerSearchCanceled(){for(var element=this._treeOutline.rootElement();element;element=element.traverseNextTreeElement(false)){if(!(element instanceof SourceFrame.XMLView.Node))
continue;element.revertHighlightChanges();}
this._updateSearchCount(0);this._updateSearchIndex(0);}
searchCanceled(){this._searchConfig=null;this._currentSearchTreeElements=[];this._innerSearchCanceled();}
performSearch(searchConfig,shouldJump,jumpBackwards){this._searchConfig=searchConfig;this._innerPerformSearch(shouldJump,jumpBackwards);}
jumpToNextSearchResult(){if(!this._currentSearchTreeElements.length)
return;var newIndex=mod(this._currentSearchFocusIndex+1,this._currentSearchTreeElements.length);this._jumpToMatch(newIndex,true);}
jumpToPreviousSearchResult(){if(!this._currentSearchTreeElements.length)
return;var newIndex=mod(this._currentSearchFocusIndex-1,this._currentSearchTreeElements.length);this._jumpToMatch(newIndex,true);}
supportsCaseSensitiveSearch(){return true;}
supportsRegexSearch(){return true;}};SourceFrame.XMLView.Node=class extends UI.TreeElement{constructor(node,closeTag,xmlView){super('',!closeTag&&!!node.childElementCount);this._node=node;this._closeTag=closeTag;this.selectable=false;this._highlightChanges=[];this._xmlView=xmlView;this._updateTitle();}
static populate(root,xmlNode,xmlView){var node=xmlNode.firstChild;while(node){var currentNode=node;node=node.nextSibling;var nodeType=currentNode.nodeType;if(nodeType===3&&currentNode.nodeValue.match(/\s+/))
continue;if((nodeType!==1)&&(nodeType!==3)&&(nodeType!==4)&&(nodeType!==7)&&(nodeType!==8))
continue;root.appendChild(new SourceFrame.XMLView.Node(currentNode,false,xmlView));}}
setSearchRegex(regex,additionalCssClassName){this.revertHighlightChanges();if(!regex)
return false;if(this._closeTag&&this.parent&&!this.parent.expanded)
return false;regex.lastIndex=0;var cssClasses=UI.highlightedSearchResultClassName;if(additionalCssClassName)
cssClasses+=' '+additionalCssClassName;var content=this.listItemElement.textContent.replace(/\xA0/g,' ');var match=regex.exec(content);var ranges=[];while(match){ranges.push(new TextUtils.SourceRange(match.index,match[0].length));match=regex.exec(content);}
if(ranges.length)
UI.highlightRangesWithStyleClass(this.listItemElement,ranges,cssClasses,this._highlightChanges);return!!this._highlightChanges.length;}
revertHighlightChanges(){UI.revertDomChanges(this._highlightChanges);this._highlightChanges=[];}
_updateTitle(){var node=this._node;switch(node.nodeType){case 1:var tag=node.tagName;if(this._closeTag){this._setTitle(['</'+tag+'>','shadow-xml-view-tag']);return;}
var titleItems=['<'+tag,'shadow-xml-view-tag'];var attributes=node.attributes;for(var i=0;i<attributes.length;++i){var attributeNode=attributes.item(i);titleItems.push('\u00a0','shadow-xml-view-tag',attributeNode.name,'shadow-xml-view-attribute-name','="','shadow-xml-view-tag',attributeNode.value,'shadow-xml-view-attribute-value','"','shadow-xml-view-tag');}
if(!this.expanded){if(node.childElementCount){titleItems.push('>','shadow-xml-view-tag','\u2026','shadow-xml-view-comment','</'+tag,'shadow-xml-view-tag');}else if(this._node.textContent){titleItems.push('>','shadow-xml-view-tag',node.textContent,'shadow-xml-view-text','</'+tag,'shadow-xml-view-tag');}else{titleItems.push(' /','shadow-xml-view-tag');}}
titleItems.push('>','shadow-xml-view-tag');this._setTitle(titleItems);return;case 3:this._setTitle([node.nodeValue,'shadow-xml-view-text']);return;case 4:this._setTitle(['<![CDATA[','shadow-xml-view-cdata',node.nodeValue,'shadow-xml-view-text',']]>','shadow-xml-view-cdata']);return;case 7:this._setTitle(['<?'+node.nodeName+' '+node.nodeValue+'?>','shadow-xml-view-processing-instruction']);return;case 8:this._setTitle(['<!--'+node.nodeValue+'-->','shadow-xml-view-comment']);return;}}
_setTitle(items){var titleFragment=createDocumentFragment();for(var i=0;i<items.length;i+=2)
titleFragment.createChild('span',items[i+1]).textContent=items[i];this.title=titleFragment;this._xmlView._innerPerformSearch(false,false);}
onattach(){this.listItemElement.classList.toggle('shadow-xml-view-close-tag',this._closeTag);}
onexpand(){this._updateTitle();}
oncollapse(){this._updateTitle();}
onpopulate(){SourceFrame.XMLView.Node.populate(this,this._node,this._xmlView);this.appendChild(new SourceFrame.XMLView.Node(this._node,true,this._xmlView));}};;SourceFrame.PreviewFactory=class{static async createPreview(provider,mimeType){var content=await provider.requestContent();if(!content)
return new UI.EmptyWidget(Common.UIString('Nothing to preview'));var parsedXML=SourceFrame.XMLView.parseXML(content,mimeType);if(parsedXML)
return SourceFrame.XMLView.createSearchableView(parsedXML);var parsedJSON=await SourceFrame.JSONView.parseJSON(content);if(parsedJSON&&typeof parsedJSON.data==='object')
return SourceFrame.JSONView.createSearchableView((parsedJSON));var resourceType=provider.contentType()||Common.resourceTypes.Other;if(resourceType.isTextType()){var highlighterType=mimeType.replace(/;.*/,'');return SourceFrame.ResourceSourceFrame.createSearchableView(provider,highlighterType);}
switch(resourceType){case Common.resourceTypes.Image:return new SourceFrame.ImageView(mimeType,provider);case Common.resourceTypes.Font:return new SourceFrame.FontView(mimeType,provider);}
return null;}};;SourceFrame.SourceCodeDiff=class{constructor(workspaceDiff,textEditor){this._textEditor=textEditor;this._decorations=[];this._textEditor.installGutter(SourceFrame.SourceCodeDiff.DiffGutterType,true);this._uiSourceCode=null;this._workspaceDiff=workspaceDiff;this._animatedLines=[];this._update();}
setUISourceCode(uiSourceCode){if(uiSourceCode===this._uiSourceCode)
return;if(this._uiSourceCode)
this._workspaceDiff.unsubscribeFromDiffChange(this._uiSourceCode,this._update,this);if(uiSourceCode)
this._workspaceDiff.subscribeToDiffChange(uiSourceCode,this._update,this);this._uiSourceCode=uiSourceCode;this._update();}
highlightModifiedLines(oldContent,newContent){if(typeof oldContent!=='string'||typeof newContent!=='string')
return;var diff=this._computeDiff(Diff.Diff.lineDiff(oldContent.split('\n'),newContent.split('\n')));var changedLines=[];for(var i=0;i<diff.length;++i){var diffEntry=diff[i];if(diffEntry.type===SourceFrame.SourceCodeDiff.GutterDecorationType.Delete)
continue;for(var lineNumber=diffEntry.from;lineNumber<diffEntry.to;++lineNumber){var position=this._textEditor.textEditorPositionHandle(lineNumber,0);if(position)
changedLines.push(position);}}
this._updateHighlightedLines(changedLines);this._animationTimeout=setTimeout(this._updateHighlightedLines.bind(this,[]),400);}
_updateHighlightedLines(newLines){if(this._animationTimeout)
clearTimeout(this._animationTimeout);this._animationTimeout=null;this._textEditor.operation(operation.bind(this));function operation(){toggleLines.call(this,false);this._animatedLines=newLines;toggleLines.call(this,true);}
function toggleLines(value){for(var i=0;i<this._animatedLines.length;++i){var location=this._animatedLines[i].resolve();if(location)
this._textEditor.toggleLineClass(location.lineNumber,'highlight-line-modification',value);}}}
_updateDecorations(removed,added){this._textEditor.operation(operation);function operation(){for(var decoration of removed)
decoration.remove();for(var decoration of added)
decoration.install();}}
_computeDiff(diff){var result=[];var hasAdded=false;var hasRemoved=false;var blockStartLineNumber=0;var currentLineNumber=0;var isInsideBlock=false;for(var i=0;i<diff.length;++i){var token=diff[i];if(token[0]===Diff.Diff.Operation.Equal){if(isInsideBlock)
flush();currentLineNumber+=token[1].length;continue;}
if(!isInsideBlock){isInsideBlock=true;blockStartLineNumber=currentLineNumber;}
if(token[0]===Diff.Diff.Operation.Delete){hasRemoved=true;}else{currentLineNumber+=token[1].length;hasAdded=true;}}
if(isInsideBlock)
flush();if(result.length>1&&result[0].from===0&&result[1].from===0){var merged={type:SourceFrame.SourceCodeDiff.GutterDecorationType.Modify,from:0,to:result[1].to};result.splice(0,2,merged);}
return result;function flush(){var type=SourceFrame.SourceCodeDiff.GutterDecorationType.Insert;var from=blockStartLineNumber;var to=currentLineNumber;if(hasAdded&&hasRemoved){type=SourceFrame.SourceCodeDiff.GutterDecorationType.Modify;}else if(!hasAdded&&hasRemoved&&from===0&&to===0){type=SourceFrame.SourceCodeDiff.GutterDecorationType.Modify;to=1;}else if(!hasAdded&&hasRemoved){type=SourceFrame.SourceCodeDiff.GutterDecorationType.Delete;from-=1;}
result.push({type:type,from:from,to:to});isInsideBlock=false;hasAdded=false;hasRemoved=false;}}
_update(){if(this._uiSourceCode)
this._workspaceDiff.requestDiff(this._uiSourceCode).then(this._innerUpdate.bind(this));else
this._innerUpdate(null);}
_innerUpdate(lineDiff){if(!lineDiff){this._updateDecorations(this._decorations,[]);this._decorations=[];return;}
var oldDecorations=new Map();for(var i=0;i<this._decorations.length;++i){var decoration=this._decorations[i];var lineNumber=decoration.lineNumber();if(lineNumber===-1)
continue;oldDecorations.set(lineNumber,decoration);}
var diff=this._computeDiff(lineDiff);var newDecorations=new Map();for(var i=0;i<diff.length;++i){var diffEntry=diff[i];for(var lineNumber=diffEntry.from;lineNumber<diffEntry.to;++lineNumber)
newDecorations.set(lineNumber,{lineNumber:lineNumber,type:diffEntry.type});}
var decorationDiff=oldDecorations.diff(newDecorations,(e1,e2)=>e1.type===e2.type);var addedDecorations=decorationDiff.added.map(entry=>new SourceFrame.SourceCodeDiff.GutterDecoration(this._textEditor,entry.lineNumber,entry.type));this._decorations=decorationDiff.equal.concat(addedDecorations);this._updateDecorations(decorationDiff.removed,addedDecorations);this._decorationsSetForTest(newDecorations);}
_decorationsSetForTest(decorations){}
dispose(){if(this._uiSourceCode)
WorkspaceDiff.workspaceDiff().unsubscribeFromDiffChange(this._uiSourceCode,this._update,this);}};SourceFrame.SourceCodeDiff.DiffGutterType='CodeMirror-gutter-diff';SourceFrame.SourceCodeDiff.GutterDecorationType={Insert:Symbol('Insert'),Delete:Symbol('Delete'),Modify:Symbol('Modify'),};SourceFrame.SourceCodeDiff.GutterDecoration=class{constructor(textEditor,lineNumber,type){this._textEditor=textEditor;this._position=this._textEditor.textEditorPositionHandle(lineNumber,0);this._className='';if(type===SourceFrame.SourceCodeDiff.GutterDecorationType.Insert)
this._className='diff-entry-insert';else if(type===SourceFrame.SourceCodeDiff.GutterDecorationType.Delete)
this._className='diff-entry-delete';else if(type===SourceFrame.SourceCodeDiff.GutterDecorationType.Modify)
this._className='diff-entry-modify';this.type=type;}
lineNumber(){var location=this._position.resolve();if(!location)
return-1;return location.lineNumber;}
install(){var location=this._position.resolve();if(!location)
return;var element=createElementWithClass('div','diff-marker');element.textContent='\u00A0';this._textEditor.setGutterDecoration(location.lineNumber,SourceFrame.SourceCodeDiff.DiffGutterType,element);this._textEditor.toggleLineClass(location.lineNumber,this._className,true);}
remove(){var location=this._position.resolve();if(!location)
return;this._textEditor.setGutterDecoration(location.lineNumber,SourceFrame.SourceCodeDiff.DiffGutterType,null);this._textEditor.toggleLineClass(location.lineNumber,this._className,false);}};;Runtime.cachedResources["source_frame/fontView.css"]="/*\n * Copyright (c) 2014 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n.font-view {\n    font-size: 60px;\n    white-space: pre-wrap;\n    word-wrap: break-word;\n    text-align: center;\n    padding: 15px;\n}\n\n/*# sourceURL=source_frame/fontView.css */";Runtime.cachedResources["source_frame/imageView.css"]="/*\n * Copyright (c) 2014 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n.image-view {\n    overflow: auto;\n}\n\n.image-view > .image {\n    padding: 20px 20px 10px 20px;\n    text-align: center;\n}\n\n.image-view img.resource-image-view {\n    max-width: 100%;\n    max-height: 1000px;\n    background-image: url(Images/checker.png);\n    box-shadow: 0 5px 10px rgba(0, 0, 0, 0.5);\n    -webkit-user-select: text;\n    -webkit-user-drag: auto;\n}\n\n/*# sourceURL=source_frame/imageView.css */";Runtime.cachedResources["source_frame/jsonView.css"]=".json-view {\n    padding: 2px 6px;\n    overflow: auto;\n}\n\n/*# sourceURL=source_frame/jsonView.css */";Runtime.cachedResources["source_frame/messagesPopover.css"]="/*\n * Copyright 2017 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n.text-editor-messages-description-container {\n    display: inline-block;\n}\n\n.text-editor-row-message:first-child {\n    border-top-width: 0;\n}\n\n.text-editor-row-message {\n    line-height: 1.2;\n    white-space: nowrap;\n    display: flex;\n    align-items: center;\n    justify-content: flex-start;\n    margin-top: 2px;\n}\n\n.text-editor-row-message-repeat-count {\n    margin-right: 0.5em;\n}\n\n/*# sourceURL=source_frame/messagesPopover.css */";Runtime.cachedResources["source_frame/xmlTree.css"]="/*\n * Copyright 2016 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n.tree-outline ol {\n    list-style: none;\n    padding: 0;\n    margin: 0;\n    -webkit-padding-start: 16px;\n}\n\nol.tree-outline {\n    -webkit-padding-start: 0;\n}\n\n.tree-outline li {\n    min-height: 12px;\n}\n\n.tree-outline li.shadow-xml-view-close-tag {\n    margin-left: -16px;\n}\n\n.shadow-xml-view-tag {\n    color: rgb(136, 18, 128);\n}\n\n.shadow-xml-view-comment {\n    color: rgb(35, 110, 37);\n}\n\n.shadow-xml-view-processing-instruction {\n    color: rgb(35, 110, 37);\n}\n\n.shadow-xml-view-attribute-name {\n    color: rgb(153, 69, 0);\n}\n\n.shadow-xml-view-attribute-value {\n    color: rgb(26, 26, 166);\n}\n\n.shadow-xml-view-text {\n    color: rgb(0, 0, 0);\n    white-space: pre;\n}\n\n.shadow-xml-view-cdata {\n    color: rgb(0, 0, 0);\n}\n\n/*# sourceURL=source_frame/xmlTree.css */";Runtime.cachedResources["source_frame/xmlView.css"]="/*\n * Copyright (c) 2014 The Chromium Authors. All rights reserved.\n * Use of this source code is governed by a BSD-style license that can be\n * found in the LICENSE file.\n */\n\n.shadow-xml-view {\n    -webkit-user-select: text;\n    overflow: auto;\n    padding: 2px 4px;\n}\n\n/*# sourceURL=source_frame/xmlView.css */";