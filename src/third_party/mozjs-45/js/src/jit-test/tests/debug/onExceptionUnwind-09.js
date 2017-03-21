// Ensure that ScriptDebugEpilogue gets called when onExceptionUnwind
// terminates execution.
var g = newGlobal();
var dbg = Debugger(g);
var frame;
dbg.onExceptionUnwind = function (f, x) {
    frame = f;
    assertEq(frame.live, true);
    return null;
};
dbg.onDebuggerStatement = function(f) {
    assertEq(f.eval('throw 42'), null);
    assertEq(frame.live, false);
};
g.eval('debugger');
