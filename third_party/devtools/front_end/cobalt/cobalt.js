
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
        consoleContainer.appendChild(UI.createTextButton(Common.UIString('DebugCommand'), event => {
            const outputElement = document.getElementsByClassName('console-output')[0];
            const command = document.getElementsByClassName('debug-command')[0].value;
            if (command.length == 0) {
                const result = this._cobaltAgent.invoke_getConsoleCommands().then(result => {
                    outputElement.innerHTML = JSON.stringify(result.commands, undefined, 2);
                });
            } else {
                const result = this._cobaltAgent.invoke_sendConsoleCommand({
                    command: command,
                    message: document.getElementsByClassName('debug-message')[0].value
                }).then(result => {
                    outputElement.innerHTML = JSON.stringify(result, undefined, 2);
                });
            }
        }));
        consoleContainer.appendChild(UI.createLabel('Command:'));
        consoleContainer.appendChild(UI.createInput('debug-command', 'text'));
        consoleContainer.appendChild(UI.createLabel('Message:'));
        consoleContainer.appendChild(UI.createInput('debug-message', 'text'));
        consoleContainer.createChild('pre', 'console-output');
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
