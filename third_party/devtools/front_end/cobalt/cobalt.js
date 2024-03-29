
/**
 * @implements {SDK.TargetManager.Observer}
 * @unrestricted
 */
export default class CobaltPanel extends UI.VBox {
    constructor() {
        const trace_files = [
            ['Trace', 'console_trace.json'],
            ['Timed Trace', 'timed_trace.json']
        ];
        const timed_trace_durations = ['5', '10', '20', '60'];
        super(true, false);
        SDK.targetManager.observeTargets(this);

        this._target = UI.context.flavor(SDK.Target);
        this._runtimeAgent = this._target.runtimeAgent();
        this._cobaltAgent = this._target.cobaltAgent();

        this.element = this._shadowRoot.createChild('div');
        this.element.textContent = 'Cobalt Console';
        let download_element = this._shadowRoot.createChild('a', 'download');
        download_element.style.display = 'none';

        const traceContainer = this.element.createChild('div', 'trace-container');
        traceContainer.appendChild(UI.createTextButton(Common.UIString('Start Trace'), event => {
            console.log("Start Trace");
            const filename = trace_files[0][1];
            this.run(`(function() { window.h5vcc.traceEvent.start('${filename}');})()`);
            console.log("Started Trace");
        }));
        traceContainer.appendChild(UI.createTextButton(Common.UIString('Stop Trace'), event => {
            console.log("Stop Trace");
            this.run(`(function() { window.h5vcc.traceEvent.stop();})()`);
            console.log("Stopped Trace");
        }));
        traceContainer.appendChild(UI.createLabel('Navigate Timed Trace:'));
        timed_trace_durations.forEach((duration) => {
            traceContainer.appendChild(UI.createTextButton(Common.UIString(duration + 's'), event => {
                console.log("Request Navigate Timed Trace. " + duration);
                this._cobaltAgent.invoke_sendConsoleCommand({
                    command: 'navigate_timed_trace', message: duration
                });
                console.log("Requested Navigate Timed Trace.");
            }));
        });
        trace_files.forEach((file) => {
            traceContainer.appendChild(UI.createTextButton(Common.UIString('Download ' + file[0]), event => {
                console.log("Download Trace");
                const filename = file[1];
                this.run(`(function() { return window.h5vcc.traceEvent.read('${filename}');})()`).then(function (result) {
                    download_element.setAttribute('href', 'data:text/plain;charset=utf-8,' +
                        encodeURIComponent(result.result.value));
                    download_element.setAttribute('download', filename);
                    console.log("Downloaded Trace");
                    download_element.click();
                    download_element.setAttribute('href', undefined);
                });
            }));
        });

        const netLogContainer = this.element.createChild('div', 'netlog-container');
        netLogContainer.appendChild(UI.createTextButton(Common.UIString('Start NetLog'), event => {
            console.log("Start NetLog");
            this.run(`(function() { window.h5vcc.netLog.start();})()`);
            console.log("Started NetLog");
        }));
        netLogContainer.appendChild(UI.createTextButton(Common.UIString('Stop NetLog'), event => {
            console.log("Stop NetLog");
            this.run(`(function() { window.h5vcc.netLog.stop();})()`);
            console.log("Stopped NetLog");
        }));
        netLogContainer.appendChild(UI.createTextButton(Common.UIString('Download NetLog'), event => {
            console.log("Download Trace");
            this.run(`(function() { return window.h5vcc.netLog.stopAndRead();})()`).then(function (result) {
                const netlog_file = 'net_log.json';
                download_element.setAttribute('href', 'data:text/plain;charset=utf-8,' +
                    encodeURIComponent(result.result.value));
                download_element.setAttribute('download', netlog_file);
                console.log("Downloaded NetLog");
                download_element.click();
                download_element.setAttribute('href', undefined);
            });
        }));


        const debugLogContainer = this.element.createChild('div', 'debug-log-container');
        debugLogContainer.appendChild(UI.createTextButton(Common.UIString('DebugLog On'), event => {
            this._cobaltAgent.invoke_sendConsoleCommand({
                command: 'debug_log', message: 'on'
            });
        }));
        debugLogContainer.appendChild(UI.createTextButton(Common.UIString('DebugLog Off'), event => {
            this._cobaltAgent.invoke_sendConsoleCommand({
                command: 'debug_log', message: 'off'
            });
        }));

        const lifecycleContainer = this.element.createChild('div', 'lifecycle-container');
        lifecycleContainer.appendChild(UI.createTextButton(Common.UIString('Blur'), event => {
            this._cobaltAgent.invoke_sendConsoleCommand({ command: 'blur' });
        }));
        lifecycleContainer.appendChild(UI.createTextButton(Common.UIString('Focus'), event => {
            this._cobaltAgent.invoke_sendConsoleCommand({ command: 'focus' });
        }));
        lifecycleContainer.appendChild(UI.createTextButton(Common.UIString('Conceal'), event => {
            this._cobaltAgent.invoke_sendConsoleCommand({ command: 'conceal' });
        }));
        lifecycleContainer.appendChild(UI.createTextButton(Common.UIString('Freeze'), event => {
            this._cobaltAgent.invoke_sendConsoleCommand({ command: 'freeze' });
        }));
        lifecycleContainer.appendChild(UI.createTextButton(Common.UIString('Reveal'), event => {
            this._cobaltAgent.invoke_sendConsoleCommand({ command: 'reveal' });
        }));
        lifecycleContainer.appendChild(UI.createTextButton(Common.UIString('Quit'), event => {
            this._cobaltAgent.invoke_sendConsoleCommand({ command: 'quit' });
        }));

        const consoleContainer = this.element.createChild('div', 'console-container');
        consoleContainer.appendChild(UI.createLabel('Debug Command:'));
        var commandInput = consoleContainer.appendChild(UI.createInput('debug-command', 'text'));
        commandInput.addEventListener("keypress", event => {
            if (event.key === "Enter") {
                event.preventDefault();
                const consoleResponse = document.getElementsByClassName('console-response')[0];
                const command = document.getElementsByClassName('debug-command')[0].value;
                if (command.length == 0) {
                    const result = this._cobaltAgent.invoke_getConsoleCommands().then(result => {
                        consoleResponse.innerHTML = JSON.stringify(result.commands, undefined, 2);
                    });
                } else {
                    const result = this._cobaltAgent.invoke_sendConsoleCommand({
                        command: command,
                        message: document.getElementsByClassName('debug-message')[0].value
                    }).then(result => {
                        consoleResponse.innerHTML = result;
                    });
                }
            }
        });
        consoleContainer.appendChild(UI.createLabel('Message:'));
        consoleContainer.appendChild(UI.createInput('debug-message', 'text'));
        consoleContainer.appendChild(UI.createTextButton(Common.UIString('Download Response'), event => {
            const command = document.getElementsByClassName('debug-command')[0].value;
            const filename = "commmand_" + command + '.txt';
            download_element.setAttribute('href', 'data:text/plain;charset=utf-8,' +
                encodeURIComponent(document.getElementsByClassName("console-response")[0].value));
            download_element.setAttribute('download', filename);
            download_element.click();
            download_element.setAttribute('href', undefined);
        }));
        const textAreaContainer = consoleContainer.createChild('div');
        textAreaContainer.setAttribute("style", "display: flex; flex-direction:column;");
        textAreaContainer.setAttribute("width", "100%");
        textAreaContainer.setAttribute("height", "100%");
        const consoleResponse = textAreaContainer.createChild('textarea', 'console-response');
        consoleResponse.setAttribute("style", "margin: 0;");
        consoleResponse.setAttribute("rows", "25");
        consoleResponse.setAttribute("wrap", "off");
    }

    async run(expression) {
        return await this._runtimeAgent.invoke_evaluate({ expression, returnByValue: true });
    }


    /**
     * @override
     */
    focus() {
    }

    /**
     * @override
     */
    wasShown() {
        super.wasShown();
        if (this._model) {
            this._model.enable();
        }
    }

    /**
     * @override
     */
    willHide() {
        if (this._model) {
            this._model.disable();
        }
        super.willHide();
    }

    /**
     * @override
     * @param {!SDK.Target} target
     */
    targetAdded(target) {
        if (this._model) {
            return;
        }
        if (!this._model) {
            return;
        }
    }

    /**
     * @override
     * @param {!SDK.Target} target
     */
    targetRemoved(target) {
        if (!this._model || this._model.target() !== target) {
            return;
        }
    }
}

/* Legacy exported object */
self.Cobalt = self.Cobalt || {};

/**
 * @constructor
 */
self.Cobalt.CobaltPanel = CobaltPanel;
